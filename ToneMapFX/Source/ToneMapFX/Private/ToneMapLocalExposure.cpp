// Copyright Epic Games, Inc. All Rights Reserved.
// Local-exposure pass setup adapted from Renderer/PostProcess for a plugin-owned
// HDR pre-pass. Uses Unreal's existing shader sources and Gaussian filter.
// The Renderer entry points (except AddGaussianBlurPass) are not exported.

#include "ToneMapLocalExposure.h"
#include "PostProcess/PostProcessLocalExposure.h"
#include "PostProcess/PostProcessEyeAdaptation.h"
#include "PostProcess/PostProcessWeightedSampleSum.h"
#include "Curves/CurveFloat.h"
#include "SceneRendering.h"
#include "ShaderCompilerCore.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "ColorManagement/ColorSpace.h"
#include "SystemTextures.h"
#include "RenderGraphUtils.h"

namespace ToneMapFXLocalExposurePrivate
{
float ReadConsoleFloat(const TCHAR* Name, float Default)
{
	const IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(Name);
	return Variable ? Variable->GetFloat() : Default;
}
int32 ReadConsoleInt(const TCHAR* Name, int32 Default)
{
	const IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(Name);
	return Variable ? Variable->GetInt() : Default;
}
float LuminanceMaxFromLensAttenuation()
{
	return ReadConsoleInt(TEXT("r.DefaultFeature.AutoExposure.ExtendDefaultLuminanceRange"), 0)
		? 0.78f / FMath::Max(ReadConsoleFloat(TEXT("r.EyeAdaptation.LensAttenuation"), 0.78f), 0.01f) : 1.0f;
}
EAutoExposureMethod ResolveAutoExposureMethod(const FViewInfo& View)
{
	if (!View.Family->EngineShowFlags.EyeAdaptation || ReadConsoleInt(TEXT("r.EyeAdaptationQuality"), 2) == 0)
		return AEM_Manual;
	const int32 Override = ReadConsoleInt(TEXT("r.EyeAdaptation.MethodOverride"), -1);
	if (Override == 1) return AEM_Histogram;
	if (Override == 2) return AEM_Basic;
	if (Override == 3) return AEM_Manual;
	return View.FinalPostProcessSettings.AutoExposureMethod;
}

// The engine's parameter builder is private to Renderer. Local-exposure shaders
// only consume luminance and histogram fields, plus manual middle-grey bias.
FEyeAdaptationParameters BuildEyeAdaptationParameters(const FViewInfo& View)
{
	FEyeAdaptationParameters P{};
	const FPostProcessSettings& Settings = View.FinalPostProcessSettings;
	const bool bExtended = ReadConsoleInt(TEXT("r.DefaultFeature.AutoExposure.ExtendDefaultLuminanceRange"), 0) != 0;
	const float LuminanceMax = LuminanceMaxFromLensAttenuation();
	const float LogMin = bExtended ? EV100ToLog2(LuminanceMax, Settings.HistogramLogMin) : Settings.HistogramLogMin;
	const float LogMax = bExtended ? EV100ToLog2(LuminanceMax, Settings.HistogramLogMax) : Settings.HistogramLogMax;
	P.Scalars.HistogramScale = 1.0f / FMath::Max(LogMax - LogMin, 0.001f);
	P.Scalars.HistogramBias = -LogMin * P.Scalars.HistogramScale;
	P.Scalars.LuminanceMin = ResolveAutoExposureMethod(View) == AEM_Basic ? 0.0001f : FMath::Exp2(LogMin);
	const int32 Method = ReadConsoleInt(TEXT("r.AutoExposure.LuminanceMethod"), 0);
	P.Scalars.LuminanceWeights = Method == 1 ? FVector3f(0.3f, 0.59f, 0.11f) :
		(Method == 2 ? FVector3f(UE::Color::FColorSpace::GetWorking().GetLuminanceFactors()) : FVector3f(1.0f / 3.0f));
	P.Scalars.ExposureCompensationSettings = FMath::Exp2(Settings.AutoExposureBias);
	P.Scalars.ExposureCompensationCurve = 1.0f;
	const float AverageLum = static_cast<const FSceneView&>(View).GetLastAverageSceneLuminance();
	if (Settings.AutoExposureBiasCurve && AverageLum > 0.0f)
	{
		const float EV100 = LuminanceToEV100(LuminanceMax, AverageLum) + FMath::Log2(1.0f / 0.18f);
		P.Scalars.ExposureCompensationCurve = FMath::Exp2(Settings.AutoExposureBiasCurve->GetFloatValue(EV100));
	}
	if (!View.Family->EngineShowFlags.EyeAdaptation || View.Family->ExposureSettings.bFixed)
	{
		P.Scalars.ExposureCompensationSettings = 1.0f;
		P.Scalars.ExposureCompensationCurve = 1.0f;
	}
	return P;
}
namespace
{

class FToneMapLocalExposureLogLumCS : public FGlobalShader
{
public:
	// Changing these numbers requires LocalExposure.usf to be recompiled
	static const uint32 ThreadGroupSizeX = 8;
	static const uint32 ThreadGroupSizeY = 8;

	DECLARE_GLOBAL_SHADER(FToneMapLocalExposureLogLumCS);
	SHADER_USE_PARAMETER_STRUCT(FToneMapLocalExposureLogLumCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT(FEyeAdaptationParameters, EyeAdaptation)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, Input)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, InputTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, OutputFloat)
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), ThreadGroupSizeX);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), ThreadGroupSizeY);
	}
};

