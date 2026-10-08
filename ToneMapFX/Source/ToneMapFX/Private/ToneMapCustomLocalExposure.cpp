// Licensed under the zlib License. See LICENSE file in the project root.

#include "ToneMapCustomLocalExposure.h"
#include "ToneMapLocalExposure.h"
#include "ColorManagement/ColorSpace.h"
#include "GlobalShader.h"
#include "PixelShaderUtils.h"
#include "RenderGraphUtils.h"
#include "SceneRendering.h"
#include "ShaderParameterStruct.h"

class FToneMapCustomLocalExposureLogLumPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FToneMapCustomLocalExposureLogLumPS);
	SHADER_USE_PARAMETER_STRUCT(FToneMapCustomLocalExposureLogLumPS, FGlobalShader);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneColorTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SceneColorSampler)
		SHADER_PARAMETER(FScreenTransform, SvPositionToSceneColorUV)
		SHADER_PARAMETER(float, OneOverPreExposure)
		SHADER_PARAMETER(FVector3f, LuminanceWeights)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};
IMPLEMENT_GLOBAL_SHADER(FToneMapCustomLocalExposureLogLumPS, "/Plugin/ToneMapFX/Private/ToneMapCustomLocalExposure.usf", "LogLumPS", SF_Pixel);

class FToneMapCustomLocalExposureBilateralPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FToneMapCustomLocalExposureBilateralPS);
	SHADER_USE_PARAMETER_STRUCT(FToneMapCustomLocalExposureBilateralPS, FGlobalShader);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, LogLumTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, GuideTexture)
		SHADER_PARAMETER(FIntPoint, WorkSize)
		SHADER_PARAMETER(FIntPoint, BlurDirection)
		SHADER_PARAMETER(float, SpatialSigma)
		SHADER_PARAMETER(float, RangeSigmaEV)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};
IMPLEMENT_GLOBAL_SHADER(FToneMapCustomLocalExposureBilateralPS, "/Plugin/ToneMapFX/Private/ToneMapCustomLocalExposure.usf", "BilateralPS", SF_Pixel);

class FToneMapCustomLocalExposureApplyPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FToneMapCustomLocalExposureApplyPS);
	SHADER_USE_PARAMETER_STRUCT(FToneMapCustomLocalExposureApplyPS, FGlobalShader);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneColorTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SceneColorSampler)
		SHADER_PARAMETER(FScreenTransform, SvPositionToSceneColorUV)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, LogLumTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, BaseLayerTexture)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, EyeAdaptationBuffer)
		SHADER_PARAMETER(float, Strength)
		SHADER_PARAMETER(float, HighlightContrast)
		SHADER_PARAMETER(float, ShadowContrast)
		SHADER_PARAMETER(float, MaxLiftEV)
		SHADER_PARAMETER(float, MaxDarkenEV)
		SHADER_PARAMETER(float, MidtoneProtectionEV)
		SHADER_PARAMETER(float, MiddleGreyBias)
		SHADER_PARAMETER(uint32, ProtectDeepShadows)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};
IMPLEMENT_GLOBAL_SHADER(FToneMapCustomLocalExposureApplyPS, "/Plugin/ToneMapFX/Private/ToneMapCustomLocalExposure.usf", "ApplyPS", SF_Pixel);