IMPLEMENT_GLOBAL_SHADER(FToneMapLocalExposureLogLumCS, "/Engine/Private/PostProcessLocalExposure.usf", "SetupLogLuminanceCS", SF_Compute);

class FToneMapLocalExposureApplyCS : public FGlobalShader
{
public:
	// Changing these numbers requires LocalExposure.usf to be recompiled
	static const uint32 ThreadGroupSizeX = 8;
	static const uint32 ThreadGroupSizeY = 8;

	DECLARE_GLOBAL_SHADER(FToneMapLocalExposureApplyCS);
	SHADER_USE_PARAMETER_STRUCT(FToneMapLocalExposureApplyCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)

		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, Input)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, Output)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, InputTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutputFloat4)

		SHADER_PARAMETER_STRUCT(FEyeAdaptationParameters, EyeAdaptation)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, EyeAdaptationBuffer)

		SHADER_PARAMETER_STRUCT(FLocalExposureParameters, LocalExposure)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D, LumBilateralGrid)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, BlurredLogLum)

		SHADER_PARAMETER_SAMPLER(SamplerState, TextureSampler)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), ThreadGroupSizeX);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), ThreadGroupSizeY);
	}
};

IMPLEMENT_GLOBAL_SHADER(FToneMapLocalExposureApplyCS, "/Engine/Private/PostProcessLocalExposure.usf", "ApplyLocalExposureCS", SF_Compute);

class FToneMapLocalExposureFusionSetupCS : public FGlobalShader
{
public:
	// Changing these numbers requires LocalExposure.usf to be recompiled
	static const uint32 ThreadGroupSizeX = 8;
	static const uint32 ThreadGroupSizeY = 8;

	DECLARE_GLOBAL_SHADER(FToneMapLocalExposureFusionSetupCS);
	SHADER_USE_PARAMETER_STRUCT(FToneMapLocalExposureFusionSetupCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)

		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, Input)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, InputTexture)

		SHADER_PARAMETER_STRUCT(FEyeAdaptationParameters, EyeAdaptation)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, EyeAdaptationBuffer)

		SHADER_PARAMETER_STRUCT(FLocalExposureParameters, LocalExposure)

		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, Output)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutputFloat4)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutputFloat4_1)

		SHADER_PARAMETER(float, TargetLuminance)

		SHADER_PARAMETER(float, FilmSlope)
		SHADER_PARAMETER(float, FilmToe)
		SHADER_PARAMETER(float, FilmShoulder)
		SHADER_PARAMETER(float, FilmBlackClip)
		SHADER_PARAMETER(float, FilmWhiteClip)
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), ThreadGroupSizeX);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), ThreadGroupSizeY);
	}
};

IMPLEMENT_GLOBAL_SHADER(FToneMapLocalExposureFusionSetupCS, "/Engine/Private/PostProcessLocalExposure.usf", "FusionSetupCS", SF_Compute);

class FToneMapLocalExposureFusionBlendCS : public FGlobalShader
{
public:
	// Changing these numbers requires LocalExposure.usf to be recompiled
	static const uint32 ThreadGroupSizeX = 8;
	static const uint32 ThreadGroupSizeY = 8;

	DECLARE_GLOBAL_SHADER(FToneMapLocalExposureFusionBlendCS);
	SHADER_USE_PARAMETER_STRUCT(FToneMapLocalExposureFusionBlendCS, FGlobalShader);

	class FLaplacianDim : SHADER_PERMUTATION_BOOL("LAPLACIAN");
	using FPermutationDomain = TShaderPermutationDomain<FLaplacianDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT(FEyeAdaptationParameters, EyeAdaptation)

		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, Input)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, InputTexture)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, WeightTexture)

		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, CoarserMip)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, CoarserMipTexture)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, PrevResultTexture)

		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, Output)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, OutputFloat)

		SHADER_PARAMETER_SAMPLER(SamplerState, TextureSampler)

		SHADER_PARAMETER(FScreenTransform, DispatchThreadToCoarseMipUV)
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), ThreadGroupSizeX);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), ThreadGroupSizeY);
	}
};

IMPLEMENT_GLOBAL_SHADER(FToneMapLocalExposureFusionBlendCS, "/Engine/Private/PostProcessLocalExposure.usf", "FusionBlendCS", SF_Compute);

} //! namespace

class FToneMapLocalExposureGridCS : public FGlobalShader
{
public:
	// Changing these numbers requires Histogram.usf to be recompiled.
	static const uint32 LoopCountX = 8;
	static const uint32 LoopCountY = 8;

	// we store 4 buckets in one ARGB texel.
	static const uint32 HistogramBucketsPerTexel = 4;

	DECLARE_GLOBAL_SHADER(FToneMapLocalExposureGridCS);
	SHADER_USE_PARAMETER_STRUCT(FToneMapLocalExposureGridCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, Input)
		SHADER_PARAMETER_STRUCT(FEyeAdaptationParameters, EyeAdaptation)
		SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, InputTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, HistogramRWTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float2>, BilateralGridRWTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, DebugOutput)
		SHADER_PARAMETER(FIntPoint, ThreadGroupCount)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		const bool bBilateralGrid = true;
		OutEnvironment.SetDefine(TEXT("BILATERAL_GRID"), 1);
		OutEnvironment.SetDefine(TEXT("USE_PRECALCULATED_LUMINANCE"), 0);
		OutEnvironment.SetDefine(TEXT("USE_APPROX_ILLUMINANCE"), 0);
		OutEnvironment.SetDefine(TEXT("USE_DEBUG_OUTPUT"), 0);

		const FIntPoint ThreadGroupSize = GetThreadGroupSize(bBilateralGrid);
		const uint32 HistogramSize = GetHistogramSize(bBilateralGrid);

		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), ThreadGroupSize.X);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), ThreadGroupSize.Y);
		OutEnvironment.SetDefine(TEXT("LOOP_SIZEX"), LoopCountX);
		OutEnvironment.SetDefine(TEXT("LOOP_SIZEY"), LoopCountY);
		OutEnvironment.SetDefine(TEXT("HISTOGRAM_SIZE"), HistogramSize);

		OutEnvironment.CompilerFlags.Add( CFLAG_StandardOptimization );
	}

	static uint32 GetHistogramSize(bool bBilateralGrid)
	{
		return bBilateralGrid ? 32 : 64;
	}

	static FIntPoint GetThreadGroupSize(bool bBilateralGrid)
	{
		return bBilateralGrid ? FIntPoint(8, 8) : FIntPoint(8, 4);
	}

	static FIntPoint GetTexelsPerThreadGroup( bool bBilateralGrid)
	{
		// One ThreadGroup ThreadGroupSizeX*ThreadGroupSizeY processes blocks of size LoopCountX*LoopCountY
		return GetThreadGroupSize(bBilateralGrid) * FIntPoint(LoopCountX, LoopCountY);
	}

	static FIntPoint GetThreadGroupCount(FIntPoint InputExtent, bool bBilateralGrid)
	{
		const FIntPoint TexelsPerThreadGroup = GetTexelsPerThreadGroup(bBilateralGrid);

		return FIntPoint::DivideAndRoundUp(InputExtent, TexelsPerThreadGroup);
	}
};

IMPLEMENT_GLOBAL_SHADER(FToneMapLocalExposureGridCS, "/Engine/Private/PostProcessHistogram.usf", "MainCS", SF_Compute);

class FToneMapLocalExposureDownsampleCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FToneMapLocalExposureDownsampleCS);
	SHADER_USE_PARAMETER_STRUCT(FToneMapLocalExposureDownsampleCS, FGlobalShader);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, Input)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, Output)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, InputTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
		SHADER_PARAMETER(FScreenTransform, DispatchThreadIdToInputUV)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutComputeTexture)
	END_SHADER_PARAMETER_STRUCT()
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), 8);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), 8);
		OutEnvironment.SetDefine(TEXT("DOWNSAMPLE_QUALITY"), 1);
	}
};
IMPLEMENT_GLOBAL_SHADER(FToneMapLocalExposureDownsampleCS, "/Engine/Private/PostProcessDownsample.usf", "MainCS", SF_Compute);

FScreenPassTexture Downsample(FRDGBuilder& GraphBuilder, const FViewInfo& View, FScreenPassTextureSlice Input)
{
	const FScreenPassTextureViewport Viewport = GetDownscaledViewport(FScreenPassTextureViewport(Input), 2);
	const FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(Viewport.Extent, Input.TextureSRV->GetParent()->Desc.Format,
		FClearValueBinding::None, TexCreate_ShaderResource | TexCreate_UAV);
	FScreenPassTexture Output(GraphBuilder.CreateTexture(Desc, TEXT("ToneMap.LocalExposure.Downsample")), Viewport.Rect);
	auto* P = GraphBuilder.AllocParameters<FToneMapLocalExposureDownsampleCS::FParameters>();
	P->ViewUniformBuffer = View.ViewUniformBuffer;
	P->Input = GetScreenPassTextureViewportParameters(FScreenPassTextureViewport(Input));
	P->Output = GetScreenPassTextureViewportParameters(FScreenPassTextureViewport(Output));
	P->InputTexture = Input.TextureSRV;
	P->InputSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	P->DispatchThreadIdToInputUV = ((FScreenTransform::Identity + 0.5f) / Output.ViewRect.Size()) *
		FScreenTransform::ChangeTextureBasisFromTo(FScreenPassTextureViewport(Input), FScreenTransform::ETextureBasis::ViewportUV, FScreenTransform::ETextureBasis::TextureUV);
	P->OutComputeTexture = GraphBuilder.CreateUAV(Output.Texture);
	FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("ToneMap.LocalExposure.Downsample"),
		View.ShaderMap->GetShader<FToneMapLocalExposureDownsampleCS>(), P,
		FComputeShaderUtils::GetGroupCount(Output.ViewRect.Size(), FIntPoint(8, 8)));
	return Output;
}

TArray<FScreenPassTextureSlice> BuildDownsampleChain(FRDGBuilder& GraphBuilder, const FViewInfo& View, FScreenPassTexture Input, uint32 NumMips)
{
	TArray<FScreenPassTextureSlice> Chain;
	Chain.Add(FScreenPassTextureSlice::CreateFromScreenPassTexture(GraphBuilder, Input));
	for (uint32 Index = 1; Index < NumMips; ++Index)
	{
		Chain.Add(FScreenPassTextureSlice::CreateFromScreenPassTexture(GraphBuilder, Downsample(GraphBuilder, View, Chain.Last())));
	}
	return Chain;
}

FVector2f GetLocalExposureBilateralGridUVScale(const FIntPoint ViewRectSize)
{
	const FIntPoint TexelsPerThreadGroup = FToneMapLocalExposureGridCS::GetTexelsPerThreadGroup(true);

	const FIntPoint ThreadGroupCount = FToneMapLocalExposureGridCS::GetThreadGroupCount(ViewRectSize, true);

	return FVector2f(float(ViewRectSize.X) / TexelsPerThreadGroup.X / ThreadGroupCount.X, float(ViewRectSize.Y) / TexelsPerThreadGroup.Y / ThreadGroupCount.Y);
}