FScreenPassTexture AddToneMapCustomLocalExposurePass(
	FRDGBuilder& GraphBuilder, const FViewInfo& View, FScreenPassTexture SceneColor,
	uint8 AutoExposureMode, FRDGTextureRef AdaptedLumTexture,
	float MinAutoExposure, float MaxAutoExposure,
	const FToneMapCustomLocalExposureSettings& Settings)
{
	const float Strength = FMath::Clamp(Settings.Strength, 0.0f, 1.0f);
	const float HighlightContrast = FMath::Clamp(Settings.HighlightContrast, 0.0f, 1.0f);
	const float ShadowContrast = FMath::Clamp(Settings.ShadowContrast, 0.0f, 1.0f);
	const float MaxLiftEV = FMath::Clamp(Settings.MaxLiftEV, 0.0f, 8.0f);
	const float MaxDarkenEV = FMath::Clamp(Settings.MaxDarkenEV, 0.0f, 8.0f);
	if (!SceneColor.IsValid() || SceneColor.ViewRect.IsEmpty() || View.GetFeatureLevel() < ERHIFeatureLevel::SM5 ||
		Strength <= 0.0f || (MaxLiftEV <= 0.0f && MaxDarkenEV <= 0.0f) ||
		(HighlightContrast >= 1.0f && ShadowContrast >= 1.0f))
	{
		return SceneColor;
	}

	RDG_EVENT_SCOPE(GraphBuilder, "ToneMapFX DurandLocalExposure");
	// Full-resolution analysis keeps this prototype comparable to the existing
	// Durand operator without introducing low-resolution upsampling artifacts.
	const FIntPoint WorkSize = SceneColor.ViewRect.Size();
	const FIntRect WorkRect(FIntPoint::ZeroValue, WorkSize);
	const FScreenPassTextureViewport WorkViewport(WorkSize, WorkRect);
	const FScreenTransform SceneColorUV =
		FScreenTransform::ChangeTextureBasisFromTo(WorkViewport, FScreenTransform::ETextureBasis::TexelPosition, FScreenTransform::ETextureBasis::ViewportUV) *
		FScreenTransform::ChangeTextureBasisFromTo(FScreenPassTextureViewport(SceneColor), FScreenTransform::ETextureBasis::ViewportUV, FScreenTransform::ETextureBasis::TextureUV);
	const FRDGTextureDesc LogDesc = FRDGTextureDesc::Create2D(WorkSize, PF_R32_FLOAT,
		FClearValueBinding::None, TexCreate_ShaderResource | TexCreate_RenderTargetable);
	FRDGTextureRef LogLum = GraphBuilder.CreateTexture(LogDesc, TEXT("ToneMap.CustomLocalExposure.LogLum"));
	{
		auto* P = GraphBuilder.AllocParameters<FToneMapCustomLocalExposureLogLumPS::FParameters>();
		P->SceneColorTexture = SceneColor.Texture;
		P->SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		P->SvPositionToSceneColorUV = SceneColorUV;
		P->OneOverPreExposure = 1.0f / FMath::Max(View.PreExposure, 0.001f);
		P->LuminanceWeights = FVector3f(UE::Color::FColorSpace::GetWorking().GetLuminanceFactors());
		P->RenderTargets[0] = FRenderTargetBinding(LogLum, ERenderTargetLoadAction::ENoAction);
		TShaderMapRef<FToneMapCustomLocalExposureLogLumPS> Shader(View.ShaderMap);
		FPixelShaderUtils::AddFullscreenPass(GraphBuilder, View.ShaderMap,
			RDG_EVENT_NAME("CustomLocalExposure.LogLum"), Shader, P, WorkRect);
	}

	FRDGTextureRef BasePing = GraphBuilder.CreateTexture(LogDesc, TEXT("ToneMap.CustomLocalExposure.BasePing"));
	FRDGTextureRef BasePong = GraphBuilder.CreateTexture(LogDesc, TEXT("ToneMap.CustomLocalExposure.BasePong"));
	auto FilterBase = [&](FRDGTextureRef Input, FRDGTextureRef Output, FIntPoint Direction, const TCHAR* EventName)
	{
		auto* P = GraphBuilder.AllocParameters<FToneMapCustomLocalExposureBilateralPS::FParameters>();
		P->LogLumTexture = Input;
		P->GuideTexture = LogLum;
		P->WorkSize = WorkSize;
		P->BlurDirection = Direction;
		P->SpatialSigma = FMath::Clamp(Settings.SpatialSigma, 1.0f, 32.0f);
		P->RangeSigmaEV = FMath::Clamp(Settings.RangeSigmaEV, 0.05f, 8.0f);
		P->RenderTargets[0] = FRenderTargetBinding(Output, ERenderTargetLoadAction::ENoAction);
		TShaderMapRef<FToneMapCustomLocalExposureBilateralPS> Shader(View.ShaderMap);
		FPixelShaderUtils::AddFullscreenPass(GraphBuilder, View.ShaderMap,
			FRDGEventName(EventName), Shader, P, WorkRect);
	};
	FilterBase(LogLum, BasePing, FIntPoint(1, 0), TEXT("CustomLocalExposure.BilateralH"));
	FilterBase(BasePing, BasePong, FIntPoint(0, 1), TEXT("CustomLocalExposure.BilateralV"));

	FRDGBufferRef Exposure = AddToneMapLocalExposureBuffer(GraphBuilder, View, SceneColor,
		AutoExposureMode, AdaptedLumTexture, MinAutoExposure, MaxAutoExposure);
	FRDGTextureRef Result = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(WorkSize, PF_FloatRGBA,
		FClearValueBinding::None, TexCreate_ShaderResource | TexCreate_RenderTargetable), TEXT("ToneMap.CustomLocalExposure.SceneColor"));
	{
		auto* P = GraphBuilder.AllocParameters<FToneMapCustomLocalExposureApplyPS::FParameters>();
		P->SceneColorTexture = SceneColor.Texture;
		P->SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		P->SvPositionToSceneColorUV = SceneColorUV;
		P->LogLumTexture = LogLum;
		P->BaseLayerTexture = BasePong;
		P->EyeAdaptationBuffer = GraphBuilder.CreateSRV(Exposure);
		P->Strength = Strength;
		P->HighlightContrast = HighlightContrast;
		P->ShadowContrast = ShadowContrast;
		P->MaxLiftEV = MaxLiftEV;
		P->MaxDarkenEV = MaxDarkenEV;
		P->MidtoneProtectionEV = FMath::Clamp(Settings.MidtoneProtectionEV, 0.0f, 4.0f);
		P->MiddleGreyBias = FMath::Clamp(Settings.MiddleGreyBias, -4.0f, 4.0f);
		P->ProtectDeepShadows = Settings.bProtectDeepShadows ? 1u : 0u;
		P->RenderTargets[0] = FRenderTargetBinding(Result, ERenderTargetLoadAction::ENoAction);
		TShaderMapRef<FToneMapCustomLocalExposureApplyPS> Shader(View.ShaderMap);
		FPixelShaderUtils::AddFullscreenPass(GraphBuilder, View.ShaderMap,
			RDG_EVENT_NAME("CustomLocalExposure.Apply"), Shader, P, WorkRect);
	}
	return FScreenPassTexture(Result, WorkRect);
}