FRDGTextureRef AddBilateralGridPass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FEyeAdaptationParameters& EyeAdaptationParameters,
	FScreenPassTextureSlice SceneColor)
{
	check(SceneColor.IsValid());

	// Bilateral grid requires PF_G32R32F UAV. If the format doesn't support it, skip.
	if (!UE::PixelFormat::HasCapabilities(PF_G32R32F, EPixelFormatCapabilities::UAV))
	{
		return nullptr;
	}

	const FIntPoint ThreadGroupSize = FToneMapLocalExposureGridCS::GetThreadGroupSize(true);
	const FIntPoint ThreadGroupCount = FToneMapLocalExposureGridCS::GetThreadGroupCount(SceneColor.ViewRect.Size(), true);
	const FIntPoint NumTiles = ThreadGroupCount;

	FRDGTextureRef LocalExposureTexture = nullptr;

	RDG_EVENT_SCOPE(GraphBuilder, "LocalExposure");

	{
		const FIntVector TextureExtent = FIntVector(NumTiles.X, NumTiles.Y, FToneMapLocalExposureGridCS::GetHistogramSize(true));

		const FRDGTextureDesc TextureDesc = FRDGTextureDesc::Create3D(
			TextureExtent,
			PF_G32R32F,
			FClearValueBinding::None,
			TexCreate_UAV | TexCreate_ShaderResource);

		LocalExposureTexture = GraphBuilder.CreateTexture(TextureDesc, TEXT("LocalExposure"));

		auto* PassParameters = GraphBuilder.AllocParameters<FToneMapLocalExposureGridCS::FParameters>();
		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->EyeAdaptation = EyeAdaptationParameters;
		PassParameters->Input = GetScreenPassTextureViewportParameters(FScreenPassTextureViewport(SceneColor));
		PassParameters->InputSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		PassParameters->InputTexture = SceneColor.TextureSRV;
		PassParameters->BilateralGridRWTexture = GraphBuilder.CreateUAV(LocalExposureTexture);
		PassParameters->ThreadGroupCount = ThreadGroupCount;

		auto ComputeShader = View.ShaderMap->GetShader<FToneMapLocalExposureGridCS>();

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("FLocalExposure %dx%d (CS)", SceneColor.ViewRect.Width(), SceneColor.ViewRect.Height()),
			ERDGPassFlags::Compute,
			ComputeShader,
			PassParameters,
			FIntVector(ThreadGroupCount.X, ThreadGroupCount.Y, 1));
	}

	return LocalExposureTexture;
}


FVector2f GetLocalExposureBilateralGridUVScale(const FIntPoint ViewRectSize);

FLocalExposureParameters BuildLocalExposureParameters(const FViewInfo& View, FIntPoint ViewRectSize, const FEyeAdaptationParameters& EyeAdaptationParameters)
{
	const FPostProcessSettings& Settings = View.FinalPostProcessSettings;

	const EAutoExposureMethod AutoExposureMethod = ResolveAutoExposureMethod(View);

	float LocalExposureMiddleGreyExposureCompensation = FMath::Pow(2.0f, View.FinalPostProcessSettings.LocalExposureMiddleGreyBias);

	if (AutoExposureMethod == EAutoExposureMethod::AEM_Manual)
	{
		// when using manual exposure cancel exposure compensation setting and curve from middle grey used by local exposure.
		LocalExposureMiddleGreyExposureCompensation /= (EyeAdaptationParameters.Scalars.ExposureCompensationSettings * EyeAdaptationParameters.Scalars.ExposureCompensationCurve);
	}

	const FVector2f LocalExposureBilateralGridUVScale = GetLocalExposureBilateralGridUVScale(ViewRectSize);

	float HighlightContrast = Settings.LocalExposureHighlightContrastScale;
	float ShadowContrast = Settings.LocalExposureShadowContrastScale;

	const float AverageSceneLuminance = static_cast<const FSceneView&>(View).GetLastAverageSceneLuminance();
	if (AverageSceneLuminance > 0)
	{
		const float LuminanceMax = LuminanceMaxFromLensAttenuation();
		// We need the Log2(0.18) to convert from average luminance to saturation luminance
		const float LuminanceEV100 = LuminanceToEV100(LuminanceMax, AverageSceneLuminance) + FMath::Log2(1.0f / 0.18f);

		if (Settings.LocalExposureHighlightContrastCurve)
		{
			HighlightContrast *= Settings.LocalExposureHighlightContrastCurve->GetFloatValue(LuminanceEV100);
		}

		if (Settings.LocalExposureShadowContrastCurve)
		{
			ShadowContrast *= Settings.LocalExposureShadowContrastCurve->GetFloatValue(LuminanceEV100);
		}
	}

	if (View.FinalPostProcessSettings.LocalExposureMethod == ELocalExposureMethod::Fusion)
	{
		const float HighlightEV = FMath::Lerp<float>(6, 0, HighlightContrast);
		HighlightContrast = FMath::Pow(2, -HighlightEV);

		const float ShadowEV = FMath::Lerp<float>(6, 0, ShadowContrast);
		ShadowContrast = FMath::Pow(2, ShadowEV);
	}

	FLocalExposureParameters Parameters;
	Parameters.HighlightContrastScale = HighlightContrast;
	Parameters.ShadowContrastScale = ShadowContrast;
	Parameters.DetailStrength = Settings.LocalExposureDetailStrength;
	Parameters.BlurredLuminanceBlend = Settings.LocalExposureBlurredLuminanceBlend;
	Parameters.MiddleGreyExposureCompensation = LocalExposureMiddleGreyExposureCompensation;
	Parameters.BilateralGridUVScale = LocalExposureBilateralGridUVScale;
	Parameters.HighlightThreshold = Settings.LocalExposureHighlightThreshold;
	Parameters.ShadowThreshold = Settings.LocalExposureShadowThreshold;
	Parameters.HighlightThresholdStrength = Settings.LocalExposureHighlightThresholdStrength;
	Parameters.ShadowThresholdStrength = Settings.LocalExposureShadowThresholdStrength;
	return Parameters;
}

FRDGTextureRef AddBlurredLogLuminancePass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FEyeAdaptationParameters& EyeAdaptationParameters,
	FScreenPassTextureSlice InputTexture)
{
	check(InputTexture.IsValid());

	RDG_EVENT_SCOPE(GraphBuilder, "LocalExposure - Blurred Luminance");

	FRDGTextureRef GaussianLumSetupTexture;

	// Copy log luminance to temporary texture
	{
		const FRDGTextureDesc TextureDesc = FRDGTextureDesc::Create2D(
			InputTexture.ViewRect.Size(),
			PF_R16F,
			FClearValueBinding::None,
			TexCreate_UAV | TexCreate_ShaderResource);

		GaussianLumSetupTexture = GraphBuilder.CreateTexture(TextureDesc, TEXT("GaussianLumSetupTexture"));

		auto* PassParameters = GraphBuilder.AllocParameters<FToneMapLocalExposureLogLumCS::FParameters>();
		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->EyeAdaptation = EyeAdaptationParameters;
		PassParameters->Input = GetScreenPassTextureViewportParameters(FScreenPassTextureViewport(InputTexture));
		PassParameters->InputTexture = InputTexture.TextureSRV;
		PassParameters->OutputFloat = GraphBuilder.CreateUAV(GaussianLumSetupTexture);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("SetupLogLuminance %dx%d", GaussianLumSetupTexture->Desc.Extent.X, GaussianLumSetupTexture->Desc.Extent.Y),
			ERDGPassFlags::Compute,
			View.ShaderMap->GetShader<FToneMapLocalExposureLogLumCS>(),
			PassParameters,
			FComputeShaderUtils::GetGroupCount(GaussianLumSetupTexture->Desc.Extent, FIntPoint(FToneMapLocalExposureLogLumCS::ThreadGroupSizeX, FToneMapLocalExposureLogLumCS::ThreadGroupSizeY)));
	}

	FRDGTextureRef GaussianTexture;

	{
		FGaussianBlurInputs GaussianBlurInputs;
		GaussianBlurInputs.NameX = TEXT("LocalExposureGaussianX");
		GaussianBlurInputs.NameY = TEXT("LocalExposureGaussianY");
		GaussianBlurInputs.Filter = FScreenPassTextureSlice::CreateFromScreenPassTexture(GraphBuilder, FScreenPassTexture(GaussianLumSetupTexture));
		GaussianBlurInputs.TintColor = FLinearColor::White;
		GaussianBlurInputs.CrossCenterWeight = FVector2f::ZeroVector;
		GaussianBlurInputs.KernelSizePercent = View.FinalPostProcessSettings.LocalExposureBlurredLuminanceKernelSizePercent;
		GaussianBlurInputs.UseMirrorAddressMode = true;

		GaussianTexture = AddGaussianBlurPass(GraphBuilder, View, GaussianBlurInputs).Texture;
	}

	return GaussianTexture;
}

void AddBilateralApplyPass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FEyeAdaptationParameters& EyeAdaptationParameters,
	FRDGBufferRef EyeAdaptationBuffer,
	const FLocalExposureParameters& LocalExposureParamaters,
	FRDGTextureRef LocalExposureTexture,
	FRDGTextureRef BlurredLogLuminanceTexture,
	FScreenPassTextureSlice Input,
	FScreenPassTextureSlice Output,
	ERDGPassFlags PassFlags)
{
	check(Input.IsValid() && Output.IsValid());
	check(PassFlags == ERDGPassFlags::Compute || PassFlags == ERDGPassFlags::AsyncCompute);

	RDG_EVENT_SCOPE(GraphBuilder, "LocalExposure - Apply");

	auto* PassParameters = GraphBuilder.AllocParameters<FToneMapLocalExposureApplyCS::FParameters>();
	PassParameters->View = View.ViewUniformBuffer;
	PassParameters->Input = GetScreenPassTextureViewportParameters(FScreenPassTextureViewport(Input));
	PassParameters->Output = GetScreenPassTextureViewportParameters(FScreenPassTextureViewport(Output));

	PassParameters->InputTexture = Input.TextureSRV;
	{
		FRDGTextureUAVDesc OutputDesc(Output.TextureSRV->Desc.Texture);
		if (Output.TextureSRV->Desc.Texture->Desc.IsTextureArray())
		{
			OutputDesc.DimensionOverride = ETextureDimension::Texture2D;
			OutputDesc.FirstArraySlice = Output.TextureSRV->Desc.FirstArraySlice;
			OutputDesc.NumArraySlices = 1;
		}

		PassParameters->OutputFloat4 = GraphBuilder.CreateUAV(OutputDesc);
	}

	PassParameters->EyeAdaptation = EyeAdaptationParameters;
	PassParameters->EyeAdaptationBuffer = GraphBuilder.CreateSRV(EyeAdaptationBuffer);

	PassParameters->LocalExposure = LocalExposureParamaters;
	PassParameters->LumBilateralGrid = LocalExposureTexture;
	PassParameters->BlurredLogLum = BlurredLogLuminanceTexture;

	PassParameters->TextureSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("ApplyLocalExposure %dx%d", Output.ViewRect.Width(), Output.ViewRect.Height()),
		PassFlags,
		View.ShaderMap->GetShader<FToneMapLocalExposureApplyCS>(),
		PassParameters,
		FComputeShaderUtils::GetGroupCount(Output.ViewRect.Size(), FIntPoint(FToneMapLocalExposureApplyCS::ThreadGroupSizeX, FToneMapLocalExposureApplyCS::ThreadGroupSizeY)));
}

FExposureFusionData AddFusionPass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FEyeAdaptationParameters& EyeAdaptationParameters,
	FRDGBufferRef EyeAdaptationBuffer,
	const FLocalExposureParameters& LocalExposureParamaters,
	FScreenPassTextureSlice Input)
{
	check(Input.IsValid());

	RDG_EVENT_SCOPE(GraphBuilder, "LocalExposure - Fusion");

	FScreenPassTexture LumTexture;
	FScreenPassTexture WeightTexture;

	{
		const FRDGTextureDesc& InputDesc = Input.TextureSRV->GetParent()->Desc;

		const FRDGTextureDesc TextureDesc = FRDGTextureDesc::Create2D(
			InputDesc.Extent,
			PF_FloatRGB,
			FClearValueBinding::None,
			TexCreate_UAV | TexCreate_ShaderResource);

		// output uses same viewport as input
		LumTexture = FScreenPassTexture(GraphBuilder.CreateTexture(TextureDesc, TEXT("LocalExposureLumTexture")), Input.ViewRect);
		WeightTexture = FScreenPassTexture(GraphBuilder.CreateTexture(TextureDesc, TEXT("LocalExposureWeightTexture")), Input.ViewRect);

		auto* PassParameters = GraphBuilder.AllocParameters<FToneMapLocalExposureFusionSetupCS::FParameters>();
		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->EyeAdaptation = EyeAdaptationParameters;
		PassParameters->EyeAdaptationBuffer = GraphBuilder.CreateSRV(EyeAdaptationBuffer);
		PassParameters->LocalExposure = LocalExposureParamaters;
		PassParameters->Input = GetScreenPassTextureViewportParameters(FScreenPassTextureViewport(Input));
		PassParameters->InputTexture = Input.TextureSRV;
		PassParameters->Output = GetScreenPassTextureViewportParameters(FScreenPassTextureViewport(LumTexture));
		PassParameters->OutputFloat4 = GraphBuilder.CreateUAV(LumTexture.Texture);
		PassParameters->OutputFloat4_1 = GraphBuilder.CreateUAV(WeightTexture.Texture);
		PassParameters->TargetLuminance = ReadConsoleFloat(TEXT("r.LocalExposure.ExposureFusion.TargetLuminance"), 0.5f);

		const FPostProcessSettings& Settings = View.FinalPostProcessSettings;

		PassParameters->FilmSlope = Settings.FilmSlope;
		PassParameters->FilmToe = Settings.FilmToe;
		PassParameters->FilmShoulder = Settings.FilmShoulder;
		PassParameters->FilmBlackClip = Settings.FilmBlackClip;
		PassParameters->FilmWhiteClip = Settings.FilmWhiteClip;

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("FusionSetup %dx%d", Input.ViewRect.Width(), Input.ViewRect.Height()),
			ERDGPassFlags::Compute,
			View.ShaderMap->GetShader<FToneMapLocalExposureFusionSetupCS>(),
			PassParameters,
			FComputeShaderUtils::GetGroupCount(Input.ViewRect.Size(), FIntPoint(FToneMapLocalExposureFusionSetupCS::ThreadGroupSizeX, FToneMapLocalExposureFusionSetupCS::ThreadGroupSizeY)));
	}

	const uint32 MaxMips = FMath::Log2(float(FMath::Min(LumTexture.Texture->Desc.Extent.X, LumTexture.Texture->Desc.Extent.Y))) + 1;
	const uint32 NumMips = FMath::Clamp((uint32)FMath::Max(ReadConsoleInt(TEXT("r.LocalExposure.ExposureFusion.NumLevels"), 16), 1), 1, MaxMips);

	const TArray<FScreenPassTextureSlice> LumChain = BuildDownsampleChain(GraphBuilder, View, LumTexture, NumMips);
	const TArray<FScreenPassTextureSlice> WeightChain = BuildDownsampleChain(GraphBuilder, View, WeightTexture, NumMips);

	FScreenPassTexture Output;

	FScreenPassTextureSlice CoarserMip;

	for(int32 Index = NumMips - 1; Index >= 0; --Index)
	{
		FScreenPassTextureSlice CurrentLum = LumChain[Index];
		FScreenPassTextureSlice CurrentWeight = WeightChain[Index];

		FRDGTextureSRVRef PrevResult = Output.IsValid() ? GraphBuilder.CreateSRV(Output.Texture) : nullptr;

		{
			FRDGTextureDesc OutputDesc = CurrentLum.TextureSRV->GetParent()->Desc;
			OutputDesc.Reset();
			OutputDesc.Flags |= TexCreate_UAV;

			// output uses same viewport as mip
			Output = FScreenPassTexture(GraphBuilder.CreateTexture(OutputDesc, TEXT("LocalExposureResult")), CurrentLum.ViewRect);
		}

		auto* PassParameters = GraphBuilder.AllocParameters<FToneMapLocalExposureFusionBlendCS::FParameters>();
		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->EyeAdaptation = EyeAdaptationParameters;
		PassParameters->InputTexture = CurrentLum.TextureSRV;
		PassParameters->WeightTexture = CurrentWeight.TextureSRV;
		if (CoarserMip.IsValid())
		{
			PassParameters->DispatchThreadToCoarseMipUV =
				((FScreenTransform::Identity + 0.5f) / Output.ViewRect.Size()) *
				FScreenTransform::ChangeTextureBasisFromTo(FScreenPassTextureViewport(CoarserMip), FScreenTransform::ETextureBasis::ViewportUV, FScreenTransform::ETextureBasis::TextureUV);
			PassParameters->CoarserMip = GetScreenPassTextureViewportParameters(FScreenPassTextureViewport(CoarserMip));
			PassParameters->CoarserMipTexture = CoarserMip.TextureSRV;
		}
		PassParameters->PrevResultTexture = PrevResult;
		PassParameters->Output = GetScreenPassTextureViewportParameters(FScreenPassTextureViewport(Output));
		PassParameters->OutputFloat = GraphBuilder.CreateUAV(Output.Texture);
		PassParameters->TextureSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

		FToneMapLocalExposureFusionBlendCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FToneMapLocalExposureFusionBlendCS::FLaplacianDim>(PrevResult != nullptr);

		auto ComputeShader = View.ShaderMap->GetShader<FToneMapLocalExposureFusionBlendCS>(PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("FusionBlend %dx%d", Output.ViewRect.Width(), Output.ViewRect.Height()),
			ERDGPassFlags::Compute,
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(Output.ViewRect.Size(), FIntPoint(FToneMapLocalExposureFusionBlendCS::ThreadGroupSizeX, FToneMapLocalExposureFusionBlendCS::ThreadGroupSizeY)));

		CoarserMip = CurrentLum;
	}

	FExposureFusionData OutputData;
	OutputData.Result = Output;
	OutputData.Exposures = LumTexture;
	OutputData.Weights = WeightTexture;

	return MoveTemp(OutputData);
}
class FToneMapLocalExposureSetupExposureCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FToneMapLocalExposureSetupExposureCS);
	SHADER_USE_PARAMETER_STRUCT(FToneMapLocalExposureSetupExposureCS, FGlobalShader);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, EngineEyeAdaptationBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float4>, EyeAdaptationOutput)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AdaptedLumTexture)
		SHADER_PARAMETER(float, GlobalExposure)
		SHADER_PARAMETER(uint32, AutoExposureMode)
		SHADER_PARAMETER(float, MinAutoExposure)
		SHADER_PARAMETER(float, MaxAutoExposure)
	END_SHADER_PARAMETER_STRUCT()
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};
IMPLEMENT_GLOBAL_SHADER(FToneMapLocalExposureSetupExposureCS, "/Plugin/ToneMapFX/Private/ToneMapLocalExposure.usf", "SetupExposureCS", SF_Compute);

class FToneMapLocalExposureApplyFusionCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FToneMapLocalExposureApplyFusionCS);
	SHADER_USE_PARAMETER_STRUCT(FToneMapLocalExposureApplyFusionCS, FGlobalShader);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, Input)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, Output)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, Fusion)
		SHADER_PARAMETER_STRUCT(FEyeAdaptationParameters, EyeAdaptation)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, EyeAdaptationBuffer)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, InputTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, FusionTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, TextureSampler)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutputFloat4)
		SHADER_PARAMETER(float, FilmSlope)
		SHADER_PARAMETER(float, FilmToe)
		SHADER_PARAMETER(float, FilmShoulder)
		SHADER_PARAMETER(float, FilmBlackClip)
		SHADER_PARAMETER(float, FilmWhiteClip)
	END_SHADER_PARAMETER_STRUCT()
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};
IMPLEMENT_GLOBAL_SHADER(FToneMapLocalExposureApplyFusionCS, "/Plugin/ToneMapFX/Private/ToneMapLocalExposure.usf", "ApplyFusionCS", SF_Compute);

} // namespace ToneMapFXLocalExposurePrivate

FRDGBufferRef AddToneMapLocalExposureBuffer(
	FRDGBuilder& GraphBuilder, const FViewInfo& View, FScreenPassTexture SceneColor,
	uint8 AutoExposureMode, FRDGTextureRef AdaptedLumTexture, float MinAutoExposure, float MaxAutoExposure)
{
	using namespace ToneMapFXLocalExposurePrivate;
	FRDGBufferRef EngineExposure = nullptr;
	if (AutoExposureMode == 1)
	{
		if (FRDGPooledBuffer* Buffer = View.GetEyeAdaptationBuffer())
			EngineExposure = GraphBuilder.RegisterExternalBuffer(Buffer);
	}
	if (!EngineExposure)
	{
		const FVector4f Neutral(1.0f, 0.0f, 0.0f, 1.0f);
		EngineExposure = CreateStructuredBuffer(GraphBuilder, TEXT("ToneMap.LocalExposure.NeutralExposure"), sizeof(FVector4f), 1,
			&Neutral, sizeof(Neutral));
	}
	FRDGBufferRef Exposure = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(FVector4f), 1), TEXT("ToneMap.LocalExposure.Exposure"));
	{
		auto* P = GraphBuilder.AllocParameters<FToneMapLocalExposureSetupExposureCS::FParameters>();
		P->EngineEyeAdaptationBuffer = GraphBuilder.CreateSRV(EngineExposure);
		P->EyeAdaptationOutput = GraphBuilder.CreateUAV(Exposure);
		P->AdaptedLumTexture = AdaptedLumTexture ? AdaptedLumTexture : SceneColor.Texture;
		P->GlobalExposure = FMath::Max(static_cast<const FSceneView&>(View).GetLastEyeAdaptationExposure(), 0.001f);
		P->AutoExposureMode = AutoExposureMode;
		P->MinAutoExposure = MinAutoExposure;
		P->MaxAutoExposure = MaxAutoExposure;
		FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("ToneMap.LocalExposure.SetupExposure"),
			View.ShaderMap->GetShader<FToneMapLocalExposureSetupExposureCS>(), P, FIntVector(1, 1, 1));
	}

	return Exposure;
}

FScreenPassTexture AddToneMapLocalExposurePass(
	FRDGBuilder& GraphBuilder, const FViewInfo& View, FScreenPassTexture SceneColor,
	uint8 AutoExposureMode, FRDGTextureRef AdaptedLumTexture, float MinAutoExposure, float MaxAutoExposure)
{
	using namespace ToneMapFXLocalExposurePrivate;
	const FPostProcessSettings& Settings = View.FinalPostProcessSettings;
	const bool bEnabled = !FMath::IsNearlyEqual(Settings.LocalExposureHighlightContrastScale, 1.0f) ||
		!FMath::IsNearlyEqual(Settings.LocalExposureShadowContrastScale, 1.0f) ||
		Settings.LocalExposureHighlightContrastCurve || Settings.LocalExposureShadowContrastCurve ||
		!FMath::IsNearlyEqual(Settings.LocalExposureDetailStrength, 1.0f);
	if (!bEnabled || !SceneColor.IsValid() || View.GetFeatureLevel() < ERHIFeatureLevel::SM5)
		return SceneColor;
	RDG_EVENT_SCOPE(GraphBuilder, "ToneMapFX LocalExposure");

	const FEyeAdaptationParameters EyeParameters = BuildEyeAdaptationParameters(View);
	FRDGBufferRef Exposure = AddToneMapLocalExposureBuffer(GraphBuilder, View, SceneColor,
		AutoExposureMode, AdaptedLumTexture, MinAutoExposure, MaxAutoExposure);

	// Compact the output viewport. Engine bilateral application expects the
	// output's texture UV to cover precisely this view, including split screen.
	const FRDGTextureDesc OutputDesc = FRDGTextureDesc::Create2D(SceneColor.ViewRect.Size(), PF_FloatRGBA,
		FClearValueBinding::None, TexCreate_ShaderResource | TexCreate_UAV);
	FScreenPassTexture Output(GraphBuilder.CreateTexture(OutputDesc, TEXT("ToneMap.LocalExposure.SceneColor")));
	const FScreenPassTextureSlice InputSlice = FScreenPassTextureSlice::CreateFromScreenPassTexture(GraphBuilder, SceneColor);
	// Match the engine's half-resolution local-exposure analysis.
	const FScreenPassTexture Analysis = Downsample(GraphBuilder, View, InputSlice);
	const FScreenPassTextureSlice AnalysisSlice = FScreenPassTextureSlice::CreateFromScreenPassTexture(GraphBuilder, Analysis);
	const FLocalExposureParameters LocalParameters = BuildLocalExposureParameters(View, Analysis.ViewRect.Size(), EyeParameters);
	if (Settings.LocalExposureMethod == ELocalExposureMethod::Fusion)
	{
		const FExposureFusionData Fusion = AddFusionPass(GraphBuilder, View, EyeParameters, Exposure, LocalParameters, AnalysisSlice);
		auto* P = GraphBuilder.AllocParameters<FToneMapLocalExposureApplyFusionCS::FParameters>();
		P->View = View.ViewUniformBuffer;
		P->Input = GetScreenPassTextureViewportParameters(FScreenPassTextureViewport(SceneColor));
		P->Output = GetScreenPassTextureViewportParameters(FScreenPassTextureViewport(Output));
		P->Fusion = GetScreenPassTextureViewportParameters(FScreenPassTextureViewport(Fusion.Result));
		P->EyeAdaptation = EyeParameters;
		P->EyeAdaptationBuffer = GraphBuilder.CreateSRV(Exposure);
		P->InputTexture = InputSlice.TextureSRV;
		P->FusionTexture = Fusion.Result.Texture;
		P->TextureSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		P->OutputFloat4 = GraphBuilder.CreateUAV(Output.Texture);
		P->FilmSlope = Settings.FilmSlope;
		P->FilmToe = Settings.FilmToe;
		P->FilmShoulder = Settings.FilmShoulder;
		P->FilmBlackClip = Settings.FilmBlackClip;
		P->FilmWhiteClip = Settings.FilmWhiteClip;
		FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("ToneMap.LocalExposure.ApplyFusion"),
			View.ShaderMap->GetShader<FToneMapLocalExposureApplyFusionCS>(), P,
			FComputeShaderUtils::GetGroupCount(Output.ViewRect.Size(), FIntPoint(8, 8)));
	}
	else
	{
		FRDGTextureRef Grid = AddBilateralGridPass(GraphBuilder, View, EyeParameters, AnalysisSlice);
		if (!Grid) return SceneColor;
		FRDGTextureRef Blurred = GSystemTextures.GetBlackDummy(GraphBuilder);
		if (Settings.LocalExposureBlurredLuminanceBlend > 0.0f)
			Blurred = AddBlurredLogLuminancePass(GraphBuilder, View, EyeParameters, AnalysisSlice);
		AddBilateralApplyPass(GraphBuilder, View, EyeParameters, Exposure, LocalParameters, Grid, Blurred,
			InputSlice, FScreenPassTextureSlice::CreateFromScreenPassTexture(GraphBuilder, Output), ERDGPassFlags::Compute);
	}
	return Output;
}
