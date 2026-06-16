// Licensed under the zlib License. See LICENSE file in the project root.

#include "ToneMapSubsystem.h"
#include "ToneMapComponent.h"
#include "ToneMapShaders.h"
#include "ClassicBloomShaders.h"
#include "ToneMapDurand.h"
#include "ToneMapFattal.h"
#include "ToneMapLensEffects.h"
#include "ToneMapVignetteShaders.h"
#include "ToneMapSharpenShaders.h"
#include "ToneMapLUTShaders.h"
#include "ToneMapCombineLUTShaders.h"
#include "SceneView.h"
#include "SceneRendering.h"
#include "ScreenPass.h"
#include "PostProcess/PostProcessMaterialInputs.h"
#include "PostProcess/PostProcessTonemap.h"
#include "ToneMapHDREncode.h"
#include "HDRHelper.h"
#include "RHIGlobals.h"
#include "RenderGraphUtils.h"
#include "PixelShaderUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
struct FToneMapWeightedComponent
{
	UToneMapComponent* Component = nullptr;
	float RawWeight = 0.0f;
	float NormalizedWeight = 0.0f;
};

struct FToneMapResolvedSettings
{
	EToneMapMode Mode = EToneMapMode::PostProcess;
	EToneMapProcessingPath ProcessingPath = EToneMapProcessingPath::PerPixel;
	EToneMapPostProcessPass PostProcessPass = EToneMapPostProcessPass::Tonemap;
	EToneMapAutoExposure AutoExposureMode = EToneMapAutoExposure::EngineDefault;
	EToneMapFilmCurve FilmCurve = EToneMapFilmCurve::Hable;
	EToneMapHDROutputMode HDROutputMode = EToneMapHDROutputMode::PaperWhiteNits;
	EToneMapSharpenMethod SharpenMethod = EToneMapSharpenMethod::LegacyUnsharpMask;
	EAgXLook AgXLook = EAgXLook::None;
	EToneMapColorBlendSpace ColorBlendSpace = EToneMapColorBlendSpace::LegacyLinearRGB;
	EBloomMode BloomMode = EBloomMode::SoftFocus;
	EBloomBlendMode BloomBlendMode = EBloomBlendMode::SoftLight;
	EToneMapHaloPattern HaloPattern = EToneMapHaloPattern::StretchedLines;
	EToneMapHSLMode HSLMode = EToneMapHSLMode::Smooth;
	EVignetteMode VignetteMode = EVignetteMode::Circular;
	EVignetteFalloff VignetteFalloff = EVignetteFalloff::Smooth;
	EVignetteTextureChannel VignetteTextureChannel = EVignetteTextureChannel::Alpha;

	bool bHDROutput = false;
	bool bAutoDetectHDROutput = false;
	bool bDebugHDRLogging = false;
	bool bEnableWhiteBalance = true;
	bool bEnableToneAdjustments = true;
	bool bUseCameraExposure = false;
	bool bEnableDithering = true;
	bool bForceFP16Pipeline = true;
	bool bEnableCiliaryCorona = false;
	bool bEnableLenticularHalo = false;
	bool bEnableBloom = false;
	bool bUseSceneColor = true;
	bool bProtectHighlights = false;
	bool bKawaseSoftThreshold = true;
	bool bEnableVignette = false;
	bool bVignetteUseAlphaTexture = false;
	bool bVignetteAlphaTextureOnly = false;
	bool bEnableLUT = false;
	bool bEnableSharpening = false;
	bool bDisableUnrealBloom = true;
	bool bDisableUnrealMotionBlur = false;

	float PaperWhiteNits = 200.0f;
	float Temperature = 0.0f;
	float Tint = 0.0f;
	float Exposure = 0.0f;
	float Contrast = 0.0f;
	float Highlights = 0.0f;
	float Shadows = 0.0f;
	float Whites = 0.0f;
	float Blacks = 0.0f;
	float ToneSmoothing = 100.0f;
	float ContrastMidpoint = 0.18f;
	float Clarity = 0.0f;
	float ClarityRadius = 8.0f;
	float Vibrance = 0.0f;
	float Saturation = 0.0f;
	float SharpenAmount = 25.0f;
	float SharpenRadius = 1.0f;
	float DynamicContrast = 0.0f;
	float CorrectContrast = 0.0f;
	float CorrectColorCast = 0.0f;
	float CurveHighlights = 0.0f;
	float CurveLights = 0.0f;
	float CurveDarks = 0.0f;
	float CurveShadows = 0.0f;
	float HSLSmoothing = 100.0f;
	float HueReds = 0.0f;
	float HueOranges = 0.0f;
	float HueYellows = 0.0f;
	float HueGreens = 0.0f;
	float HueAquas = 0.0f;
	float HueBlues = 0.0f;
	float HuePurples = 0.0f;
	float HueMagentas = 0.0f;
	float SatReds = 0.0f;
	float SatOranges = 0.0f;
	float SatYellows = 0.0f;
	float SatGreens = 0.0f;
	float SatAquas = 0.0f;
	float SatBlues = 0.0f;
	float SatPurples = 0.0f;
	float SatMagentas = 0.0f;
	float LumReds = 0.0f;
	float LumOranges = 0.0f;
	float LumYellows = 0.0f;
	float LumGreens = 0.0f;
	float LumAquas = 0.0f;
	float LumBlues = 0.0f;
	float LumPurples = 0.0f;
	float LumMagentas = 0.0f;
	float CameraISO = 100.0f;
	float ShutterSpeedDenominator = 125.0f;
	float Aperture = 5.6f;
	float DitherQuantization = 1.0f / 255.0f;
	float AdaptationSpeedUp = 3.0f;
	float AdaptationSpeedDown = 1.0f;
	float MinAutoExposure = 0.05f;
	float MaxAutoExposure = 20.0f;
	float HableShoulderStrength = 0.15f;
	float HableLinearStrength = 0.50f;
	float HableLinearAngle = 0.10f;
	float HableToeStrength = 0.20f;
	float HableToeNumerator = 0.02f;
	float HableToeDenominator = 0.30f;
	float HableWhitePoint = 11.2f;
	float ReinhardWhitePoint = 100.0f;
	float CustomCurveMinEV = -10.0f;
	float CustomCurveMaxEV = 6.5f;
	float HDRSaturation = 1.0f;
	FLinearColor HDRColorBalance = FLinearColor::White;
	float DurandSpatialSigma = 16.0f;
	float DurandRangeSigma = 0.35f;
	float DurandBaseCompression = 0.5f;
	float DurandDetailBoost = 1.0f;
	float FattalAlpha = 0.1f;
	float FattalBeta = 0.9f;
	float FattalSaturation = 0.8f;
	float FattalNoise = 0.0001f;
	int32 FattalJacobiIterations = 30;
	float AgXMinEV = -10.0f;
	float AgXMaxEV = 6.5f;
	float CoronaIntensity = 0.5f;
	int32 CoronaSpikeCount = 6;
	int32 CoronaSpikeLength = 80;
	float CoronaThreshold = 0.8f;
	float HaloIntensity = 0.3f;
	float HaloRadius = 0.15f;
	float HaloThickness = 0.03f;
	float HaloIrregularity = 0.45f;
	float HaloArcStretch = 0.35f;
	int32 HaloLineCount = 5;
	float HaloLineThickness = 5.0f;
	float HaloThreshold = 0.9f;
	FLinearColor HaloTint = FLinearColor(0.85f, 0.90f, 1.0f, 1.0f);
	float BloomIntensity = 1.0f;
	float BloomThreshold = 0.8f;
	float BloomThresholdSoftness = 0.5f;
	float BloomMaxBrightness = 0.0f;
	float BloomSize = 16.0f;
	EBloomColorMode BloomColorMode = EBloomColorMode::SceneColor;
	FLinearColor BloomTint = FLinearColor::White;
	float BloomBlendStrength = 1.0f;
	float BloomSaturation = 1.0f;
	float HighlightProtection = 0.5f;
	float DownsampleScale = 1.0f;
	int32 BlurPasses = 1;
	int32 GlareStreakCount = 6;
	int32 GlareStreakLength = 40;
	float GlareRotationOffset = 0.0f;
	float GlareFalloff = 3.0f;
	int32 GlareSamples = 16;
	int32 KawaseMipCount = 5;
	float KawaseFilterRadius = 0.002f;
	float KawaseThresholdKnee = 0.5f;
	float SoftFocusOverlayMultiplier = 0.5f;
	float SoftFocusBlendStrength = 0.33f;
	float SoftFocusSoftLightMultiplier = 0.4f;
	float SoftFocusFinalBlend = 0.25f;
	float VignetteSize = 30.0f;
	float VignetteIntensity = 50.0f;
	float VignetteFalloffExponent = 2.0f;
	float LUTIntensity = 1.0f;
	UTexture* CustomToneCurveTexture = nullptr;
	UTexture* LUTTexture = nullptr;
	UTexture* VignetteAlphaTexture = nullptr;

	bool IsAnyHSLActive() const
	{
		const float Eps = 0.01f;
		return FMath::Abs(HueReds) > Eps || FMath::Abs(HueOranges) > Eps ||
			FMath::Abs(HueYellows) > Eps || FMath::Abs(HueGreens) > Eps ||
			FMath::Abs(HueAquas) > Eps || FMath::Abs(HueBlues) > Eps ||
			FMath::Abs(HuePurples) > Eps || FMath::Abs(HueMagentas) > Eps ||
			FMath::Abs(SatReds) > Eps || FMath::Abs(SatOranges) > Eps ||
			FMath::Abs(SatYellows) > Eps || FMath::Abs(SatGreens) > Eps ||
			FMath::Abs(SatAquas) > Eps || FMath::Abs(SatBlues) > Eps ||
			FMath::Abs(SatPurples) > Eps || FMath::Abs(SatMagentas) > Eps ||
			FMath::Abs(LumReds) > Eps || FMath::Abs(LumOranges) > Eps ||
			FMath::Abs(LumYellows) > Eps || FMath::Abs(LumGreens) > Eps ||
			FMath::Abs(LumAquas) > Eps || FMath::Abs(LumBlues) > Eps ||
			FMath::Abs(LumPurples) > Eps || FMath::Abs(LumMagentas) > Eps;
	}

	bool IsAnyCurveActive() const
	{
		const float Eps = 0.01f;
		return FMath::Abs(CurveHighlights) > Eps || FMath::Abs(CurveLights) > Eps ||
			FMath::Abs(CurveDarks) > Eps || FMath::Abs(CurveShadows) > Eps;
	}
};

static const TCHAR* GetBoolText(const bool bValue)
{
	return bValue ? TEXT("true") : TEXT("false");
}

static const TCHAR* GetHDROutputModeText(const EToneMapHDROutputMode Mode)
{
	switch (Mode)
	{
	case EToneMapHDROutputMode::PaperWhiteNits:
		return TEXT("PaperWhiteNits");
	case EToneMapHDROutputMode::TrueHDR:
		return TEXT("TrueHDR");
	default:
		return TEXT("Unknown");
	}
}

static void CopyToneMapSettings(const UToneMapComponent& Component, FToneMapResolvedSettings& Out)
{
#define COPY_FIELD(FieldName) Out.FieldName = Component.FieldName
	COPY_FIELD(Mode);
	COPY_FIELD(ProcessingPath);
	COPY_FIELD(PostProcessPass);
	COPY_FIELD(AutoExposureMode);
	COPY_FIELD(FilmCurve);
	COPY_FIELD(HDROutputMode);
	COPY_FIELD(SharpenMethod);
	COPY_FIELD(AgXLook);
	COPY_FIELD(ColorBlendSpace);
	COPY_FIELD(BloomMode);
	COPY_FIELD(BloomBlendMode);
	COPY_FIELD(HaloPattern);
	COPY_FIELD(HSLMode);
	COPY_FIELD(VignetteMode);
	COPY_FIELD(VignetteFalloff);
	COPY_FIELD(VignetteTextureChannel);
	COPY_FIELD(bHDROutput);
	COPY_FIELD(bAutoDetectHDROutput);
	COPY_FIELD(bDebugHDRLogging);
	COPY_FIELD(bEnableWhiteBalance);
	COPY_FIELD(bEnableToneAdjustments);
	COPY_FIELD(bUseCameraExposure);
	COPY_FIELD(bEnableDithering);
	COPY_FIELD(bForceFP16Pipeline);
	COPY_FIELD(bEnableCiliaryCorona);
	COPY_FIELD(bEnableLenticularHalo);
	COPY_FIELD(bEnableBloom);
	COPY_FIELD(bUseSceneColor);
	COPY_FIELD(bProtectHighlights);
	COPY_FIELD(bKawaseSoftThreshold);
	COPY_FIELD(bEnableVignette);
	COPY_FIELD(bVignetteUseAlphaTexture);
	COPY_FIELD(bVignetteAlphaTextureOnly);
	COPY_FIELD(bEnableLUT);
	COPY_FIELD(bEnableSharpening);
	COPY_FIELD(bDisableUnrealBloom);
	COPY_FIELD(bDisableUnrealMotionBlur);
	COPY_FIELD(PaperWhiteNits);
	COPY_FIELD(Temperature);
	COPY_FIELD(Tint);
	COPY_FIELD(Exposure);
	COPY_FIELD(Contrast);
	COPY_FIELD(Highlights);
	COPY_FIELD(Shadows);
	COPY_FIELD(Whites);
	COPY_FIELD(Blacks);
	COPY_FIELD(ToneSmoothing);
	COPY_FIELD(ContrastMidpoint);
	COPY_FIELD(Clarity);
	COPY_FIELD(ClarityRadius);
	COPY_FIELD(Vibrance);
	COPY_FIELD(Saturation);
	COPY_FIELD(SharpenAmount);
	COPY_FIELD(SharpenRadius);
	COPY_FIELD(DynamicContrast);
	COPY_FIELD(CorrectContrast);
	COPY_FIELD(CorrectColorCast);
	COPY_FIELD(CurveHighlights);
	COPY_FIELD(CurveLights);
	COPY_FIELD(CurveDarks);
	COPY_FIELD(CurveShadows);
	COPY_FIELD(HSLSmoothing);
	COPY_FIELD(HueReds);
	COPY_FIELD(HueOranges);
	COPY_FIELD(HueYellows);
	COPY_FIELD(HueGreens);
	COPY_FIELD(HueAquas);
	COPY_FIELD(HueBlues);
	COPY_FIELD(HuePurples);
	COPY_FIELD(HueMagentas);
	COPY_FIELD(SatReds);
	COPY_FIELD(SatOranges);
	COPY_FIELD(SatYellows);
	COPY_FIELD(SatGreens);
	COPY_FIELD(SatAquas);
	COPY_FIELD(SatBlues);
	COPY_FIELD(SatPurples);
	COPY_FIELD(SatMagentas);
	COPY_FIELD(LumReds);
	COPY_FIELD(LumOranges);
	COPY_FIELD(LumYellows);
	COPY_FIELD(LumGreens);
	COPY_FIELD(LumAquas);
	COPY_FIELD(LumBlues);
	COPY_FIELD(LumPurples);
	COPY_FIELD(LumMagentas);
	COPY_FIELD(CameraISO);
	COPY_FIELD(ShutterSpeedDenominator);
	COPY_FIELD(Aperture);
	COPY_FIELD(DitherQuantization);
	COPY_FIELD(AdaptationSpeedUp);
	COPY_FIELD(AdaptationSpeedDown);
	COPY_FIELD(MinAutoExposure);
	COPY_FIELD(MaxAutoExposure);
	COPY_FIELD(HableShoulderStrength);
	COPY_FIELD(HableLinearStrength);
	COPY_FIELD(HableLinearAngle);
	COPY_FIELD(HableToeStrength);
	COPY_FIELD(HableToeNumerator);
	COPY_FIELD(HableToeDenominator);
	COPY_FIELD(HableWhitePoint);
	COPY_FIELD(ReinhardWhitePoint);
	COPY_FIELD(CustomCurveMinEV);
	COPY_FIELD(CustomCurveMaxEV);
	COPY_FIELD(HDRSaturation);
	COPY_FIELD(HDRColorBalance);
	COPY_FIELD(DurandSpatialSigma);
	COPY_FIELD(DurandRangeSigma);
	COPY_FIELD(DurandBaseCompression);
	COPY_FIELD(DurandDetailBoost);
	COPY_FIELD(FattalAlpha);
	COPY_FIELD(FattalBeta);
	COPY_FIELD(FattalSaturation);
	COPY_FIELD(FattalNoise);
	COPY_FIELD(FattalJacobiIterations);
	COPY_FIELD(AgXMinEV);
	COPY_FIELD(AgXMaxEV);
	COPY_FIELD(CoronaIntensity);
	COPY_FIELD(CoronaSpikeCount);
	COPY_FIELD(CoronaSpikeLength);
	COPY_FIELD(CoronaThreshold);
	COPY_FIELD(HaloIntensity);
	COPY_FIELD(HaloRadius);
	COPY_FIELD(HaloThickness);
	COPY_FIELD(HaloIrregularity);
	COPY_FIELD(HaloArcStretch);
	COPY_FIELD(HaloLineCount);
	COPY_FIELD(HaloLineThickness);
	COPY_FIELD(HaloThreshold);
	COPY_FIELD(HaloTint);
	COPY_FIELD(BloomIntensity);
	COPY_FIELD(BloomThreshold);
	COPY_FIELD(BloomThresholdSoftness);
	COPY_FIELD(BloomMaxBrightness);
	COPY_FIELD(BloomSize);
	COPY_FIELD(BloomColorMode);
	COPY_FIELD(BloomTint);
	COPY_FIELD(BloomBlendStrength);
	COPY_FIELD(BloomSaturation);
	COPY_FIELD(HighlightProtection);
	COPY_FIELD(DownsampleScale);
	COPY_FIELD(BlurPasses);
	COPY_FIELD(GlareStreakCount);
	COPY_FIELD(GlareStreakLength);
	COPY_FIELD(GlareRotationOffset);
	COPY_FIELD(GlareFalloff);
	COPY_FIELD(GlareSamples);
	COPY_FIELD(KawaseMipCount);
	COPY_FIELD(KawaseFilterRadius);
	COPY_FIELD(KawaseThresholdKnee);
	COPY_FIELD(SoftFocusOverlayMultiplier);
	COPY_FIELD(SoftFocusBlendStrength);
	COPY_FIELD(SoftFocusSoftLightMultiplier);
	COPY_FIELD(SoftFocusFinalBlend);
	COPY_FIELD(VignetteSize);
	COPY_FIELD(VignetteIntensity);
	COPY_FIELD(VignetteFalloffExponent);
	COPY_FIELD(LUTIntensity);
#undef COPY_FIELD
	Out.CustomToneCurveTexture = Component.GetCustomToneCurveTexture();
	Out.LUTTexture = Component.LUTTexture.Get();
	Out.VignetteAlphaTexture = Component.VignetteAlphaTexture.Get();

	if (!Component.bUseSceneColor && Out.BloomColorMode == EBloomColorMode::SceneColor)
	{
		Out.BloomColorMode = EBloomColorMode::Tint;
	}
}

static FRHITexture* GetTextureRHI(UTexture* Texture)
{
	if (!Texture || !Texture->GetResource())
	{
		return nullptr;
	}

	return Texture->GetResource()->TextureRHI;
}

static FRDGTextureRef RegisterExternalTextureOrFallback(
	FRDGBuilder& GraphBuilder,
	UTexture* Texture,
	FRDGTextureRef FallbackTexture,
	const TCHAR* DebugName)
{
	if (FRHITexture* TextureRHI = GetTextureRHI(Texture))
	{
		return GraphBuilder.RegisterExternalTexture(CreateRenderTarget(TextureRHI, DebugName));
	}

	return FallbackTexture;
}

static float GetShaderFilmCurveMode(const FToneMapResolvedSettings& Settings)
{
	if (Settings.FilmCurve == EToneMapFilmCurve::CustomLuminanceCurve && !GetTextureRHI(Settings.CustomToneCurveTexture))
	{
		return (float)static_cast<uint8>(EToneMapFilmCurve::ReinhardStandard);
	}

	return (float)static_cast<uint8>(Settings.FilmCurve);
}

template <typename GetterType>
static float BlendFloat(float DefaultValue, const TArray<FToneMapWeightedComponent>& Candidates, float TotalWeight, GetterType Getter)
{
	float WeightedValue = 0.0f;
	for (const FToneMapWeightedComponent& Candidate : Candidates)
	{
		WeightedValue += Getter(Candidate.Component) * Candidate.NormalizedWeight;
	}
	return FMath::Lerp(DefaultValue, WeightedValue, TotalWeight);
}

struct FOklabColor
{
	float L = 0.0f;
	float A = 0.0f;
	float B = 0.0f;
	float Alpha = 1.0f;
};

static float SignedCubeRoot(float Value)
{
	return Value >= 0.0f
		? FMath::Pow(Value, 1.0f / 3.0f)
		: -FMath::Pow(-Value, 1.0f / 3.0f);
}

static FOklabColor LinearSRGBToOklab(const FLinearColor& Color)
{
	const float L = 0.4122214708f * Color.R + 0.5363325363f * Color.G + 0.0514459929f * Color.B;
	const float M = 0.2119034982f * Color.R + 0.6806995451f * Color.G + 0.1073969566f * Color.B;
	const float S = 0.0883024619f * Color.R + 0.2817188376f * Color.G + 0.6299787005f * Color.B;

	const float LPrime = SignedCubeRoot(L);
	const float MPrime = SignedCubeRoot(M);
	const float SPrime = SignedCubeRoot(S);

	FOklabColor Result;
	Result.L = 0.2104542553f * LPrime + 0.7936177850f * MPrime - 0.0040720468f * SPrime;
	Result.A = 1.9779984951f * LPrime - 2.4285922050f * MPrime + 0.4505937099f * SPrime;
	Result.B = 0.0259040371f * LPrime + 0.7827717662f * MPrime - 0.8086757660f * SPrime;
	Result.Alpha = Color.A;
	return Result;
}

static FLinearColor OklabToLinearSRGB(const FOklabColor& Color)
{
	const float LPrime = Color.L + 0.3963377774f * Color.A + 0.2158037573f * Color.B;
	const float MPrime = Color.L - 0.1055613458f * Color.A - 0.0638541728f * Color.B;
	const float SPrime = Color.L - 0.0894841775f * Color.A - 1.2914855480f * Color.B;

	const float L = LPrime * LPrime * LPrime;
	const float M = MPrime * MPrime * MPrime;
	const float S = SPrime * SPrime * SPrime;

	return FLinearColor(
		FMath::Max(4.0767416621f * L - 3.3077115913f * M + 0.2309699292f * S, 0.0f),
		FMath::Max(-1.2684380046f * L + 2.6097574011f * M - 0.3413193965f * S, 0.0f),
		FMath::Max(-0.0041960863f * L - 0.7034186147f * M + 1.7076147010f * S, 0.0f),
		Color.Alpha);
}

template <typename GetterType>
static FLinearColor BlendColor(const FLinearColor& DefaultValue, const TArray<FToneMapWeightedComponent>& Candidates, float TotalWeight, EToneMapColorBlendSpace ColorBlendSpace, GetterType Getter)
{
	if (ColorBlendSpace == EToneMapColorBlendSpace::LegacyLinearRGB)
	{
		FLinearColor WeightedValue = FLinearColor::Transparent;
		for (const FToneMapWeightedComponent& Candidate : Candidates)
		{
			const FLinearColor Value = Getter(Candidate.Component);
			WeightedValue.R += Value.R * Candidate.NormalizedWeight;
			WeightedValue.G += Value.G * Candidate.NormalizedWeight;
			WeightedValue.B += Value.B * Candidate.NormalizedWeight;
			WeightedValue.A += Value.A * Candidate.NormalizedWeight;
		}

		return FLinearColor(
			FMath::Lerp(DefaultValue.R, WeightedValue.R, TotalWeight),
			FMath::Lerp(DefaultValue.G, WeightedValue.G, TotalWeight),
			FMath::Lerp(DefaultValue.B, WeightedValue.B, TotalWeight),
			FMath::Lerp(DefaultValue.A, WeightedValue.A, TotalWeight));
	}

	FOklabColor WeightedValue;
	WeightedValue.Alpha = 0.0f;
	for (const FToneMapWeightedComponent& Candidate : Candidates)
	{
		const FOklabColor Value = LinearSRGBToOklab(Getter(Candidate.Component));
		WeightedValue.L += Value.L * Candidate.NormalizedWeight;
		WeightedValue.A += Value.A * Candidate.NormalizedWeight;
		WeightedValue.B += Value.B * Candidate.NormalizedWeight;
		WeightedValue.Alpha += Value.Alpha * Candidate.NormalizedWeight;
	}

	const FOklabColor DefaultOklab = LinearSRGBToOklab(DefaultValue);
	FOklabColor BlendedValue;
	BlendedValue.L = FMath::Lerp(DefaultOklab.L, WeightedValue.L, TotalWeight);
	BlendedValue.A = FMath::Lerp(DefaultOklab.A, WeightedValue.A, TotalWeight);
	BlendedValue.B = FMath::Lerp(DefaultOklab.B, WeightedValue.B, TotalWeight);
	BlendedValue.Alpha = FMath::Lerp(DefaultOklab.Alpha, WeightedValue.Alpha, TotalWeight);
	return OklabToLinearSRGB(BlendedValue);
}

template <typename PredicateType>
static const UToneMapComponent* FindDominantComponentForFeature(const TArray<FToneMapWeightedComponent>& Candidates, PredicateType Predicate)
{
	const UToneMapComponent* DominantComponent = nullptr;
	float DominantWeight = 0.0f;

	for (const FToneMapWeightedComponent& Candidate : Candidates)
	{
		const UToneMapComponent* Component = Candidate.Component;
		if (!Component || !Predicate(Component))
		{
			continue;
		}

		const bool bTakesDominance =
			!DominantComponent ||
			Component->BlendPriority > DominantComponent->BlendPriority ||
			(Component->BlendPriority == DominantComponent->BlendPriority && Candidate.RawWeight > DominantWeight);

		if (bTakesDominance)
		{
			DominantComponent = Component;
			DominantWeight = Candidate.RawWeight;
		}
	}

	return DominantComponent;
}

static bool ResolveToneMapSettings(
	const TArray<TWeakObjectPtr<UToneMapComponent>>& Components,
	const FVector& ViewLocation,
	FToneMapResolvedSettings& OutSettings,
	UToneMapComponent*& OutDominantComponent)
{
	TArray<FToneMapWeightedComponent> Candidates;
	float RawWeightSum = 0.0f;
	UToneMapComponent* DominantComponent = nullptr;
	float DominantWeight = 0.0f;

	for (const TWeakObjectPtr<UToneMapComponent>& Ptr : Components)
	{
		UToneMapComponent* Component = Ptr.Get();
		if (!Component || !Component->IsActive() || !Component->bEnabled)
		{
			continue;
		}

		const float Weight = Component->GetBlendWeightAtLocation(ViewLocation);
		if (Weight <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		FToneMapWeightedComponent& Candidate = Candidates.AddDefaulted_GetRef();
		Candidate.Component = Component;
		Candidate.RawWeight = Weight;
		RawWeightSum += Weight;

		const bool bTakesDominance =
			!DominantComponent ||
			Component->BlendPriority > DominantComponent->BlendPriority ||
			(Component->BlendPriority == DominantComponent->BlendPriority && Weight > DominantWeight);

		if (bTakesDominance)
		{
			DominantComponent = Component;
			DominantWeight = Weight;
		}
	}

	if (!DominantComponent || RawWeightSum <= KINDA_SMALL_NUMBER)
	{
		OutDominantComponent = nullptr;
		return false;
	}

	for (FToneMapWeightedComponent& Candidate : Candidates)
	{
		Candidate.NormalizedWeight = Candidate.RawWeight / RawWeightSum;
	}

	const float TotalWeight = FMath::Clamp(RawWeightSum, 0.0f, 1.0f);
	const UToneMapComponent* Defaults = GetDefault<UToneMapComponent>();

	CopyToneMapSettings(*DominantComponent, OutSettings);

#define BLEND_FIELD(FieldName) OutSettings.FieldName = BlendFloat(Defaults->FieldName, Candidates, TotalWeight, [](const UToneMapComponent* Component) { return Component->FieldName; })
#define BLEND_EFFECT_FIELD(FieldName, DefaultValue, EnableField) OutSettings.FieldName = BlendFloat(DefaultValue, Candidates, TotalWeight, [](const UToneMapComponent* Component) { return Component->EnableField ? Component->FieldName : 0.0f; })
#define BLEND_COLOR_FIELD(FieldName) OutSettings.FieldName = BlendColor(Defaults->FieldName, Candidates, TotalWeight, OutSettings.ColorBlendSpace, [](const UToneMapComponent* Component) { return Component->FieldName; })

	OutSettings.Temperature = BlendFloat(0.0f, Candidates, TotalWeight, [](const UToneMapComponent* Component) { return Component->bEnableWhiteBalance ? Component->Temperature : 0.0f; });
	OutSettings.Tint        = BlendFloat(0.0f, Candidates, TotalWeight, [](const UToneMapComponent* Component) { return Component->bEnableWhiteBalance ? Component->Tint : 0.0f; });
	BLEND_FIELD(Exposure);
	BLEND_FIELD(Contrast);
	OutSettings.Highlights = BlendFloat(0.0f, Candidates, TotalWeight, [](const UToneMapComponent* Component) { return Component->bEnableToneAdjustments ? Component->Highlights : 0.0f; });
	OutSettings.Shadows    = BlendFloat(0.0f, Candidates, TotalWeight, [](const UToneMapComponent* Component) { return Component->bEnableToneAdjustments ? Component->Shadows : 0.0f; });
	OutSettings.Whites     = BlendFloat(0.0f, Candidates, TotalWeight, [](const UToneMapComponent* Component) { return Component->bEnableToneAdjustments ? Component->Whites : 0.0f; });
	OutSettings.Blacks     = BlendFloat(0.0f, Candidates, TotalWeight, [](const UToneMapComponent* Component) { return Component->bEnableToneAdjustments ? Component->Blacks : 0.0f; });
	BLEND_FIELD(ToneSmoothing);
	BLEND_FIELD(ContrastMidpoint);
	BLEND_FIELD(Clarity);
	BLEND_FIELD(ClarityRadius);
	BLEND_FIELD(Vibrance);
	BLEND_FIELD(Saturation);
	BLEND_EFFECT_FIELD(SharpenAmount, 0.0f, bEnableSharpening);
	BLEND_FIELD(SharpenRadius);
	BLEND_FIELD(DynamicContrast);
	BLEND_FIELD(CorrectContrast);
	BLEND_FIELD(CorrectColorCast);
	BLEND_FIELD(CurveHighlights);
	BLEND_FIELD(CurveLights);
	BLEND_FIELD(CurveDarks);
	BLEND_FIELD(CurveShadows);
	BLEND_FIELD(HSLSmoothing);
	BLEND_FIELD(HueReds);
	BLEND_FIELD(HueOranges);
	BLEND_FIELD(HueYellows);
	BLEND_FIELD(HueGreens);
	BLEND_FIELD(HueAquas);
	BLEND_FIELD(HueBlues);
	BLEND_FIELD(HuePurples);
	BLEND_FIELD(HueMagentas);
	BLEND_FIELD(SatReds);
	BLEND_FIELD(SatOranges);
	BLEND_FIELD(SatYellows);
	BLEND_FIELD(SatGreens);
	BLEND_FIELD(SatAquas);
	BLEND_FIELD(SatBlues);
	BLEND_FIELD(SatPurples);
	BLEND_FIELD(SatMagentas);
	BLEND_FIELD(LumReds);
	BLEND_FIELD(LumOranges);
	BLEND_FIELD(LumYellows);
	BLEND_FIELD(LumGreens);
	BLEND_FIELD(LumAquas);
	BLEND_FIELD(LumBlues);
	BLEND_FIELD(LumPurples);
	BLEND_FIELD(LumMagentas);
	BLEND_FIELD(PaperWhiteNits);
	BLEND_FIELD(DitherQuantization);
	BLEND_FIELD(AdaptationSpeedUp);
	BLEND_FIELD(AdaptationSpeedDown);
	BLEND_FIELD(MinAutoExposure);
	BLEND_FIELD(MaxAutoExposure);
	BLEND_FIELD(HableShoulderStrength);
	BLEND_FIELD(HableLinearStrength);
	BLEND_FIELD(HableLinearAngle);
	BLEND_FIELD(HableToeStrength);
	BLEND_FIELD(HableToeNumerator);
	BLEND_FIELD(HableToeDenominator);
	BLEND_FIELD(HableWhitePoint);
	BLEND_FIELD(ReinhardWhitePoint);
	BLEND_FIELD(HDRSaturation);
	BLEND_COLOR_FIELD(HDRColorBalance);
	BLEND_FIELD(DurandSpatialSigma);
	BLEND_FIELD(DurandRangeSigma);
	BLEND_FIELD(DurandBaseCompression);
	BLEND_FIELD(DurandDetailBoost);
	BLEND_FIELD(FattalAlpha);
	BLEND_FIELD(FattalBeta);
	BLEND_FIELD(FattalSaturation);
	BLEND_FIELD(FattalNoise);
	BLEND_FIELD(AgXMinEV);
	BLEND_FIELD(AgXMaxEV);
	BLEND_EFFECT_FIELD(CoronaIntensity, 0.0f, bEnableCiliaryCorona);
	BLEND_FIELD(CoronaThreshold);
	BLEND_EFFECT_FIELD(HaloIntensity, 0.0f, bEnableLenticularHalo);
	BLEND_FIELD(HaloRadius);
	BLEND_FIELD(HaloThickness);
	BLEND_FIELD(HaloIrregularity);
	BLEND_FIELD(HaloArcStretch);
	BLEND_FIELD(HaloLineThickness);
	BLEND_FIELD(HaloThreshold);
	OutSettings.HaloTint = BlendColor(Defaults->HaloTint, Candidates, TotalWeight, OutSettings.ColorBlendSpace, [Defaults](const UToneMapComponent* Component) { return Component->bEnableLenticularHalo ? Component->HaloTint : Defaults->HaloTint; });
	BLEND_EFFECT_FIELD(BloomIntensity, 0.0f, bEnableBloom);
	BLEND_FIELD(BloomThreshold);
	BLEND_FIELD(BloomThresholdSoftness);
	BLEND_FIELD(BloomMaxBrightness);
	BLEND_FIELD(BloomSize);
	OutSettings.BloomTint = BlendColor(Defaults->BloomTint, Candidates, TotalWeight, OutSettings.ColorBlendSpace, [Defaults](const UToneMapComponent* Component) { return Component->bEnableBloom ? Component->BloomTint : Defaults->BloomTint; });
	OutSettings.BloomBlendStrength = BlendFloat(Defaults->BloomBlendStrength, Candidates, TotalWeight, [Defaults](const UToneMapComponent* Component) { return Component->bEnableBloom ? Component->BloomBlendStrength : Defaults->BloomBlendStrength; });
	BLEND_FIELD(BloomSaturation);
	BLEND_FIELD(HighlightProtection);
	BLEND_FIELD(DownsampleScale);
	BLEND_FIELD(GlareRotationOffset);
	BLEND_FIELD(GlareFalloff);
	BLEND_FIELD(KawaseFilterRadius);
	BLEND_FIELD(KawaseThresholdKnee);
	BLEND_FIELD(SoftFocusOverlayMultiplier);
	BLEND_FIELD(SoftFocusBlendStrength);
	BLEND_FIELD(SoftFocusSoftLightMultiplier);
	BLEND_FIELD(SoftFocusFinalBlend);
	BLEND_FIELD(VignetteSize);
	BLEND_EFFECT_FIELD(VignetteIntensity, 0.0f, bEnableVignette);
	BLEND_FIELD(VignetteFalloffExponent);
	OutSettings.LUTIntensity = BlendFloat(0.0f, Candidates, TotalWeight, [](const UToneMapComponent* Component) { return (Component->bEnableLUT && Component->LUTTexture != nullptr) ? Component->LUTIntensity : 0.0f; });

	if (const UToneMapComponent* BloomOwner = FindDominantComponentForFeature(Candidates, [](const UToneMapComponent* Component) { return Component->bEnableBloom && Component->BloomIntensity > 0.01f; }))
	{
		OutSettings.BloomMode = BloomOwner->BloomMode;
		OutSettings.BloomBlendMode = BloomOwner->BloomBlendMode;
		OutSettings.BloomColorMode = BloomOwner->BloomColorMode;
		OutSettings.bUseSceneColor = BloomOwner->bUseSceneColor;
		OutSettings.bProtectHighlights = BloomOwner->bProtectHighlights;
		OutSettings.bKawaseSoftThreshold = BloomOwner->bKawaseSoftThreshold;
		OutSettings.BlurPasses = BloomOwner->BlurPasses;
		OutSettings.GlareStreakCount = BloomOwner->GlareStreakCount;
		OutSettings.GlareStreakLength = BloomOwner->GlareStreakLength;
		OutSettings.GlareSamples = BloomOwner->GlareSamples;
		OutSettings.KawaseMipCount = BloomOwner->KawaseMipCount;

		if (!BloomOwner->bUseSceneColor && OutSettings.BloomColorMode == EBloomColorMode::SceneColor)
		{
			OutSettings.BloomColorMode = EBloomColorMode::Tint;
		}
	}

	if (const UToneMapComponent* CoronaOwner = FindDominantComponentForFeature(Candidates, [](const UToneMapComponent* Component) { return Component->bEnableCiliaryCorona && Component->CoronaIntensity > 0.01f; }))
	{
		OutSettings.CoronaSpikeCount = CoronaOwner->CoronaSpikeCount;
		OutSettings.CoronaSpikeLength = CoronaOwner->CoronaSpikeLength;
	}

	if (const UToneMapComponent* LUTOwner = FindDominantComponentForFeature(Candidates, [](const UToneMapComponent* Component) { return Component->bEnableLUT && Component->LUTTexture != nullptr && Component->LUTIntensity > 0.001f; }))
	{
		OutSettings.LUTTexture = LUTOwner->LUTTexture.Get();
	}
	else
	{
		OutSettings.LUTTexture = nullptr;
	}

	if (const UToneMapComponent* VignetteOwner = FindDominantComponentForFeature(Candidates, [](const UToneMapComponent* Component) { return Component->bEnableVignette && FMath::Abs(Component->VignetteIntensity) > 0.01f; }))
	{
		OutSettings.VignetteMode = VignetteOwner->VignetteMode;
		OutSettings.VignetteFalloff = VignetteOwner->VignetteFalloff;
		OutSettings.VignetteTextureChannel = VignetteOwner->VignetteTextureChannel;
		OutSettings.bVignetteUseAlphaTexture = VignetteOwner->bVignetteUseAlphaTexture;
		OutSettings.bVignetteAlphaTextureOnly = VignetteOwner->bVignetteAlphaTextureOnly;
		OutSettings.VignetteAlphaTexture = VignetteOwner->VignetteAlphaTexture.Get();
	}
	else
	{
		OutSettings.VignetteAlphaTexture = nullptr;
	}

#undef BLEND_COLOR_FIELD
#undef BLEND_EFFECT_FIELD
#undef BLEND_FIELD

	OutSettings.bEnableWhiteBalance = FMath::Abs(OutSettings.Temperature) > 0.01f || FMath::Abs(OutSettings.Tint) > 0.01f;
	OutSettings.bEnableToneAdjustments =
		FMath::Abs(OutSettings.Highlights) > 0.01f ||
		FMath::Abs(OutSettings.Shadows) > 0.01f ||
		FMath::Abs(OutSettings.Whites) > 0.01f ||
		FMath::Abs(OutSettings.Blacks) > 0.01f;
	OutSettings.bEnableSharpening = OutSettings.SharpenAmount > 0.01f;
	OutSettings.bEnableCiliaryCorona = OutSettings.CoronaIntensity > 0.01f;
	OutSettings.bEnableLenticularHalo = OutSettings.HaloIntensity > 0.0f;
	OutSettings.bEnableBloom = OutSettings.BloomIntensity > 0.01f;
	OutSettings.bEnableVignette = FMath::Abs(OutSettings.VignetteIntensity) > 0.01f;
	OutSettings.bEnableLUT = OutSettings.LUTTexture != nullptr && OutSettings.LUTIntensity > 0.001f;

	OutDominantComponent = DominantComponent;
	return true;
}
} // namespace

// =============================================================================
// FToneMapSceneViewExtension
// =============================================================================

FToneMapSceneViewExtension::FToneMapSceneViewExtension(
	const FAutoRegister& AutoRegister,
	UToneMapSubsystem* InSubsystem)
	: FSceneViewExtensionBase(AutoRegister)
	, WeakSubsystem(InSubsystem)
{
}

FToneMapSceneViewExtension::~FToneMapSceneViewExtension()
{
	// TRefCountPtr<IPooledRenderTarget> destructor handles cleanup
}

void FToneMapSceneViewExtension::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
	UToneMapSubsystem* Subsystem = WeakSubsystem.Get();
	if (!Subsystem) return;

	FToneMapResolvedSettings ResolvedSettings;
	UToneMapComponent* DominantComponent = nullptr;
	if (!ResolveToneMapSettings(Subsystem->GetComponents(), InView.ViewLocation, ResolvedSettings, DominantComponent))
	{
		bCachedReplaceTonemap = false;
		bCachedHDROutput = false;
		LastHDRSetupDebugState.bValid = false;
		return;
	}
	(void)DominantComponent;

	bCachedReplaceTonemap = (ResolvedSettings.Mode == EToneMapMode::ReplaceTonemap);
	const bool bHDRRequested = bCachedReplaceTonemap && ResolvedSettings.bHDROutput;
	const bool bHDRAllowed = IsHDRAllowed();
	const bool bNoHDRCommandLine = FParse::Param(FCommandLine::Get(), TEXT("nohdr"));
	const bool bHDRAutoDetectAvailable = GRHISupportsHDROutput && !bNoHDRCommandLine;
	const bool bEffectiveHDROutput = ResolvedSettings.bAutoDetectHDROutput
		? (bHDRRequested && bHDRAutoDetectAvailable)
		: bHDRRequested;
	bCachedHDROutput = bEffectiveHDROutput;

	int32 HDRCVarValue = -1;
			// Auto-toggle r.HDR.EnableHDROutput to match the effective HDR state.
			// IConsoleManager is available through CoreMinimal.h — no extra includes.
			{
				static IConsoleVariable* CVarHDR = IConsoleManager::Get().FindConsoleVariable(TEXT("r.HDR.EnableHDROutput"));
				if (CVarHDR)
				{
					const int32 DesiredValue = bEffectiveHDROutput ? 1 : 0;
					if (CVarHDR->GetInt() != DesiredValue)
					{
						CVarHDR->Set(DesiredValue, ECVF_SetByCode);
					}
					HDRCVarValue = CVarHDR->GetInt();
				}
			}

	if (ResolvedSettings.bDebugHDRLogging)
	{
		const uint32 SetupOutputDevice = InViewFamily.RenderTarget
			? static_cast<uint32>(InViewFamily.RenderTarget->GetDisplayOutputFormat())
			: 0;
		const float SetupOutputMaxNits = HDRGetDisplayMaximumLuminance();
		const uint8 OutputMode = static_cast<uint8>(ResolvedSettings.HDROutputMode);
		const bool bStateChanged =
			!LastHDRSetupDebugState.bValid ||
			LastHDRSetupDebugState.bRequested != bHDRRequested ||
			LastHDRSetupDebugState.bEffective != bEffectiveHDROutput ||
			LastHDRSetupDebugState.bAutoDetect != ResolvedSettings.bAutoDetectHDROutput ||
			LastHDRSetupDebugState.bSupported != GRHISupportsHDROutput ||
			LastHDRSetupDebugState.bAllowed != bHDRAllowed ||
			LastHDRSetupDebugState.bNoHDRCommandLine != bNoHDRCommandLine ||
			LastHDRSetupDebugState.CVarValue != HDRCVarValue ||
			LastHDRSetupDebugState.OutputDevice != SetupOutputDevice ||
			LastHDRSetupDebugState.OutputMode != OutputMode ||
			!FMath::IsNearlyEqual(LastHDRSetupDebugState.OutputMaxLuminance, SetupOutputMaxNits, 0.5f) ||
			!FMath::IsNearlyEqual(LastHDRSetupDebugState.PaperWhiteNits, ResolvedSettings.PaperWhiteNits, 0.5f);

		if (bStateChanged)
		{
			UE_LOG(LogTemp, Log,
				TEXT("ToneMapFX HDR setup: Requested=%s Effective=%s AutoDetect=%s RHIHDR=%s UEAllowed=%s NoHDRCmd=%s CVar=%d OutputDevice=%u OutputMaxNits=%.1f Mode=%s PaperWhiteNits=%.1f"),
				GetBoolText(bHDRRequested),
				GetBoolText(bEffectiveHDROutput),
				GetBoolText(ResolvedSettings.bAutoDetectHDROutput),
				GetBoolText(GRHISupportsHDROutput),
				GetBoolText(bHDRAllowed),
				GetBoolText(bNoHDRCommandLine),
				HDRCVarValue,
				SetupOutputDevice,
				SetupOutputMaxNits,
				GetHDROutputModeText(ResolvedSettings.HDROutputMode),
				ResolvedSettings.PaperWhiteNits);

			LastHDRSetupDebugState.bValid = true;
			LastHDRSetupDebugState.bRequested = bHDRRequested;
			LastHDRSetupDebugState.bEffective = bEffectiveHDROutput;
			LastHDRSetupDebugState.bAutoDetect = ResolvedSettings.bAutoDetectHDROutput;
			LastHDRSetupDebugState.bSupported = GRHISupportsHDROutput;
			LastHDRSetupDebugState.bAllowed = bHDRAllowed;
			LastHDRSetupDebugState.bNoHDRCommandLine = bNoHDRCommandLine;
			LastHDRSetupDebugState.CVarValue = HDRCVarValue;
			LastHDRSetupDebugState.OutputDevice = SetupOutputDevice;
			LastHDRSetupDebugState.OutputMode = OutputMode;
			LastHDRSetupDebugState.OutputMaxLuminance = SetupOutputMaxNits;
			LastHDRSetupDebugState.PaperWhiteNits = ResolvedSettings.PaperWhiteNits;
		}
	}
	else
	{
		LastHDRSetupDebugState.bValid = false;
	}

			// Auto-toggle r.PostProcessing.PropagateAlpha to force FP16 precision
			// through the entire post-process chain (TAA/TSR, tonemapper output).
			// Prevents 10-bit/11-bit quantization banding at source.
			// Compatible with SMAA — the CVar is set once per frame on the game
			// thread, so the FAlphaChannelDim shader permutation is stable.
			{
				static IConsoleVariable* CVarPropAlpha = IConsoleManager::Get().FindConsoleVariable(TEXT("r.PostProcessing.PropagateAlpha"));
				if (CVarPropAlpha)
				{
					const int32 DesiredValue = ResolvedSettings.bForceFP16Pipeline ? 1 : 0;
					if (CVarPropAlpha->GetInt() != DesiredValue)
					{
						CVarPropAlpha->Set(DesiredValue, ECVF_SetByCode);
					}
				}
			}

			// Cache delta time for render thread (temporal adaptation).
			// Clamp to ~66ms (15 fps) so hitches from shader compilation
			// or other stalls don't cause the adaptation to lurch.
			LastDeltaTime = FMath::Min((float)FApp::GetDeltaTime(), 0.066f);

			if (bCachedReplaceTonemap)
			{
				// Disable UE's ACES tone curve, gamut expansion, and blue correction
				// so the LUT is built as a near-identity (white balance + color grading only)
				InView.FinalPostProcessSettings.bOverride_ToneCurveAmount = 1;
				InView.FinalPostProcessSettings.ToneCurveAmount = 0.0f;
				InView.FinalPostProcessSettings.bOverride_ExpandGamut = 1;
				InView.FinalPostProcessSettings.ExpandGamut = 0.0f;
				InView.FinalPostProcessSettings.bOverride_BlueCorrection = 1;
				InView.FinalPostProcessSettings.BlueCorrection = 0.0f;
			}

			// Disable UE's built-in bloom by zeroing its intensity
			if (ResolvedSettings.bDisableUnrealBloom)
			{
				InView.FinalPostProcessSettings.bOverride_BloomIntensity = 1;
				InView.FinalPostProcessSettings.BloomIntensity = 0.0f;
			}

			// Disable UE's built-in motion blur by zeroing the values checked by the renderer
			if (ResolvedSettings.bDisableUnrealMotionBlur)
			{
				InView.FinalPostProcessSettings.bOverride_MotionBlurAmount = 1;
				InView.FinalPostProcessSettings.MotionBlurAmount = 0.0f;
				InView.FinalPostProcessSettings.bOverride_MotionBlurMax = 1;
				InView.FinalPostProcessSettings.MotionBlurMax = 0.0f;
			}

			// Disable UE's built-in auto-exposure for Krawczyk and None modes.
			// Engine Default intentionally keeps UE exposure active (user wants it).
			//
			// We neutralise every path that feeds into PreExposure:
			//   AutoExposureMethod       -> AEM_Manual   (no histogram/basic GPU pass)
			//   AutoExposureBias         -> 0            (pow(2, bias) scales PreExposure)
			//   PhysicalCameraExposure   -> false        (no ISO/aperture influence)
			//   LocalExposure settings   -> neutral      (average feeds back into PreExposure)
			const bool bNeedNeutralExposure = bCachedReplaceTonemap &&
				ResolvedSettings.AutoExposureMode != EToneMapAutoExposure::EngineDefault;

			if (bNeedNeutralExposure)
			{
				InView.FinalPostProcessSettings.bOverride_AutoExposureMethod = 1;
				InView.FinalPostProcessSettings.AutoExposureMethod = AEM_Manual;

				InView.FinalPostProcessSettings.bOverride_AutoExposureBias = 1;
				InView.FinalPostProcessSettings.AutoExposureBias = 0.0f;

				InView.FinalPostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = 1;
				InView.FinalPostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;

				// Neutralise local exposure so its average doesn't feed back into PreExposure
				InView.FinalPostProcessSettings.bOverride_LocalExposureHighlightContrastScale = 1;
				InView.FinalPostProcessSettings.LocalExposureHighlightContrastScale = 1.0f;
				InView.FinalPostProcessSettings.bOverride_LocalExposureShadowContrastScale = 1;
				InView.FinalPostProcessSettings.LocalExposureShadowContrastScale = 1.0f;
				InView.FinalPostProcessSettings.bOverride_LocalExposureHighlightContrastCurve = 1;
				InView.FinalPostProcessSettings.LocalExposureHighlightContrastCurve = nullptr;
				InView.FinalPostProcessSettings.bOverride_LocalExposureShadowContrastCurve = 1;
				InView.FinalPostProcessSettings.LocalExposureShadowContrastCurve = nullptr;
				InView.FinalPostProcessSettings.bOverride_LocalExposureDetailStrength = 1;
				InView.FinalPostProcessSettings.LocalExposureDetailStrength = 1.0f;
			}
}

// ---------------------------------------------------------------------------
// Subscribe to the correct post-process pass
// ---------------------------------------------------------------------------

void FToneMapSceneViewExtension::SubscribeToPostProcessingPass(
	EPostProcessingPass PassId,
	const FSceneView& View,
	FAfterPassCallbackDelegateArray& InOutPassCallbacks,
	bool bIsPassEnabled)
{
	const FSceneViewFamily* Family = View.Family;
	if (!Family) return;

	// Skip non-renderable worlds
	if (Family->Scene && Family->Scene->GetWorld())
	{
		UWorld* World = Family->Scene->GetWorld();
		if (World->WorldType != EWorldType::Game &&
			World->WorldType != EWorldType::Editor &&
			World->WorldType != EWorldType::PIE)
			return;
	}
	if (!Family->EngineShowFlags.PostProcessing) return;
	if (!Family->EngineShowFlags.Rendering || Family->EngineShowFlags.Wireframe) return;

	UToneMapSubsystem* Subsystem = WeakSubsystem.Get();
	if (!Subsystem) return;

	// Determine desired pass from the resolved ToneMap settings for this view.
	EPostProcessingPass DesiredPass = EPostProcessingPass::Tonemap;
	FToneMapResolvedSettings ResolvedSettings;
	UToneMapComponent* DominantComponent = nullptr;
	if (!ResolveToneMapSettings(Subsystem->GetComponents(), View.ViewLocation, ResolvedSettings, DominantComponent))
	{
		return;
	}
	(void)DominantComponent;

	if (ResolvedSettings.Mode == EToneMapMode::ReplaceTonemap)
	{
		// Replace the entire tonemapper
		DesiredPass = EPostProcessingPass::ReplacingTonemapper;
	}
	else
	{
		switch (ResolvedSettings.PostProcessPass)
		{
		case EToneMapPostProcessPass::Tonemap:    DesiredPass = EPostProcessingPass::Tonemap;    break;
		case EToneMapPostProcessPass::MotionBlur: DesiredPass = EPostProcessingPass::MotionBlur; break;
		case EToneMapPostProcessPass::FXAA:                  DesiredPass = EPostProcessingPass::FXAA;                  break;
		case EToneMapPostProcessPass::VisualizeDepthOfField: DesiredPass = EPostProcessingPass::VisualizeDepthOfField; break;
		default:                                               DesiredPass = EPostProcessingPass::Tonemap;               break;
		}
	}

	if (PassId == DesiredPass)
	{
		InOutPassCallbacks.Add(
			FAfterPassCallbackDelegate::CreateRaw(
				this, &FToneMapSceneViewExtension::PostProcessPass_RenderThread));
	}
}

bool FToneMapSceneViewExtension::IsActiveThisFrame_Internal(
	const FSceneViewExtensionContext& Context) const
{
	if (!WeakSubsystem.IsValid()) return false;

	for (const TWeakObjectPtr<UToneMapComponent>& Ptr : WeakSubsystem->GetComponents())
	{
		if (Ptr.IsValid() && Ptr->IsActive() && Ptr->bEnabled) return true;
	}

	return false;
}

// ---------------------------------------------------------------------------
// Main render-thread entry — the full Tone Map pipeline
// ---------------------------------------------------------------------------

FScreenPassTexture FToneMapSceneViewExtension::PostProcessPass_RenderThread(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	const FPostProcessMaterialInputs& Inputs)
{
	check(IsInRenderingThread());

	FScreenPassTexture SceneColor = FScreenPassTexture::CopyFromSlice(
		GraphBuilder, Inputs.GetInput(EPostProcessMaterialInput::SceneColor));
	if (!SceneColor.IsValid()) return SceneColor;

	// Save original SceneColor extent before bloom/lens processing modifies it.
	// The engine allocates scene buffers with QuantizeSceneBufferSize, rounding
	// up to multiples of 8. SMAA computes RTMetrics from the texture extent
	// and expects this quantized value; un-quantized extents cause UV mismatch.
	const FIntPoint OriginalSceneColorExtent = SceneColor.Texture->Desc.Extent;

	const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(View);
	if (ViewInfo.bIsReflectionCapture || ViewInfo.bIsSceneCapture || !ViewInfo.bIsViewInfo)
		return SceneColor;
	if (!ViewInfo.Family->EngineShowFlags.Rendering ||
		!ViewInfo.Family->EngineShowFlags.PostProcessing ||
		ViewInfo.Family->EngineShowFlags.Wireframe)
		return SceneColor;
	if (!ViewInfo.ShaderMap)
		return SceneColor;

	UToneMapSubsystem* Subsystem = WeakSubsystem.Get();
	if (!Subsystem) return SceneColor;

	FToneMapResolvedSettings ResolvedSettings;
	UToneMapComponent* DominantComponent = nullptr;
	if (!ResolveToneMapSettings(Subsystem->GetComponents(), View.ViewLocation, ResolvedSettings, DominantComponent))
	{
		LastHDRRenderDebugState.bValid = false;
		return SceneColor;
	}
	(void)DominantComponent;

	const FToneMapResolvedSettings* ActiveComp = &ResolvedSettings;
	const bool bIsReplaceTonemap = (ActiveComp->Mode == EToneMapMode::ReplaceTonemap);

	RDG_EVENT_SCOPE(GraphBuilder, "ToneMapFX");

	// =====================================================================
	// ClassicBloom Pipeline — runs BEFORE tonemapping
	// In ReplaceTonemap mode: operates on HDR scene color
	// In PostProcess mode:   operates on LDR scene color  
	// =====================================================================
	bool bBloomApplied = false;

	if (ActiveComp->bEnableBloom && ActiveComp->BloomIntensity > 0.0f)
	{
		const FIntPoint SceneColorExtent = SceneColor.Texture->Desc.Extent;
		const FIntRect BloomViewRect = SceneColor.ViewRect;

		if (BloomViewRect.Width() > 0 && BloomViewRect.Height() > 0)
		{
			RDG_EVENT_SCOPE(GraphBuilder, "ClassicBloom");

			// Step 1: Downsample size calculation
			float DownsampleScale = FMath::Clamp(ActiveComp->DownsampleScale, 0.25f, 2.0f);
			int32 Divisor = FMath::Max(1, FMath::RoundToInt(2.0f / DownsampleScale));
			FIntPoint DownsampledExtent = FIntPoint::DivideAndRoundUp(FIntPoint(BloomViewRect.Width(), BloomViewRect.Height()), Divisor);
			FIntRect DownsampledRect = FIntRect(FIntPoint::ZeroValue, DownsampledExtent);

			if (DownsampledRect.Width() > 0 && DownsampledRect.Height() > 0)
			{
				FRDGTextureDesc BrightPassDesc = FRDGTextureDesc::Create2D(
					DownsampledExtent,
					PF_FloatRGBA,
					FClearValueBinding::Black,
					TexCreate_ShaderResource | TexCreate_RenderTargetable);

				// Step 2: Bright pass — extract bright pixels
				FRDGTextureRef BrightPassTexture = GraphBuilder.CreateTexture(BrightPassDesc, TEXT("ClassicBloom.BrightPass"));
				{
					TShaderMapRef<FClassicBloomBrightPassPS> PixelShader(ViewInfo.ShaderMap);
					if (PixelShader.IsValid())
					{
						float EffectiveThreshold = ActiveComp->BloomThreshold;
						bool bIsSoftFocusMode = (ActiveComp->BloomMode == EBloomMode::SoftFocus ||
							ActiveComp->BloomMode == EBloomMode::SoftFocusGlow);
						if (bIsSoftFocusMode)
						{
							EffectiveThreshold = 0.01f;
						}

						FClassicBloomBrightPassPS::FParameters* BPParams = GraphBuilder.AllocParameters<FClassicBloomBrightPassPS::FParameters>();
						BPParams->View = View.ViewUniformBuffer;
						BPParams->SceneColorTexture = SceneColor.Texture;
						BPParams->SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
						BPParams->InputViewportSizeAndInvSize = FVector4f(BloomViewRect.Width(), BloomViewRect.Height(), 1.0f / BloomViewRect.Width(), 1.0f / BloomViewRect.Height());
						BPParams->OutputViewportSizeAndInvSize = FVector4f(DownsampledRect.Width(), DownsampledRect.Height(), 1.0f / DownsampledRect.Width(), 1.0f / DownsampledRect.Height());

						FScreenPassTextureViewport OutputViewport(DownsampledExtent, DownsampledRect);
						FScreenPassTextureViewport InputViewport(SceneColorExtent, SceneColor.ViewRect);
						BPParams->SvPositionToInputTextureUV = (
							FScreenTransform::ChangeTextureBasisFromTo(OutputViewport, FScreenTransform::ETextureBasis::TexelPosition, FScreenTransform::ETextureBasis::ViewportUV) *
							FScreenTransform::ChangeTextureBasisFromTo(InputViewport, FScreenTransform::ETextureBasis::ViewportUV, FScreenTransform::ETextureBasis::TextureUV));

						BPParams->BloomThreshold = EffectiveThreshold;
						BPParams->BloomIntensity = 1.0f;
						BPParams->ThresholdSoftness = FMath::Clamp(ActiveComp->BloomThresholdSoftness, 0.0f, 1.0f);
						BPParams->MaxBrightness = FMath::Max(ActiveComp->BloomMaxBrightness, 0.0f);
						BPParams->RenderTargets[0] = FRenderTargetBinding(BrightPassTexture, ERenderTargetLoadAction::EClear);

						FPixelShaderUtils::AddFullscreenPass(
							GraphBuilder, ViewInfo.ShaderMap,
							RDG_EVENT_NAME("BrightPass"),
							PixelShader, BPParams, DownsampledRect);
					}
				}

				// Step 3: Blur — Gaussian, Directional Glare, or Kawase
				FRDGTextureRef BlurredBloomTexture = nullptr;
				bool bUseSoftFocus = (ActiveComp->BloomMode == EBloomMode::SoftFocus ||
					ActiveComp->BloomMode == EBloomMode::SoftFocusGlow);
				bool bUseSoftFocusGlow = (ActiveComp->BloomMode == EBloomMode::SoftFocusGlow);

				// --- Directional Glare ---
				if (ActiveComp->BloomMode == EBloomMode::DirectionalGlare)
				{
					int32 NumStreaks = FMath::Clamp(ActiveComp->GlareStreakCount, 2, 16);
					float StreakLength = FMath::Clamp((float)ActiveComp->GlareStreakLength, 5.0f, 200.0f);
					float ScaledStreakLength = StreakLength / (float)Divisor;
					float Falloff = FMath::Clamp(ActiveComp->GlareFalloff, 0.5f, 10.0f);
					float AngleStep = 360.0f / (float)NumStreaks;

					TShaderMapRef<FClassicBloomGlareStreakPS> GlareStreakShader(ViewInfo.ShaderMap);
					TShaderMapRef<FClassicBloomGlareAccumulatePS> GlareAccumShader(ViewInfo.ShaderMap);

					if (GlareStreakShader.IsValid() && GlareAccumShader.IsValid())
					{
						TArray<FRDGTextureRef> StreakTextures;
						StreakTextures.Reserve(NumStreaks);

						for (int32 i = 0; i < NumStreaks; ++i)
						{
							float Angle = (AngleStep * (float)i) + ActiveComp->GlareRotationOffset;
							float RadAngle = FMath::DegreesToRadians(Angle);
							FVector2f Direction(FMath::Cos(RadAngle), FMath::Sin(RadAngle));

							FRDGTextureRef StreakTexture = GraphBuilder.CreateTexture(BrightPassDesc, *FString::Printf(TEXT("ClassicBloom.Streak%d"), i));
							StreakTextures.Add(StreakTexture);

							FClassicBloomGlareStreakPS::FParameters* StreakParams = GraphBuilder.AllocParameters<FClassicBloomGlareStreakPS::FParameters>();
							StreakParams->View = View.ViewUniformBuffer;
							StreakParams->SourceTexture = BrightPassTexture;
							StreakParams->SourceSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
							StreakParams->BufferSizeAndInvSize = FVector4f(DownsampledExtent.X, DownsampledExtent.Y, 1.0f / DownsampledExtent.X, 1.0f / DownsampledExtent.Y);
							StreakParams->StreakDirection = Direction;
							StreakParams->StreakLength = ScaledStreakLength;
							StreakParams->StreakFalloff = Falloff;
							StreakParams->StreakSamples = FMath::Clamp(ActiveComp->GlareSamples, 8, 64);
							StreakParams->RenderTargets[0] = FRenderTargetBinding(StreakTexture, ERenderTargetLoadAction::EClear);

							FPixelShaderUtils::AddFullscreenPass(
								GraphBuilder, ViewInfo.ShaderMap,
								RDG_EVENT_NAME("GlareStreak%d", i),
								GlareStreakShader, StreakParams, DownsampledRect);
						}

						// Accumulate streaks
						int32 StreaksToProcess = FMath::Min(NumStreaks, 4);
						FRDGTextureRef AccumTexture = GraphBuilder.CreateTexture(BrightPassDesc, TEXT("ClassicBloom.GlareAccum"));

						FClassicBloomGlareAccumulatePS::FParameters* AccumParams = GraphBuilder.AllocParameters<FClassicBloomGlareAccumulatePS::FParameters>();
						AccumParams->View = View.ViewUniformBuffer;
						AccumParams->StreakTexture0 = StreakTextures[0];
						AccumParams->StreakTexture1 = StreaksToProcess >= 2 ? StreakTextures[1] : StreakTextures[0];
						AccumParams->StreakTexture2 = StreaksToProcess >= 3 ? StreakTextures[2] : StreakTextures[0];
						AccumParams->StreakTexture3 = StreaksToProcess >= 4 ? StreakTextures[3] : StreakTextures[0];
						AccumParams->StreakSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
						AccumParams->GlareViewportSizeAndInvSize = FVector4f(DownsampledRect.Width(), DownsampledRect.Height(), 1.0f / DownsampledRect.Width(), 1.0f / DownsampledRect.Height());
						AccumParams->NumStreaks = StreaksToProcess;
						AccumParams->RenderTargets[0] = FRenderTargetBinding(AccumTexture, ERenderTargetLoadAction::EClear);

						FPixelShaderUtils::AddFullscreenPass(
							GraphBuilder, ViewInfo.ShaderMap,
							RDG_EVENT_NAME("GlareAccumulate"),
							GlareAccumShader, AccumParams, DownsampledRect);

						// Additional batches for >4 streaks
						if (NumStreaks > 4)
						{
							FRDGTextureRef PrevAccum = AccumTexture;
							for (int32 BatchStart = 4; BatchStart < NumStreaks; BatchStart += 3)
							{
								FRDGTextureRef NextAccum = GraphBuilder.CreateTexture(BrightPassDesc, *FString::Printf(TEXT("ClassicBloom.GlareAccum%d"), BatchStart));
								int32 StreaksInBatch = FMath::Min(3, NumStreaks - BatchStart);

								AccumParams = GraphBuilder.AllocParameters<FClassicBloomGlareAccumulatePS::FParameters>();
								AccumParams->View = View.ViewUniformBuffer;
								AccumParams->StreakTexture0 = PrevAccum;
								AccumParams->StreakTexture1 = StreakTextures[BatchStart];
								AccumParams->StreakTexture2 = StreaksInBatch >= 2 ? StreakTextures[BatchStart + 1] : StreakTextures[BatchStart];
								AccumParams->StreakTexture3 = StreaksInBatch >= 3 ? StreakTextures[BatchStart + 2] : StreakTextures[BatchStart];
								AccumParams->StreakSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
								AccumParams->GlareViewportSizeAndInvSize = FVector4f(DownsampledRect.Width(), DownsampledRect.Height(), 1.0f / DownsampledRect.Width(), 1.0f / DownsampledRect.Height());
								AccumParams->NumStreaks = 1 + StreaksInBatch;
								AccumParams->RenderTargets[0] = FRenderTargetBinding(NextAccum, ERenderTargetLoadAction::EClear);

								FPixelShaderUtils::AddFullscreenPass(
									GraphBuilder, ViewInfo.ShaderMap,
									RDG_EVENT_NAME("GlareAccumulate%d", BatchStart),
									GlareAccumShader, AccumParams, DownsampledRect);

								PrevAccum = NextAccum;
							}
							AccumTexture = PrevAccum;
						}

						// Light Gaussian blur to smooth the glare
						FRDGTextureRef GlareBlurTemp = GraphBuilder.CreateTexture(BrightPassDesc, TEXT("ClassicBloom.GlareBlurTemp"));
						BlurredBloomTexture = GraphBuilder.CreateTexture(BrightPassDesc, TEXT("ClassicBloom.GlareBlurred"));

						TShaderMapRef<FClassicBloomBlurPS> BlurShader(ViewInfo.ShaderMap);

						// Horizontal blur
						{
							FClassicBloomBlurPS::FParameters* BlurParams = GraphBuilder.AllocParameters<FClassicBloomBlurPS::FParameters>();
							BlurParams->View = View.ViewUniformBuffer;
							BlurParams->SourceTexture = AccumTexture;
							BlurParams->SourceSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
							BlurParams->BufferSizeAndInvSize = FVector4f(DownsampledExtent.X, DownsampledExtent.Y, 1.0f / DownsampledExtent.X, 1.0f / DownsampledExtent.Y);
							BlurParams->BlurDirection = FVector2f(1.0f, 0.0f);
							BlurParams->BlurRadius = ActiveComp->BloomSize * 0.05f;
							BlurParams->RenderTargets[0] = FRenderTargetBinding(GlareBlurTemp, ERenderTargetLoadAction::EClear);

							FPixelShaderUtils::AddFullscreenPass(GraphBuilder, ViewInfo.ShaderMap, RDG_EVENT_NAME("GlareBlurH"), BlurShader, BlurParams, DownsampledRect);
						}

						// Vertical blur
						{
							FClassicBloomBlurPS::FParameters* BlurParams = GraphBuilder.AllocParameters<FClassicBloomBlurPS::FParameters>();
							BlurParams->View = View.ViewUniformBuffer;
							BlurParams->SourceTexture = GlareBlurTemp;
							BlurParams->SourceSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
							BlurParams->BufferSizeAndInvSize = FVector4f(DownsampledExtent.X, DownsampledExtent.Y, 1.0f / DownsampledExtent.X, 1.0f / DownsampledExtent.Y);
							BlurParams->BlurDirection = FVector2f(0.0f, 1.0f);
							BlurParams->BlurRadius = ActiveComp->BloomSize * 0.05f;
							BlurParams->RenderTargets[0] = FRenderTargetBinding(BlurredBloomTexture, ERenderTargetLoadAction::EClear);

							FPixelShaderUtils::AddFullscreenPass(GraphBuilder, ViewInfo.ShaderMap, RDG_EVENT_NAME("GlareBlurV"), BlurShader, BlurParams, DownsampledRect);
						}
					}
				}

				// --- Kawase Bloom ---
				if (ActiveComp->BloomMode == EBloomMode::Kawase && !BlurredBloomTexture)
				{
					TShaderMapRef<FClassicBloomKawaseDownsamplePS> KawaseDownsampleShader(ViewInfo.ShaderMap);
					TShaderMapRef<FClassicBloomKawaseUpsamplePS> KawaseUpsampleShader(ViewInfo.ShaderMap);

					if (KawaseDownsampleShader.IsValid() && KawaseUpsampleShader.IsValid())
					{
						int32 MipCount = FMath::Clamp(ActiveComp->KawaseMipCount, 3, 8);
						float FilterRadius = FMath::Clamp(ActiveComp->KawaseFilterRadius, 0.0001f, 0.01f);
						float ThresholdKnee = ActiveComp->bKawaseSoftThreshold ? FMath::Clamp(ActiveComp->KawaseThresholdKnee, 0.0f, 1.0f) : 0.0f;

						TArray<FRDGTextureRef> MipTextures;
						TArray<FIntPoint> MipExtents;
						TArray<FIntRect> MipRects;
						MipTextures.Reserve(MipCount);
						MipExtents.Reserve(MipCount);
						MipRects.Reserve(MipCount);

						FIntPoint CurrentExtent = DownsampledExtent;
						FIntRect CurrentRect = DownsampledRect;

						for (int32 Mip = 0; Mip < MipCount; ++Mip)
						{
							CurrentExtent = FIntPoint::DivideAndRoundUp(CurrentExtent, 2);
							CurrentRect = FIntRect(FIntPoint::ZeroValue, FIntPoint::DivideAndRoundUp(FIntPoint(CurrentRect.Width(), CurrentRect.Height()), 2));
							CurrentExtent.X = FMath::Max(CurrentExtent.X, 1);
							CurrentExtent.Y = FMath::Max(CurrentExtent.Y, 1);
							CurrentRect.Max.X = FMath::Max(CurrentRect.Max.X, 1);
							CurrentRect.Max.Y = FMath::Max(CurrentRect.Max.Y, 1);

							FRDGTextureDesc MipDesc = FRDGTextureDesc::Create2D(
								CurrentExtent, PF_FloatRGBA, FClearValueBinding::Black,
								TexCreate_ShaderResource | TexCreate_RenderTargetable);

							MipTextures.Add(GraphBuilder.CreateTexture(MipDesc, *FString::Printf(TEXT("ClassicBloom.KawaseMip%d"), Mip)));
							MipExtents.Add(CurrentExtent);
							MipRects.Add(CurrentRect);
						}

						// Downsample pass: create mip pyramid from scene color
						FRDGTextureRef DownsampleSource = SceneColor.Texture;
						FIntPoint SourceExtent = SceneColorExtent;
						FIntRect SourceRect = SceneColor.ViewRect;

						for (int32 Mip = 0; Mip < MipCount; ++Mip)
						{
							FClassicBloomKawaseDownsamplePS::FParameters* DownParams = GraphBuilder.AllocParameters<FClassicBloomKawaseDownsamplePS::FParameters>();
							DownParams->View = View.ViewUniformBuffer;
							DownParams->SourceTexture = DownsampleSource;
							DownParams->SourceSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
							DownParams->SourceSizeAndInvSize = FVector4f(SourceExtent.X, SourceExtent.Y, 1.0f / SourceExtent.X, 1.0f / SourceExtent.Y);
							DownParams->OutputSizeAndInvSize = FVector4f(MipExtents[Mip].X, MipExtents[Mip].Y, 1.0f / MipExtents[Mip].X, 1.0f / MipExtents[Mip].Y);

							FScreenPassTextureViewport OutVP(MipExtents[Mip], MipRects[Mip]);
							FScreenPassTextureViewport SrcVP(SourceExtent, SourceRect);
							DownParams->SvPositionToSourceUV = (
								FScreenTransform::ChangeTextureBasisFromTo(OutVP, FScreenTransform::ETextureBasis::TexelPosition, FScreenTransform::ETextureBasis::ViewportUV) *
								FScreenTransform::ChangeTextureBasisFromTo(SrcVP, FScreenTransform::ETextureBasis::ViewportUV, FScreenTransform::ETextureBasis::TextureUV));

							DownParams->BloomThreshold = ActiveComp->BloomThreshold;
							DownParams->ThresholdKnee = ThresholdKnee;
							DownParams->MaxBrightness = FMath::Max(ActiveComp->BloomMaxBrightness, 0.0f);
							DownParams->MipLevel = Mip;
							DownParams->bUseKarisAverage = (Mip == 0) ? 1 : 0;
							DownParams->RenderTargets[0] = FRenderTargetBinding(MipTextures[Mip], ERenderTargetLoadAction::EClear);

							FPixelShaderUtils::AddFullscreenPass(
								GraphBuilder, ViewInfo.ShaderMap,
								RDG_EVENT_NAME("KawaseDownsample_Mip%d", Mip),
								KawaseDownsampleShader, DownParams, MipRects[Mip]);

							DownsampleSource = MipTextures[Mip];
							SourceExtent = MipExtents[Mip];
							SourceRect = MipRects[Mip];
						}

						// Upsample pass: progressive upsample with additive blend
						TArray<FRDGTextureRef> UpsampleTextures;
						UpsampleTextures.Reserve(MipCount - 1);

						for (int32 Mip = MipCount - 2; Mip >= 0; --Mip)
						{
							FRDGTextureDesc UpsampleDesc = FRDGTextureDesc::Create2D(
								MipExtents[Mip], PF_FloatRGBA, FClearValueBinding::Black,
								TexCreate_ShaderResource | TexCreate_RenderTargetable);
							UpsampleTextures.Add(GraphBuilder.CreateTexture(UpsampleDesc, *FString::Printf(TEXT("ClassicBloom.KawaseUpsample%d"), Mip)));
						}

						FRDGTextureRef UpsampleSource = MipTextures[MipCount - 1];
						int32 UpsampleIdx = 0;
						for (int32 Mip = MipCount - 2; Mip >= 0; --Mip)
						{
							FClassicBloomKawaseUpsamplePS::FParameters* UpParams = GraphBuilder.AllocParameters<FClassicBloomKawaseUpsamplePS::FParameters>();
							UpParams->View = View.ViewUniformBuffer;
							UpParams->SourceTexture = UpsampleSource;
							UpParams->PreviousMipTexture = MipTextures[Mip];
							UpParams->SourceSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
							UpParams->OutputSizeAndInvSize = FVector4f(MipExtents[Mip].X, MipExtents[Mip].Y, 1.0f / MipExtents[Mip].X, 1.0f / MipExtents[Mip].Y);
							UpParams->FilterRadius = FilterRadius;
							UpParams->MaxBrightness = FMath::Max(ActiveComp->BloomMaxBrightness, 0.0f);
							UpParams->RenderTargets[0] = FRenderTargetBinding(UpsampleTextures[UpsampleIdx], ERenderTargetLoadAction::EClear);

							FPixelShaderUtils::AddFullscreenPass(
								GraphBuilder, ViewInfo.ShaderMap,
								RDG_EVENT_NAME("KawaseUpsample_Mip%d", Mip),
								KawaseUpsampleShader, UpParams, MipRects[Mip]);

							UpsampleSource = UpsampleTextures[UpsampleIdx];
							++UpsampleIdx;
						}

						// Final upsample to original downsampled size
						if (UpsampleTextures.Num() > 0)
						{
							BlurredBloomTexture = GraphBuilder.CreateTexture(BrightPassDesc, TEXT("ClassicBloom.KawaseBlurred"));

							FClassicBloomKawaseUpsamplePS::FParameters* FinalUpParams = GraphBuilder.AllocParameters<FClassicBloomKawaseUpsamplePS::FParameters>();
							FinalUpParams->View = View.ViewUniformBuffer;
							FinalUpParams->SourceTexture = UpsampleTextures.Last();
							FinalUpParams->PreviousMipTexture = MipTextures[0];
							FinalUpParams->SourceSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
							FinalUpParams->OutputSizeAndInvSize = FVector4f(DownsampledExtent.X, DownsampledExtent.Y, 1.0f / DownsampledExtent.X, 1.0f / DownsampledExtent.Y);
							FinalUpParams->FilterRadius = FilterRadius;
							FinalUpParams->MaxBrightness = FMath::Max(ActiveComp->BloomMaxBrightness, 0.0f);
							FinalUpParams->RenderTargets[0] = FRenderTargetBinding(BlurredBloomTexture, ERenderTargetLoadAction::EClear);

							FPixelShaderUtils::AddFullscreenPass(
								GraphBuilder, ViewInfo.ShaderMap,
								RDG_EVENT_NAME("KawaseUpsample_Final"),
								KawaseUpsampleShader, FinalUpParams, DownsampledRect);
						}
						else
						{
							BlurredBloomTexture = MipTextures.Num() > 0 ? MipTextures[0] : BrightPassTexture;
						}
					}
				}

				// --- Standard Gaussian blur (or fallback) ---
				if (!BlurredBloomTexture)
				{
					int32 NumBlurPasses = FMath::Clamp(ActiveComp->BlurPasses, 1, 4);
					FRDGTextureRef BlurSource = BrightPassTexture;
					FRDGTextureRef BlurTempTexture = GraphBuilder.CreateTexture(BrightPassDesc, TEXT("ClassicBloom.BlurTemp"));
					BlurredBloomTexture = GraphBuilder.CreateTexture(BrightPassDesc, TEXT("ClassicBloom.Blurred"));

					TShaderMapRef<FClassicBloomBlurPS> BlurShader(ViewInfo.ShaderMap);
					for (int32 PassIndex = 0; PassIndex < NumBlurPasses; ++PassIndex)
					{
						// Horizontal
						{
							FClassicBloomBlurPS::FParameters* BlurParams = GraphBuilder.AllocParameters<FClassicBloomBlurPS::FParameters>();
							BlurParams->View = View.ViewUniformBuffer;
							BlurParams->SourceTexture = BlurSource;
							BlurParams->SourceSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
							BlurParams->BufferSizeAndInvSize = FVector4f(DownsampledExtent.X, DownsampledExtent.Y, 1.0f / DownsampledExtent.X, 1.0f / DownsampledExtent.Y);
							BlurParams->BlurDirection = FVector2f(1.0f, 0.0f);
							BlurParams->BlurRadius = ActiveComp->BloomSize * 0.1f;
							BlurParams->RenderTargets[0] = FRenderTargetBinding(BlurTempTexture, ERenderTargetLoadAction::EClear);

							FPixelShaderUtils::AddFullscreenPass(GraphBuilder, ViewInfo.ShaderMap, RDG_EVENT_NAME("BlurHorizontal"), BlurShader, BlurParams, DownsampledRect);
						}

						// Vertical
						{
							FClassicBloomBlurPS::FParameters* BlurParams = GraphBuilder.AllocParameters<FClassicBloomBlurPS::FParameters>();
							BlurParams->View = View.ViewUniformBuffer;
							BlurParams->SourceTexture = BlurTempTexture;
							BlurParams->SourceSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
							BlurParams->BufferSizeAndInvSize = FVector4f(DownsampledExtent.X, DownsampledExtent.Y, 1.0f / DownsampledExtent.X, 1.0f / DownsampledExtent.Y);
							BlurParams->BlurDirection = FVector2f(0.0f, 1.0f);
							BlurParams->BlurRadius = ActiveComp->BloomSize * 0.1f;
							BlurParams->RenderTargets[0] = FRenderTargetBinding(BlurredBloomTexture, ERenderTargetLoadAction::EClear);

							FPixelShaderUtils::AddFullscreenPass(GraphBuilder, ViewInfo.ShaderMap, RDG_EVENT_NAME("BlurVertical"), BlurShader, BlurParams, DownsampledRect);
						}

						BlurSource = BlurredBloomTexture;
					}
				}

				// Step 4: Composite bloom back onto scene color
				if (BlurredBloomTexture)
				{
					FRDGTextureDesc CompositeDesc = SceneColor.Texture->Desc;
					CompositeDesc.ClearValue = FClearValueBinding::Black;
					CompositeDesc.Flags |= TexCreate_RenderTargetable | TexCreate_ShaderResource;
					FRDGTextureRef CompositeOutput = GraphBuilder.CreateTexture(CompositeDesc, TEXT("ClassicBloom.Composite"));
					FIntRect CompositeViewRect = SceneColor.ViewRect;

					TShaderMapRef<FClassicBloomCompositePS> CompositeShader(ViewInfo.ShaderMap);
					if (CompositeShader.IsValid())
					{
						FClassicBloomCompositePS::FParameters* CParams = GraphBuilder.AllocParameters<FClassicBloomCompositePS::FParameters>();
						CParams->View = View.ViewUniformBuffer;
						CParams->SceneColorTexture = SceneColor.Texture;
						CParams->SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
						CParams->BloomTexture = BlurredBloomTexture;
						CParams->BloomSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
						CParams->OutputViewportSizeAndInvSize = FVector4f(CompositeViewRect.Width(), CompositeViewRect.Height(), 1.0f / CompositeViewRect.Width(), 1.0f / CompositeViewRect.Height());

						FScreenPassTextureViewport CompositeOutputVP(CompositeDesc.Extent, CompositeViewRect);
						FScreenPassTextureViewport SceneColorInputVP(SceneColorExtent, SceneColor.ViewRect);
						FScreenPassTextureViewport BloomVP(DownsampledExtent, DownsampledRect);

						CParams->SvPositionToSceneColorUV = (
							FScreenTransform::ChangeTextureBasisFromTo(CompositeOutputVP, FScreenTransform::ETextureBasis::TexelPosition, FScreenTransform::ETextureBasis::ViewportUV) *
							FScreenTransform::ChangeTextureBasisFromTo(SceneColorInputVP, FScreenTransform::ETextureBasis::ViewportUV, FScreenTransform::ETextureBasis::TextureUV));

						CParams->SvPositionToBloomUV = (
							FScreenTransform::ChangeTextureBasisFromTo(CompositeOutputVP, FScreenTransform::ETextureBasis::TexelPosition, FScreenTransform::ETextureBasis::ViewportUV) *
							FScreenTransform::ChangeTextureBasisFromTo(BloomVP, FScreenTransform::ETextureBasis::ViewportUV, FScreenTransform::ETextureBasis::TextureUV));

						CParams->BloomIntensity = bUseSoftFocus ? 0.0f : ActiveComp->BloomIntensity;

						CParams->BloomTint = FVector4f(ActiveComp->BloomTint);

						CParams->BloomColorMode = static_cast<float>(static_cast<uint8>(ActiveComp->BloomColorMode));
						CParams->BloomBlendMode = (float)(bUseSoftFocusGlow
							? EBloomBlendMode::SoftLightGlow
							: ActiveComp->BloomBlendMode);
						CParams->BloomBlendStrength = ActiveComp->BloomBlendStrength;
						CParams->BloomSaturation = ActiveComp->BloomSaturation;
						CParams->bProtectHighlights = ActiveComp->bProtectHighlights ? 1.0f : 0.0f;
						CParams->HighlightProtection = ActiveComp->HighlightProtection;
						CParams->SoftFocusIntensity = bUseSoftFocus ? ActiveComp->BloomIntensity : 0.0f;
						CParams->SoftFocusParams = FVector4f(
							ActiveComp->SoftFocusOverlayMultiplier,
							ActiveComp->SoftFocusBlendStrength,
							ActiveComp->SoftFocusSoftLightMultiplier,
							ActiveComp->SoftFocusFinalBlend);

						// Removed debug options — set safe defaults
						CParams->bUseAdaptiveScaling = 0.0f;
						CParams->bShowBloomOnly = 0.0f;
						CParams->bShowGammaCompensation = 0.0f;
						CParams->bIsGameWorld = (ViewInfo.Family->Scene && ViewInfo.Family->Scene->GetWorld() && ViewInfo.Family->Scene->GetWorld()->IsGameWorld()) ? 1.0f : 0.0f;
						CParams->GameModeBloomScale = 1.0f;

						CParams->bUseBrightnessCompensation = 0.0f;
						CParams->BrightnessCompensationMode = 0.0f;
						CParams->BrightnessCompensationStrength = 0.0f;
						CParams->ExposureCompensation = 0.0f;

						// EClear ensures pixels outside CompositeViewRect are black
						// (texture extent may be larger than viewport when window is not maximized)
						CParams->RenderTargets[0] = FRenderTargetBinding(CompositeOutput, ERenderTargetLoadAction::EClear);

						FPixelShaderUtils::AddFullscreenPass(
							GraphBuilder, ViewInfo.ShaderMap,
							RDG_EVENT_NAME("CompositeBloom"),
							CompositeShader, CParams, CompositeViewRect);

						// Replace SceneColor with the bloom-composited result for downstream ToneMap processing
						SceneColor = FScreenPassTexture(CompositeOutput, CompositeViewRect);
						bBloomApplied = true;
					}
				}
			} // DownsampledRect valid
		} // BloomViewRect valid
	} // Bloom active

	// bIsReplaceTonemap already determined above

	const FScreenPassTextureViewport SceneColorViewport(SceneColor);
	const FIntPoint ViewportSize = SceneColorViewport.Rect.Size();

	// =====================================================================
	// Get bloom texture (ReplaceTonemap mode only, skipped if ClassicBloom already composited)
	// =====================================================================
	FScreenPassTexture BloomInput;
	if (bIsReplaceTonemap && !bBloomApplied)
	{
		FScreenPassTextureSlice BloomSlice = Inputs.GetInput(EPostProcessMaterialInput::CombinedBloom);
		if (BloomSlice.IsValid())
		{
			BloomInput = FScreenPassTexture::CopyFromSlice(GraphBuilder, BloomSlice);
		}
	}

	// =====================================================================
	// Krawczyk Auto-Exposure — Luminance measurement & temporal adaptation
	// =====================================================================
	// Only runs when mode is Krawczyk and we're in ReplaceTonemap mode.
	// Pipeline:
	//   1. LuminanceMeasurePS  — 16x16 grid sampling → geometric mean (1x1)
	//   2. LuminanceAdaptPS    — Exponential blend with previous frame (1x1)
	//   3. Result passed to main shader as AdaptedLumTexture
	// =====================================================================

	const bool bNeedKrawczyk = bIsReplaceTonemap &&
		(ActiveComp->AutoExposureMode == EToneMapAutoExposure::Krawczyk);

	FRDGTextureRef AdaptedLumTexture = nullptr;

	if (bNeedKrawczyk)
	{
		const float OneOverPreExposure = 1.0f / FMath::Max(ViewInfo.PreExposure, 0.001f);

		// Compute scene color UV bounds (viewport rect in texture UV space)
		const FIntRect& SceneVR = SceneColorViewport.Rect;
		const FIntPoint SceneExt = SceneColor.Texture->Desc.Extent;
		FVector4f UVBounds(
			(float)SceneVR.Min.X / SceneExt.X,
			(float)SceneVR.Min.Y / SceneExt.Y,
			(float)SceneVR.Max.X / SceneExt.X,
			(float)SceneVR.Max.Y / SceneExt.Y);

		// --- Step 1: Measure scene luminance (→ 1x1 texture) ---
		FRDGTextureRef MeasuredLumTexture;
		{
			FRDGTextureDesc LumDesc = FRDGTextureDesc::Create2D(
				FIntPoint(1, 1), PF_R32_FLOAT, FClearValueBinding::None,
				TexCreate_ShaderResource | TexCreate_RenderTargetable);

			MeasuredLumTexture = GraphBuilder.CreateTexture(LumDesc, TEXT("ToneMap.MeasuredLum"));

			auto* P = GraphBuilder.AllocParameters<FToneMapLumMeasurePS::FParameters>();
			P->View                = ViewInfo.ViewUniformBuffer;
			P->SceneColorTexture   = SceneColor.Texture;
			P->SceneColorSampler   = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			P->SceneColorUVBounds  = UVBounds;
			P->OneOverPreExposure  = OneOverPreExposure;
			P->RenderTargets[0]    = FRenderTargetBinding(MeasuredLumTexture, ERenderTargetLoadAction::ENoAction);

			TShaderMapRef<FToneMapLumMeasurePS> MeasureShader(ViewInfo.ShaderMap);
			FPixelShaderUtils::AddFullscreenPass(
				GraphBuilder, ViewInfo.ShaderMap,
				RDG_EVENT_NAME("ToneMap_LuminanceMeasure"),
				MeasureShader, P,
				FIntRect(0, 0, 1, 1));
		}

		// --- Step 2: Temporal adaptation (→ 1x1 persistent texture) ---
		if (AdaptedLuminanceRT.IsValid())
		{
			// Blend previous adapted luminance with new measurement
			FRDGTextureRef PrevAdaptedLum = GraphBuilder.RegisterExternalTexture(
				AdaptedLuminanceRT, TEXT("ToneMap.PrevAdaptedLum"));

			FRDGTextureDesc AdaptDesc = FRDGTextureDesc::Create2D(
				FIntPoint(1, 1), PF_R32_FLOAT, FClearValueBinding::None,
				TexCreate_ShaderResource | TexCreate_RenderTargetable);

			AdaptedLumTexture = GraphBuilder.CreateTexture(AdaptDesc, TEXT("ToneMap.AdaptedLum"));

			auto* P = GraphBuilder.AllocParameters<FToneMapLumAdaptPS::FParameters>();
			P->View                   = ViewInfo.ViewUniformBuffer;
			P->PrevAdaptedLumTexture   = PrevAdaptedLum;
			P->PrevAdaptedLumSampler   = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			P->CurrentLumTexture       = MeasuredLumTexture;
			P->CurrentLumSampler       = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			P->AdaptSpeedUp            = ActiveComp->AdaptationSpeedUp;
			P->AdaptSpeedDown          = ActiveComp->AdaptationSpeedDown;
			P->DeltaTime               = FMath::Max(LastDeltaTime, 0.001f);
			P->RenderTargets[0]        = FRenderTargetBinding(AdaptedLumTexture, ERenderTargetLoadAction::ENoAction);

			TShaderMapRef<FToneMapLumAdaptPS> AdaptShader(ViewInfo.ShaderMap);
			FPixelShaderUtils::AddFullscreenPass(
				GraphBuilder, ViewInfo.ShaderMap,
				RDG_EVENT_NAME("ToneMap_LuminanceAdapt"),
				AdaptShader, P,
				FIntRect(0, 0, 1, 1));
		}
		else
		{
			// First frame: use measured luminance directly (instant adaptation)
			AdaptedLumTexture = MeasuredLumTexture;
		}

		// Extract adapted luminance for next frame's temporal blending
		GraphBuilder.QueueTextureExtraction(AdaptedLumTexture, &AdaptedLuminanceRT);
	}

	// =====================================================================
	// Clarity blur passes (skip when Clarity == 0 for performance)
	// =====================================================================

	FRDGTextureRef BlurredTexture = SceneColor.Texture; // default: no blur

	if (FMath::Abs(ActiveComp->Clarity) > 0.01f)
	{
		FRDGTextureDesc BlurDesc = FRDGTextureDesc::Create2D(
			ViewportSize, PF_FloatRGBA, FClearValueBinding::None,
			TexCreate_ShaderResource | TexCreate_RenderTargetable);

		// --- Horizontal blur ---
		FRDGTextureRef HBlurTexture = GraphBuilder.CreateTexture(BlurDesc, TEXT("ToneMap.HBlur"));
		{
			auto* P = GraphBuilder.AllocParameters<FToneMapBlurPS::FParameters>();
			P->View            = ViewInfo.ViewUniformBuffer;
			P->SourceTexture   = SceneColor.Texture;
			P->SourceSampler   = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			P->BufferSizeAndInvSize = FVector4f(
				ViewportSize.X, ViewportSize.Y,
				1.0f / ViewportSize.X, 1.0f / ViewportSize.Y);
			P->BlurDirection   = FVector2f(1.0f, 0.0f);
			P->BlurRadius      = ActiveComp->ClarityRadius;
			const FIntRect& SceneVR = SceneColorViewport.Rect;
			const FIntPoint SceneExt = SceneColor.Texture->Desc.Extent;
			P->SourceViewportRect = FVector4f(
				(float)SceneVR.Min.X, (float)SceneVR.Min.Y,
				(float)SceneVR.Max.X, (float)SceneVR.Max.Y);
			P->SourceExtentInv = FVector4f(
				1.0f / SceneExt.X, 1.0f / SceneExt.Y, 0.0f, 0.0f);
			P->RenderTargets[0] = FRenderTargetBinding(HBlurTexture, ERenderTargetLoadAction::ENoAction);

			TShaderMapRef<FToneMapBlurPS> BlurShaderH(ViewInfo.ShaderMap);
			FPixelShaderUtils::AddFullscreenPass(
				GraphBuilder, ViewInfo.ShaderMap,
				RDG_EVENT_NAME("ToneMap_HBlur"),
				BlurShaderH, P,
				FIntRect(0, 0, ViewportSize.X, ViewportSize.Y));
		}

		// --- Vertical blur ---
		FRDGTextureRef VBlurTexture = GraphBuilder.CreateTexture(BlurDesc, TEXT("ToneMap.VBlur"));
		{
			auto* P = GraphBuilder.AllocParameters<FToneMapBlurPS::FParameters>();
			P->View            = ViewInfo.ViewUniformBuffer;
			P->SourceTexture   = HBlurTexture;
			P->SourceSampler   = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			P->BufferSizeAndInvSize = FVector4f(
				ViewportSize.X, ViewportSize.Y,
				1.0f / ViewportSize.X, 1.0f / ViewportSize.Y);
			P->BlurDirection   = FVector2f(0.0f, 1.0f);
			P->BlurRadius      = ActiveComp->ClarityRadius;
			P->SourceViewportRect = FVector4f(
				0.0f, 0.0f, (float)ViewportSize.X, (float)ViewportSize.Y);
			P->SourceExtentInv = FVector4f(
				1.0f / ViewportSize.X, 1.0f / ViewportSize.Y, 0.0f, 0.0f);
			P->RenderTargets[0] = FRenderTargetBinding(VBlurTexture, ERenderTargetLoadAction::ENoAction);

			TShaderMapRef<FToneMapBlurPS> BlurShaderV(ViewInfo.ShaderMap);
			FPixelShaderUtils::AddFullscreenPass(
				GraphBuilder, ViewInfo.ShaderMap,
				RDG_EVENT_NAME("ToneMap_VBlur"),
				BlurShaderV, P,
				FIntRect(0, 0, ViewportSize.X, ViewportSize.Y));
		}

		BlurredTexture = VBlurTexture;
	}

	// =====================================================================
	// Dynamic Contrast blur passes — Fine (radius 2) and Coarse (radius 32)
	// Generates 3-scale blur pyramid for multi-scale local contrast.
	// Skipped entirely when all Dynamic Contrast sliders are zero.
	// =====================================================================

	const bool bNeedDynamicContrastBlurs =
		(ActiveComp->DynamicContrast > 0.01f ||
		 ActiveComp->CorrectContrast > 0.01f ||
		 ActiveComp->CorrectColorCast > 0.01f);

	FRDGTextureRef BlurredFineTexture  = SceneColor.Texture;  // fallback
	FRDGTextureRef BlurredCoarseTexture = SceneColor.Texture;  // fallback

	if (bNeedDynamicContrastBlurs)
	{
		FRDGTextureDesc BlurDesc = FRDGTextureDesc::Create2D(
			ViewportSize, PF_FloatRGBA, FClearValueBinding::None,
			TexCreate_ShaderResource | TexCreate_RenderTargetable);

		const FIntRect& SceneVR = SceneColorViewport.Rect;
		const FIntPoint SceneExt = SceneColor.Texture->Desc.Extent;

		// Helper lambda: run a separable H+V Gaussian blur pass pair
		auto RunBlurPair = [&](FRDGTextureRef InputTexture,
			const FVector4f& InputViewportRect, const FVector4f& InputExtentInv,
			const FIntPoint& InputSize, float Radius,
			const TCHAR* NameH, const TCHAR* NameV,
			const TCHAR* TexNameH, const TCHAR* TexNameV) -> FRDGTextureRef
		{
			// H pass
			FRDGTextureRef HTex = GraphBuilder.CreateTexture(BlurDesc, TexNameH);
			{
				auto* P = GraphBuilder.AllocParameters<FToneMapBlurPS::FParameters>();
				P->View              = ViewInfo.ViewUniformBuffer;
				P->SourceTexture     = InputTexture;
				P->SourceSampler     = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
				P->BufferSizeAndInvSize = FVector4f(
					InputSize.X, InputSize.Y,
					1.0f / InputSize.X, 1.0f / InputSize.Y);
				P->BlurDirection     = FVector2f(1.0f, 0.0f);
				P->BlurRadius        = Radius;
				P->SourceViewportRect = InputViewportRect;
				P->SourceExtentInv   = InputExtentInv;
				P->RenderTargets[0]  = FRenderTargetBinding(HTex, ERenderTargetLoadAction::ENoAction);

				TShaderMapRef<FToneMapBlurPS> Shader(ViewInfo.ShaderMap);
				FPixelShaderUtils::AddFullscreenPass(
					GraphBuilder, ViewInfo.ShaderMap,
					FRDGEventName(NameH), Shader, P,
					FIntRect(0, 0, ViewportSize.X, ViewportSize.Y));
			}

			// V pass
			FRDGTextureRef VTex = GraphBuilder.CreateTexture(BlurDesc, TexNameV);
			{
				auto* P = GraphBuilder.AllocParameters<FToneMapBlurPS::FParameters>();
				P->View              = ViewInfo.ViewUniformBuffer;
				P->SourceTexture     = HTex;
				P->SourceSampler     = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
				P->BufferSizeAndInvSize = FVector4f(
					ViewportSize.X, ViewportSize.Y,
					1.0f / ViewportSize.X, 1.0f / ViewportSize.Y);
				P->BlurDirection     = FVector2f(0.0f, 1.0f);
				P->BlurRadius        = Radius;
				P->SourceViewportRect = FVector4f(0.0f, 0.0f, (float)ViewportSize.X, (float)ViewportSize.Y);
				P->SourceExtentInv   = FVector4f(1.0f / ViewportSize.X, 1.0f / ViewportSize.Y, 0.0f, 0.0f);
				P->RenderTargets[0]  = FRenderTargetBinding(VTex, ERenderTargetLoadAction::ENoAction);

				TShaderMapRef<FToneMapBlurPS> Shader(ViewInfo.ShaderMap);
				FPixelShaderUtils::AddFullscreenPass(
					GraphBuilder, ViewInfo.ShaderMap,
					FRDGEventName(NameV), Shader, P,
					FIntRect(0, 0, ViewportSize.X, ViewportSize.Y));
			}

			return VTex;
		};

		// Fine blur: radius 2 — captures high-frequency surface detail
		BlurredFineTexture = RunBlurPair(
			SceneColor.Texture,
			FVector4f((float)SceneVR.Min.X, (float)SceneVR.Min.Y,
			          (float)SceneVR.Max.X, (float)SceneVR.Max.Y),
			FVector4f(1.0f / SceneExt.X, 1.0f / SceneExt.Y, 0.0f, 0.0f),
			ViewportSize, 2.0f,
			TEXT("ToneMap_DynamicContrast_FineH"), TEXT("ToneMap_DynamicContrast_FineV"),
			TEXT("ToneMap.DynamicContrast.FineH"), TEXT("ToneMap.DynamicContrast.FineV"));

		// Coarse blur: radius 32 — captures large-scale tonal structure
		BlurredCoarseTexture = RunBlurPair(
			SceneColor.Texture,
			FVector4f((float)SceneVR.Min.X, (float)SceneVR.Min.Y,
			          (float)SceneVR.Max.X, (float)SceneVR.Max.Y),
			FVector4f(1.0f / SceneExt.X, 1.0f / SceneExt.Y, 0.0f, 0.0f),
			ViewportSize, 32.0f,
			TEXT("ToneMap_DynamicContrast_CoarseH"), TEXT("ToneMap_DynamicContrast_CoarseV"),
			TEXT("ToneMap.DynamicContrast.CoarseH"), TEXT("ToneMap.DynamicContrast.CoarseV"));
	}

	// =====================================================================
	// Durand-Dorsey 2002 Bilateral Tone Mapping — pre-pass
	// Runs before ToneMapProcess; sets bPreToneMapped so the film curve is skipped.
	// =====================================================================
	FRDGTextureRef PreToneMappedTexture = nullptr;
	bool bPreToneMapped = false;

	if (bIsReplaceTonemap && ActiveComp->FilmCurve == EToneMapFilmCurve::Durand)
	{
		RDG_EVENT_SCOPE(GraphBuilder, "ToneMapFX_Durand");

		const FIntPoint WS = ViewportSize;
		const FVector4f BilateralBufferSize((float)WS.X, (float)WS.Y, 1.0f / WS.X, 1.0f / WS.Y);

		const FScreenPassTextureViewport DurandWorkVP(WS, FIntRect(0, 0, WS.X, WS.Y));
		const FScreenPassTextureViewport SceneColorInputVP_D(
			FIntPoint(SceneColor.Texture->Desc.Extent.X, SceneColor.Texture->Desc.Extent.Y),
			SceneColorViewport.Rect);
		const FScreenTransform DurandSceneColorUV = (
			FScreenTransform::ChangeTextureBasisFromTo(DurandWorkVP, FScreenTransform::ETextureBasis::TexelPosition, FScreenTransform::ETextureBasis::ViewportUV) *
			FScreenTransform::ChangeTextureBasisFromTo(SceneColorInputVP_D, FScreenTransform::ETextureBasis::ViewportUV, FScreenTransform::ETextureBasis::TextureUV));

		// --- Pass 1: log-luminance ---
		FRDGTextureRef LogLumTex = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(WS, PF_R32_FLOAT, FClearValueBinding::None,
			    TexCreate_ShaderResource | TexCreate_RenderTargetable),
			TEXT("ToneMapDurand.LogLum"));

		{
			auto* P1 = GraphBuilder.AllocParameters<FToneMapDurandLogLumPS::FParameters>();
			P1->View = ViewInfo.ViewUniformBuffer;
			P1->SceneColorTexture = SceneColor.Texture;
			P1->SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			P1->SvPositionToSceneColorUV = DurandSceneColorUV;
			P1->OneOverPreExposure = 1.0f / FMath::Max(ViewInfo.PreExposure, 0.001f);
			P1->RenderTargets[0] = FRenderTargetBinding(LogLumTex, ERenderTargetLoadAction::ENoAction);
			TShaderMapRef<FToneMapDurandLogLumPS> Shader1(ViewInfo.ShaderMap);
			FPixelShaderUtils::AddFullscreenPass(GraphBuilder, ViewInfo.ShaderMap,
				RDG_EVENT_NAME("DurandLogLum"), Shader1, P1, FIntRect(0, 0, WS.X, WS.Y));
		}

		// --- Pass 2a/2b: cross-bilateral filter (horizontal then vertical) ---
		FRDGTextureRef BasePing = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(WS, PF_R32_FLOAT, FClearValueBinding::None,
			    TexCreate_ShaderResource | TexCreate_RenderTargetable),
			TEXT("ToneMapDurand.BasePing"));
		FRDGTextureRef BasePong = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(WS, PF_R32_FLOAT, FClearValueBinding::None,
			    TexCreate_ShaderResource | TexCreate_RenderTargetable),
			TEXT("ToneMapDurand.BasePong"));

		auto RunDurandBilateral = [&](FRDGTextureRef InLogLum, FRDGTextureRef GuideLogLum,
		                              FRDGTextureRef OutTex, FVector2f Dir, const TCHAR* EventName)
		{
			auto* P2 = GraphBuilder.AllocParameters<FToneMapDurandBilateralPS::FParameters>();
			P2->View = ViewInfo.ViewUniformBuffer;
			P2->LogLumTexture = InLogLum;
			P2->LogLumSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			P2->GuideTexture  = GuideLogLum;
			P2->GuideSampler  = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			P2->BufferSizeAndInvSize = BilateralBufferSize;
			P2->BlurDirection        = Dir;
			P2->SpatialSigma         = ActiveComp->DurandSpatialSigma;
			P2->RangeSigma           = ActiveComp->DurandRangeSigma;
			P2->RenderTargets[0]     = FRenderTargetBinding(OutTex, ERenderTargetLoadAction::ENoAction);
			TShaderMapRef<FToneMapDurandBilateralPS> Shader2(ViewInfo.ShaderMap);
			FPixelShaderUtils::AddFullscreenPass(GraphBuilder, ViewInfo.ShaderMap,
				FRDGEventName(EventName), Shader2, P2, FIntRect(0, 0, WS.X, WS.Y));
		};

		RunDurandBilateral(LogLumTex, LogLumTex, BasePing, FVector2f(1.0f, 0.0f), TEXT("DurandBilateralH"));
		RunDurandBilateral(BasePing,  LogLumTex, BasePong, FVector2f(0.0f, 1.0f), TEXT("DurandBilateralV"));

		// --- Pass 3: reconstruct ---
		FRDGTextureRef DurandResult = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(WS, PF_FloatRGBA, FClearValueBinding::None,
			    TexCreate_ShaderResource | TexCreate_RenderTargetable),
			TEXT("ToneMapDurand.Result"));

		{
			auto* P3 = GraphBuilder.AllocParameters<FToneMapDurandReconstructPS::FParameters>();
			P3->View = ViewInfo.ViewUniformBuffer;
			P3->SceneColorTexture = SceneColor.Texture;
			P3->SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			P3->LogLumTexture    = LogLumTex;
			P3->LogLumSampler    = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			P3->BaseLayerTexture = BasePong;
			P3->BaseLayerSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			P3->SvPositionToSceneColorUV = DurandSceneColorUV;
			P3->BufferSizeAndInvSize   = BilateralBufferSize;
			P3->OneOverPreExposure = 1.0f / FMath::Max(ViewInfo.PreExposure, 0.001f);
			P3->BaseCompression    = ActiveComp->DurandBaseCompression;
			P3->DetailBoost        = ActiveComp->DurandDetailBoost;
			P3->RenderTargets[0]   = FRenderTargetBinding(DurandResult, ERenderTargetLoadAction::ENoAction);
			TShaderMapRef<FToneMapDurandReconstructPS> Shader3(ViewInfo.ShaderMap);
			FPixelShaderUtils::AddFullscreenPass(GraphBuilder, ViewInfo.ShaderMap,
				RDG_EVENT_NAME("DurandReconstruct"), Shader3, P3, FIntRect(0, 0, WS.X, WS.Y));
		}

		PreToneMappedTexture = DurandResult;
		bPreToneMapped = true;
	}
	// =====================================================================
	// Fattal et al. 2002 Gradient-Domain Tone Mapping — pre-pass
	//
	// All passes run at full viewport resolution.  Seeding Jacobi with
	// log(lum) ensures partial convergence produces a valid compression ratio:
	//   ratio = exp(I_final - logLumIn)  →  < 1 on contrast edges (attenuated)
	//                                       ≈ 1 in smooth areas (preserved)
	// =====================================================================
	else if (bIsReplaceTonemap && ActiveComp->FilmCurve == EToneMapFilmCurve::Fattal)
	{
		RDG_EVENT_SCOPE(GraphBuilder, "ToneMapFX_Fattal");

		const FIntPoint WS = ViewportSize;
		const FVector4f FattalBufferSize((float)WS.X, (float)WS.Y, 1.0f / WS.X, 1.0f / WS.Y);
		const float     FattalOneOverPreExposure = 1.0f / FMath::Max(ViewInfo.PreExposure, 0.001f);

		const FScreenPassTextureViewport FattalWorkVP(WS, FIntRect(0, 0, WS.X, WS.Y));
		const FScreenPassTextureViewport SceneColorInputVP_F(
			FIntPoint(SceneColor.Texture->Desc.Extent.X, SceneColor.Texture->Desc.Extent.Y),
			SceneColorViewport.Rect);
		const FScreenTransform FattalSceneColorUV = (
			FScreenTransform::ChangeTextureBasisFromTo(FattalWorkVP, FScreenTransform::ETextureBasis::TexelPosition, FScreenTransform::ETextureBasis::ViewportUV) *
			FScreenTransform::ChangeTextureBasisFromTo(SceneColorInputVP_F, FScreenTransform::ETextureBasis::ViewportUV, FScreenTransform::ETextureBasis::TextureUV));

		// --- Pass 0: log-luminance (Jacobi seed) ---
		FRDGTextureRef LogLumTex = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(WS, PF_R32_FLOAT, FClearValueBinding::None,
			    TexCreate_ShaderResource | TexCreate_RenderTargetable),
			TEXT("ToneMapFattal.LogLum"));
		{
			auto* Pl = GraphBuilder.AllocParameters<FToneMapFattalLogLumPS::FParameters>();
			Pl->View = ViewInfo.ViewUniformBuffer;
			Pl->SceneColorTexture = SceneColor.Texture;
			Pl->SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			Pl->SvPositionToSceneColorUV = FattalSceneColorUV;
			Pl->OneOverPreExposure = FattalOneOverPreExposure;
			Pl->RenderTargets[0] = FRenderTargetBinding(LogLumTex, ERenderTargetLoadAction::ENoAction);
			TShaderMapRef<FToneMapFattalLogLumPS> ShaderL(ViewInfo.ShaderMap);
			FPixelShaderUtils::AddFullscreenPass(GraphBuilder, ViewInfo.ShaderMap,
				RDG_EVENT_NAME("FattalLogLum"), ShaderL, Pl, FIntRect(0, 0, WS.X, WS.Y));
		}

		// --- Pass 1: attenuated gradient field (Hx, Hy) ---
		FRDGTextureRef GradientTex = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(WS, PF_G32R32F, FClearValueBinding::None,
			    TexCreate_ShaderResource | TexCreate_RenderTargetable),
			TEXT("ToneMapFattal.Gradient"));
		{
			auto* Pg = GraphBuilder.AllocParameters<FToneMapFattalGradientPS::FParameters>();
			Pg->View = ViewInfo.ViewUniformBuffer;
			Pg->SceneColorTexture = SceneColor.Texture;
			Pg->SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			Pg->SvPositionToSceneColorUV = FattalSceneColorUV;
			Pg->BufferSizeAndInvSize = FattalBufferSize;
			Pg->OneOverPreExposure   = FattalOneOverPreExposure;
			Pg->Alpha      = ActiveComp->FattalAlpha;
			Pg->Beta       = ActiveComp->FattalBeta;
			Pg->NoiseFloor = ActiveComp->FattalNoise;
			Pg->RenderTargets[0] = FRenderTargetBinding(GradientTex, ERenderTargetLoadAction::ENoAction);
			TShaderMapRef<FToneMapFattalGradientPS> ShaderG(ViewInfo.ShaderMap);
			FPixelShaderUtils::AddFullscreenPass(GraphBuilder, ViewInfo.ShaderMap,
				RDG_EVENT_NAME("FattalGradient"), ShaderG, Pg, FIntRect(0, 0, WS.X, WS.Y));
		}

		// --- Pass 2: divergence div(H) ---
		FRDGTextureRef DivHTex = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(WS, PF_R32_FLOAT, FClearValueBinding::None,
			    TexCreate_ShaderResource | TexCreate_RenderTargetable),
			TEXT("ToneMapFattal.DivH"));
		{
			auto* Pd = GraphBuilder.AllocParameters<FToneMapFattalDivergencePS::FParameters>();
			Pd->View = ViewInfo.ViewUniformBuffer;
			Pd->GradientTexture = GradientTex;
			Pd->GradientSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			Pd->BufferSizeAndInvSize = FattalBufferSize;
			Pd->RenderTargets[0] = FRenderTargetBinding(DivHTex, ERenderTargetLoadAction::ENoAction);
			TShaderMapRef<FToneMapFattalDivergencePS> ShaderD(ViewInfo.ShaderMap);
			FPixelShaderUtils::AddFullscreenPass(GraphBuilder, ViewInfo.ShaderMap,
				RDG_EVENT_NAME("FattalDivergence"), ShaderD, Pd, FIntRect(0, 0, WS.X, WS.Y));
		}

		// --- Pass 3: Jacobi Poisson solver (seeded with log-lum) ---
		FRDGTextureRef JPing = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(WS, PF_R32_FLOAT, FClearValueBinding::None,
			    TexCreate_ShaderResource | TexCreate_RenderTargetable),
			TEXT("ToneMapFattal.JPing"));
		FRDGTextureRef JPong = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(WS, PF_R32_FLOAT, FClearValueBinding::None,
			    TexCreate_ShaderResource | TexCreate_RenderTargetable),
			TEXT("ToneMapFattal.JPong"));

		FRDGTextureRef JCurrent = LogLumTex; // seed: logLum gives useful partial-convergence
		const int32 FattalIters = FMath::Clamp(ActiveComp->FattalJacobiIterations, 1, 200);
		for (int32 It = 0; It < FattalIters; ++It)
		{
			FRDGTextureRef JOut = (It % 2 == 0) ? JPing : JPong;
			auto* Pj = GraphBuilder.AllocParameters<FToneMapFattalJacobiPS::FParameters>();
			Pj->View = ViewInfo.ViewUniformBuffer;
			Pj->CurrentITexture = JCurrent;
			Pj->CurrentISampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			Pj->DivHTexture     = DivHTex;
			Pj->DivHSampler     = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			Pj->BufferSizeAndInvSize = FattalBufferSize;
			Pj->RenderTargets[0] = FRenderTargetBinding(JOut, ERenderTargetLoadAction::ENoAction);
			TShaderMapRef<FToneMapFattalJacobiPS> ShaderJ(ViewInfo.ShaderMap);
			FPixelShaderUtils::AddFullscreenPass(GraphBuilder, ViewInfo.ShaderMap,
				RDG_EVENT_NAME("FattalJacobi"), ShaderJ, Pj, FIntRect(0, 0, WS.X, WS.Y));
			JCurrent = JOut;
		}

		// --- Pass 4: reconstruct ---
		FRDGTextureRef FattalResult = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(WS, PF_FloatRGBA, FClearValueBinding::None,
			    TexCreate_ShaderResource | TexCreate_RenderTargetable),
			TEXT("ToneMapFattal.Result"));
		{
			auto* Pr = GraphBuilder.AllocParameters<FToneMapFattalReconstructPS::FParameters>();
			Pr->View = ViewInfo.ViewUniformBuffer;
			Pr->SceneColorTexture = SceneColor.Texture;
			Pr->SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			Pr->SolvedITexture    = JCurrent;
			Pr->SolvedISampler    = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			Pr->SvPositionToSceneColorUV = FattalSceneColorUV;
			Pr->BufferSizeAndInvSize     = FattalBufferSize;
			Pr->OneOverPreExposure = FattalOneOverPreExposure;
			Pr->OutputSaturation   = ActiveComp->FattalSaturation;
			Pr->RenderTargets[0]   = FRenderTargetBinding(FattalResult, ERenderTargetLoadAction::ENoAction);
			TShaderMapRef<FToneMapFattalReconstructPS> ShaderR(ViewInfo.ShaderMap);
			FPixelShaderUtils::AddFullscreenPass(GraphBuilder, ViewInfo.ShaderMap,
				RDG_EVENT_NAME("FattalReconstruct"), ShaderR, Pr, FIntRect(0, 0, WS.X, WS.Y));
		}

		PreToneMappedTexture = FattalResult;
		bPreToneMapped = true;
	}

	// =====================================================================
	// Lens Effects — Ciliary Corona and Lenticular Halo
	// Runs after bloom composite; composites the effects onto current SceneColor.
	// =====================================================================
	{
		const bool bRunLensEffects = ActiveComp->bEnableCiliaryCorona || ActiveComp->bEnableLenticularHalo;
		if (bRunLensEffects)
		{
			RDG_EVENT_SCOPE(GraphBuilder, "ToneMapFX_LensEffects");

			const FIntPoint WS = ViewportSize;
			const FVector4f LensBufferSize((float)WS.X, (float)WS.Y, 1.0f / WS.X, 1.0f / WS.Y);

			const FScreenPassTextureViewport LensWorkVP(WS, FIntRect(0, 0, WS.X, WS.Y));
			const FScreenPassTextureViewport SceneColorInputVP_L(
				FIntPoint(SceneColor.Texture->Desc.Extent.X, SceneColor.Texture->Desc.Extent.Y),
				SceneColorViewport.Rect);
			const FScreenTransform LensSceneColorUV = (
				FScreenTransform::ChangeTextureBasisFromTo(LensWorkVP, FScreenTransform::ETextureBasis::TexelPosition, FScreenTransform::ETextureBasis::ViewportUV) *
				FScreenTransform::ChangeTextureBasisFromTo(SceneColorInputVP_L, FScreenTransform::ETextureBasis::ViewportUV, FScreenTransform::ETextureBasis::TextureUV));
			const FScreenTransform LensBrightPassUV = (
				FScreenTransform::ChangeTextureBasisFromTo(LensWorkVP, FScreenTransform::ETextureBasis::TexelPosition, FScreenTransform::ETextureBasis::ViewportUV) *
				FScreenTransform::ChangeTextureBasisFromTo(LensWorkVP, FScreenTransform::ETextureBasis::ViewportUV, FScreenTransform::ETextureBasis::TextureUV));

			// Use the lower of the two thresholds for the shared bright-pass
			const float BrightPassThreshold = (ActiveComp->bEnableCiliaryCorona && ActiveComp->bEnableLenticularHalo)
				? FMath::Min(ActiveComp->CoronaThreshold, ActiveComp->HaloThreshold)
				: (ActiveComp->bEnableCiliaryCorona ? ActiveComp->CoronaThreshold : ActiveComp->HaloThreshold);

			FRDGTextureRef BrightPassTex = GraphBuilder.CreateTexture(
				FRDGTextureDesc::Create2D(WS, PF_FloatRGBA, FClearValueBinding::None,
				    TexCreate_ShaderResource | TexCreate_RenderTargetable),
				TEXT("ToneMapLens.BrightPass"));

			{
				auto* Pb = GraphBuilder.AllocParameters<FToneMapLensBrightPassPS::FParameters>();
				Pb->View = ViewInfo.ViewUniformBuffer;
				Pb->SceneColorTexture = SceneColor.Texture;
				Pb->SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
				Pb->SvPositionToSceneColorUV = LensSceneColorUV;
				Pb->Threshold = BrightPassThreshold;
				Pb->RenderTargets[0] = FRenderTargetBinding(BrightPassTex, ERenderTargetLoadAction::ENoAction);
				TShaderMapRef<FToneMapLensBrightPassPS> ShaderBP(ViewInfo.ShaderMap);
				FPixelShaderUtils::AddFullscreenPass(GraphBuilder, ViewInfo.ShaderMap,
					RDG_EVENT_NAME("LensBrightPass"), ShaderBP, Pb, FIntRect(0, 0, WS.X, WS.Y));
			}

			FRDGTextureRef LensCoronaTex = SceneColor.Texture; // fallback
			FRDGTextureRef LensHaloTex   = SceneColor.Texture; // fallback

			// Corona streaks
			if (ActiveComp->bEnableCiliaryCorona)
			{
				FRDGTextureRef CoronaOut = GraphBuilder.CreateTexture(
					FRDGTextureDesc::Create2D(WS, PF_FloatRGBA, FClearValueBinding::None,
					    TexCreate_ShaderResource | TexCreate_RenderTargetable),
					TEXT("ToneMapLens.Corona"));

				auto* Pc = GraphBuilder.AllocParameters<FToneMapCoronaStreakPS::FParameters>();
				Pc->View = ViewInfo.ViewUniformBuffer;
				Pc->BrightPassTexture = BrightPassTex;
				Pc->BrightPassSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
				Pc->SvPositionToBrightPassUV = LensBrightPassUV;
				Pc->BufferSizeAndInvSize = LensBufferSize;
				Pc->SpikeCount           = ActiveComp->CoronaSpikeCount;
				Pc->SpikeLength          = ActiveComp->CoronaSpikeLength;
				Pc->CoronaIntensity      = ActiveComp->CoronaIntensity;
				Pc->RenderTargets[0] = FRenderTargetBinding(CoronaOut, ERenderTargetLoadAction::ENoAction);
				TShaderMapRef<FToneMapCoronaStreakPS> ShaderC(ViewInfo.ShaderMap);
				FPixelShaderUtils::AddFullscreenPass(GraphBuilder, ViewInfo.ShaderMap,
					RDG_EVENT_NAME("CoronaStreaks"), ShaderC, Pc, FIntRect(0, 0, WS.X, WS.Y));

				LensCoronaTex = CoronaOut;
			}

			// Lenticular halo ring
			if (ActiveComp->bEnableLenticularHalo)
			{
				FRDGTextureRef HaloOut = GraphBuilder.CreateTexture(
					FRDGTextureDesc::Create2D(WS, PF_FloatRGBA, FClearValueBinding::None,
					    TexCreate_ShaderResource | TexCreate_RenderTargetable),
					TEXT("ToneMapLens.Halo"));

				auto* Ph = GraphBuilder.AllocParameters<FToneMapHaloRingPS::FParameters>();
				Ph->View = ViewInfo.ViewUniformBuffer;
				Ph->BrightPassTexture = BrightPassTex;
				Ph->BrightPassSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
				Ph->SvPositionToBrightPassUV = LensBrightPassUV;
				Ph->BufferSizeAndInvSize = LensBufferSize;
				Ph->HaloPattern = static_cast<float>(static_cast<uint8>(ActiveComp->HaloPattern));
				Ph->HaloRadius    = ActiveComp->HaloRadius;
				Ph->HaloThickness = ActiveComp->HaloThickness;
				Ph->HaloIrregularity = ActiveComp->HaloIrregularity;
				Ph->HaloArcStretch = ActiveComp->HaloArcStretch;
				Ph->HaloLineCount = FMath::Clamp(ActiveComp->HaloLineCount, 1, 12);
				Ph->HaloLineThickness = ActiveComp->HaloLineThickness;
				Ph->HaloIntensity = ActiveComp->HaloIntensity;
				Ph->HaloTint      = FVector3f(ActiveComp->HaloTint.R, ActiveComp->HaloTint.G, ActiveComp->HaloTint.B);
				Ph->RenderTargets[0] = FRenderTargetBinding(HaloOut, ERenderTargetLoadAction::ENoAction);
				TShaderMapRef<FToneMapHaloRingPS> ShaderH(ViewInfo.ShaderMap);
				FPixelShaderUtils::AddFullscreenPass(GraphBuilder, ViewInfo.ShaderMap,
					RDG_EVENT_NAME("HaloRing"), ShaderH, Ph, FIntRect(0, 0, WS.X, WS.Y));

				LensHaloTex = HaloOut;
			}

			// Composite lens effects onto scene color
			FRDGTextureRef LensCompositeOut = GraphBuilder.CreateTexture(
				FRDGTextureDesc::Create2D(WS, SceneColor.Texture->Desc.Format, FClearValueBinding::None,
				    TexCreate_ShaderResource | TexCreate_RenderTargetable),
				TEXT("ToneMapLens.Composite"));

			{
				auto* Plc = GraphBuilder.AllocParameters<FToneMapLensCompositePS::FParameters>();
				Plc->View = ViewInfo.ViewUniformBuffer;
				Plc->SceneColorTexture = SceneColor.Texture;
				Plc->SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
				Plc->CoronaTexture     = LensCoronaTex;
				Plc->CoronaSampler     = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
				Plc->HaloTexture       = LensHaloTex;
				Plc->HaloSampler       = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
				Plc->SvPositionToSceneColorUV = LensSceneColorUV;
				Plc->SvPositionToLensUV       = LensBrightPassUV;
				Plc->bEnableCorona = ActiveComp->bEnableCiliaryCorona  ? 1.0f : 0.0f;
				Plc->bEnableHalo   = ActiveComp->bEnableLenticularHalo ? 1.0f : 0.0f;
				Plc->RenderTargets[0] = FRenderTargetBinding(LensCompositeOut, ERenderTargetLoadAction::ENoAction);
				TShaderMapRef<FToneMapLensCompositePS> ShaderLC(ViewInfo.ShaderMap);
				FPixelShaderUtils::AddFullscreenPass(GraphBuilder, ViewInfo.ShaderMap,
					RDG_EVENT_NAME("LensEffectsComposite"), ShaderLC, Plc, FIntRect(0, 0, WS.X, WS.Y));
			}

			// Replace SceneColor so downstream ToneMapProcess sees the lens-composited image
			SceneColor = FScreenPassTexture(LensCompositeOut, FIntRect(0, 0, WS.X, WS.Y));
		}
	}

	// =====================================================================
	// Determine output target
	// =====================================================================

	FScreenPassRenderTarget OutputTarget;

	if (bIsReplaceTonemap && Inputs.OverrideOutput.IsValid())
	{
		// ReplacingTonemapper: engine provides the final backbuffer as OverrideOutput
		// when Tonemap is the last pass (no FXAA/SMAA after). Use it directly.
		OutputTarget = Inputs.OverrideOutput;
	}
	else
	{
		// Create an intermediate FP16 texture for downstream passes (SMAA, FXAA).
		// Extent must match the engine's quantized scene buffer extent so that
		// SMAA's QuantizeSceneBufferSize(Extent) is a no-op and RTMetrics are
		// correct.  ViewRect stays (0,0)-based matching ViewportSize.
		// EClear prevents uninitialized gap pixels between ViewRect and extent
		// from showing as green in non-fullscreen viewports.
		OutputTarget = FScreenPassRenderTarget(
			GraphBuilder.CreateTexture(
				FRDGTextureDesc::Create2D(
					OriginalSceneColorExtent, PF_FloatRGBA,
					FClearValueBinding::Black,
					TexCreate_ShaderResource | TexCreate_RenderTargetable),
				TEXT("ToneMap.Output")),
			FIntRect(0, 0, ViewportSize.X, ViewportSize.Y),
			ERenderTargetLoadAction::EClear);
	}

	// =====================================================================
	// Post-pass chain: LUT → Vignette (each redirects through intermediates)
	// =====================================================================
	const bool bWantHDREncode = bIsReplaceTonemap && ActiveComp->bHDROutput && bCachedHDROutput;
	const bool bNeedSharpening = ActiveComp->bEnableSharpening
		&& ActiveComp->SharpenAmount > 0.01f;

	const bool bNeedVignette = ActiveComp->bEnableVignette
		&& FMath::Abs(ActiveComp->VignetteIntensity) > 0.01f;

	// HDR output encoding as a final pass: requires ReplaceTonemap + HDR checkbox +
	// an HDR-capable display (OutputDevice >= 3 in EDisplayOutputFormat).
	bool bNeedHDREncode = false;
	uint32 HDROutputDevice = 0;
	float  HDRMaxDisplayNits = 80.0f;
	if (bWantHDREncode)
	{
		FTonemapperOutputDeviceParameters OutDevParams = GetTonemapperOutputDeviceParameters(*ViewInfo.Family);
		HDROutputDevice = OutDevParams.OutputDevice;
		HDRMaxDisplayNits = FMath::Max(OutDevParams.OutputMaxLuminance, 80.0f);
		// Only add the HDR encode pass when the display is actually HDR (device >= 3)
		bNeedHDREncode = (HDROutputDevice >= 3);
	}
	const bool bLinearHDROutput = bNeedHDREncode && ActiveComp->HDROutputMode == EToneMapHDROutputMode::TrueHDR;
	const bool bNeedLUT = !bLinearHDROutput
		&& ActiveComp->bEnableLUT
		&& ActiveComp->LUTTexture != nullptr
		&& ActiveComp->LUTTexture->GetResource() != nullptr
		&& ActiveComp->LUTTexture->GetResource()->TextureRHI != nullptr
		&& ActiveComp->LUTIntensity > 0.001f;

	if (ActiveComp->bDebugHDRLogging)
	{
		const uint8 OutputMode = static_cast<uint8>(ActiveComp->HDROutputMode);
		const bool bStateChanged =
			!LastHDRRenderDebugState.bValid ||
			LastHDRRenderDebugState.bWantEncode != bWantHDREncode ||
			LastHDRRenderDebugState.bNeedEncode != bNeedHDREncode ||
			LastHDRRenderDebugState.bLinearOutput != bLinearHDROutput ||
			LastHDRRenderDebugState.OutputDevice != HDROutputDevice ||
			LastHDRRenderDebugState.OutputMode != OutputMode ||
			!FMath::IsNearlyEqual(LastHDRRenderDebugState.OutputMaxLuminance, HDRMaxDisplayNits, 0.5f) ||
			!FMath::IsNearlyEqual(LastHDRRenderDebugState.PaperWhiteNits, ActiveComp->PaperWhiteNits, 0.5f);

		if (bStateChanged)
		{
			UE_LOG(LogTemp, Log,
				TEXT("ToneMapFX HDR render: WantEncode=%s NeedEncode=%s TrueHDRLinear=%s OutputDevice=%u MaxDisplayNits=%.1f Mode=%s PaperWhiteNits=%.1f LUTPass=%s"),
				GetBoolText(bWantHDREncode),
				GetBoolText(bNeedHDREncode),
				GetBoolText(bLinearHDROutput),
				HDROutputDevice,
				HDRMaxDisplayNits,
				GetHDROutputModeText(ActiveComp->HDROutputMode),
				ActiveComp->PaperWhiteNits,
				GetBoolText(bNeedLUT));

			LastHDRRenderDebugState.bValid = true;
			LastHDRRenderDebugState.bWantEncode = bWantHDREncode;
			LastHDRRenderDebugState.bNeedEncode = bNeedHDREncode;
			LastHDRRenderDebugState.bLinearOutput = bLinearHDROutput;
			LastHDRRenderDebugState.OutputDevice = HDROutputDevice;
			LastHDRRenderDebugState.OutputMode = OutputMode;
			LastHDRRenderDebugState.OutputMaxLuminance = HDRMaxDisplayNits;
			LastHDRRenderDebugState.PaperWhiteNits = ActiveComp->PaperWhiteNits;
		}
	}
	else
	{
		LastHDRRenderDebugState.bValid = false;
	}

	FScreenPassRenderTarget FinalOutputTarget = OutputTarget;

	// =====================================================================
	// Dithering quantum — auto-detect from display output bit depth
	// Applied only in the LAST pass of the chain to avoid dither noise
	// being averaged out by subsequent passes (e.g. sharpening kernel).
	//
	// UE's built-in tonemapper defaults to 1/1023 (10-bit), but its
	// combined 3D LUT naturally introduces sub-LSB interpolation noise
	// that acts as extra dithering.  Our per-pixel math doesn't get that
	// free smoothing, so we default to 1/255 (8-bit) which is safe for
	// both 8-bit and 10-bit displays — on a 10-bit panel the added noise
	// is only ~¼ LSB, invisible in practice.
	//
	// HDR scRGB / float16 output → 0 (no quantization dithering needed).
	// =====================================================================
	float DitherQuantizationValue = 0.0f;
	if (ActiveComp->bEnableDithering)
	{
		// Disable for HDR linear / scRGB — float16 has sufficient precision
		const bool bIsHDRLinear = bNeedHDREncode && HDROutputDevice >= 5;

		if (!bIsHDRLinear)
		{
			DitherQuantizationValue = ActiveComp->DitherQuantization;
		}
	}

	// Determine which pass is last in the chain (only it receives dithering)
	const bool bUseLUTPath = (ActiveComp->ProcessingPath == EToneMapProcessingPath::LUT);
	const bool bToneMapIsLast  = !bNeedSharpening && !bNeedLUT && !bNeedVignette && !bNeedHDREncode;
	const bool bSharpenIsLast  = bNeedSharpening && !bNeedLUT && !bNeedVignette && !bNeedHDREncode;
	const bool bLUTIsLast      = bNeedLUT && !bNeedVignette && !bNeedHDREncode;
	const bool bVignetteIsLast = bNeedVignette && !bNeedHDREncode;
	const bool bHDREncodeIsLast = bNeedHDREncode;

	// If any post-passes follow ToneMapProcess, redirect it to an intermediate
	if (bNeedSharpening || bNeedLUT || bNeedVignette || bNeedHDREncode)
	{
		OutputTarget = FScreenPassRenderTarget(
			GraphBuilder.CreateTexture(
				FRDGTextureDesc::Create2D(
					ViewportSize, PF_FloatRGBA,
					FClearValueBinding::None,
					TexCreate_ShaderResource | TexCreate_RenderTargetable),
				TEXT("ToneMap.PrePostPasses")),
			FIntRect(0, 0, ViewportSize.X, ViewportSize.Y),
			ERenderTargetLoadAction::ENoAction);
	}

	// =====================================================================
	// Main Tone Map processing pass (dual-path: Per-Pixel vs LUT)
	// =====================================================================

	if (!bUseLUTPath)
	{
		// =================================================================
		// PER-PIXEL PATH — existing single-pass processing
		// =================================================================
		auto* P = GraphBuilder.AllocParameters<FToneMapProcessPS::FParameters>();
		P->View              = ViewInfo.ViewUniformBuffer;
		P->SceneColorTexture = SceneColor.Texture;
		P->SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		P->BlurredTexture    = BlurredTexture;
		P->BlurredSampler    = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

		// Build FScreenTransform for proper SvPosition → texture UV mapping.
		// This correctly handles viewport offsets (e.g. OverrideOutput with non-zero
		// Min in ReplaceTonemap mode) that caused glitches on viewport resize.
		const FIntPoint OutputExtent = FIntPoint(OutputTarget.Texture->Desc.Extent.X, OutputTarget.Texture->Desc.Extent.Y);
		const FIntRect  OutputViewRect = OutputTarget.ViewRect;
		const FScreenPassTextureViewport OutputVP(OutputExtent, OutputViewRect);

		// SceneColor viewport (engine-provided, may have non-zero offset)
		const FScreenPassTextureViewport SceneColorInputVP(
			FIntPoint(SceneColor.Texture->Desc.Extent.X, SceneColor.Texture->Desc.Extent.Y),
			SceneColorViewport.Rect);

		// SvPosition → ViewportUV [0,1] → SceneColor TextureUV
		P->SvPositionToSceneColorUV = (
			FScreenTransform::ChangeTextureBasisFromTo(OutputVP, FScreenTransform::ETextureBasis::TexelPosition, FScreenTransform::ETextureBasis::ViewportUV) *
			FScreenTransform::ChangeTextureBasisFromTo(SceneColorInputVP, FScreenTransform::ETextureBasis::ViewportUV, FScreenTransform::ETextureBasis::TextureUV));

		// Blurred texture: created at ViewportSize with rect (0,0)->(W,H), no offset
		const FScreenPassTextureViewport BlurredVP(
			ViewportSize, FIntRect(0, 0, ViewportSize.X, ViewportSize.Y));

		// SvPosition → ViewportUV [0,1] → Blurred TextureUV
		P->SvPositionToBlurredUV = (
			FScreenTransform::ChangeTextureBasisFromTo(OutputVP, FScreenTransform::ETextureBasis::TexelPosition, FScreenTransform::ETextureBasis::ViewportUV) *
			FScreenTransform::ChangeTextureBasisFromTo(BlurredVP, FScreenTransform::ETextureBasis::ViewportUV, FScreenTransform::ETextureBasis::TextureUV));

		// Output viewport rect for split-screen comparison
		P->OutputViewportRect = FVector4f(
			(float)OutputViewRect.Min.X, (float)OutputViewRect.Min.Y,
			(float)OutputViewRect.Max.X, (float)OutputViewRect.Max.Y);

		// ---- Bloom texture (ReplaceTonemap mode) ----
		if (bIsReplaceTonemap && BloomInput.IsValid())
		{
			P->BloomTexture = BloomInput.Texture;
			P->BloomSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			const FScreenPassTextureViewport BloomVP(BloomInput);

			// SvPosition → ViewportUV [0,1] → Bloom TextureUV
			P->SvPositionToBloomUV = (
				FScreenTransform::ChangeTextureBasisFromTo(OutputVP, FScreenTransform::ETextureBasis::TexelPosition, FScreenTransform::ETextureBasis::ViewportUV) *
				FScreenTransform::ChangeTextureBasisFromTo(BloomVP, FScreenTransform::ETextureBasis::ViewportUV, FScreenTransform::ETextureBasis::TextureUV));
		}
		else
		{
			// Provide a valid fallback (scene color itself, won't be sampled when bReplaceTonemap==0)
			P->BloomTexture = SceneColor.Texture;
			P->BloomSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			P->SvPositionToBloomUV = P->SvPositionToSceneColorUV;
		}

		// ---- ReplaceTonemap mode flag & exposure ----
		P->bReplaceTonemap    = bIsReplaceTonemap ? 1.0f : 0.0f;
		P->bLinearHDROutput   = bLinearHDROutput ? 1.0f : 0.0f;
		P->OneOverPreExposure = 1.0f / FMath::Max(ViewInfo.PreExposure, 0.001f);
		P->GlobalExposure     = FMath::Max(View.GetLastEyeAdaptationExposure(), 0.001f);

		// ---- Auto-Exposure mode & Krawczyk adapted luminance ----
		P->AutoExposureMode = (float)static_cast<uint8>(ActiveComp->AutoExposureMode);
		if (bNeedKrawczyk && AdaptedLumTexture)
		{
			P->AdaptedLumTexture = AdaptedLumTexture;
		}
		else
		{
			// Provide a valid fallback (won't be sampled when mode != Krawczyk)
			P->AdaptedLumTexture = SceneColor.Texture;
		}
		P->AdaptedLumSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		P->MinAutoExposure = ActiveComp->MinAutoExposure;
		P->MaxAutoExposure = ActiveComp->MaxAutoExposure;

		// ---- Film Curve mode & Hable params ----
		P->FilmCurveMode = GetShaderFilmCurveMode(*ActiveComp);
		P->HableParams1 = FVector4f(
			ActiveComp->HableShoulderStrength,  // A
			ActiveComp->HableLinearStrength,    // B
			ActiveComp->HableLinearAngle,       // C
			ActiveComp->HableToeStrength);      // D
		P->HableParams2 = FVector4f(
			ActiveComp->HableToeNumerator,      // E
			ActiveComp->HableToeDenominator,    // F
			ActiveComp->HableWhitePoint,        // W
			0.0f);                              // unused
		P->ReinhardWhitePoint = ActiveComp->ReinhardWhitePoint;
		P->CustomCurveTexture = RegisterExternalTextureOrFallback(
			GraphBuilder,
			ActiveComp->CustomToneCurveTexture,
			SceneColor.Texture,
			TEXT("ToneMapCustomToneCurve"));
		P->CustomCurveSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		P->CustomCurveParams = FVector4f(
			ActiveComp->CustomCurveMinEV,
			1.0f / FMath::Max(ActiveComp->CustomCurveMaxEV - ActiveComp->CustomCurveMinEV, 0.01f),
			0.0f,
			0.0f);
		P->HDRSaturation  = ActiveComp->HDRSaturation;
		P->HDRColorBalance = FVector3f(
			ActiveComp->HDRColorBalance.R,
			ActiveComp->HDRColorBalance.G,
			ActiveComp->HDRColorBalance.B);

		// ---- AgX params ----
		P->AgXParams = FVector4f(
			ActiveComp->AgXMinEV,
			ActiveComp->AgXMaxEV,
			(float)static_cast<uint8>(ActiveComp->AgXLook),
			0.0f);

		// ---- Pre-tone-mapped texture (Durand / Fattal bypass) ----
		P->bPreToneMapped = bPreToneMapped ? 1.0f : 0.0f;
		if (bPreToneMapped && PreToneMappedTexture)
		{
			P->PreToneMappedTexture = PreToneMappedTexture;
			P->PreToneMappedSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

			// Pre-mapped texture lives in ViewportSize space with rect (0,0)→(W,H)
			const FScreenPassTextureViewport PreTMVP(
				FIntPoint(PreToneMappedTexture->Desc.Extent.X, PreToneMappedTexture->Desc.Extent.Y),
				FIntRect(0, 0, PreToneMappedTexture->Desc.Extent.X, PreToneMappedTexture->Desc.Extent.Y));
			P->SvPositionToPreToneMappedUV = (
				FScreenTransform::ChangeTextureBasisFromTo(OutputVP, FScreenTransform::ETextureBasis::TexelPosition, FScreenTransform::ETextureBasis::ViewportUV) *
				FScreenTransform::ChangeTextureBasisFromTo(PreTMVP, FScreenTransform::ETextureBasis::ViewportUV, FScreenTransform::ETextureBasis::TextureUV));
		}
		else
		{
			// Fallback: provide valid texture to prevent RDG null-binding assert (won't be sampled)
			P->PreToneMappedTexture = SceneColor.Texture;
			P->PreToneMappedSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			P->SvPositionToPreToneMappedUV = P->SvPositionToSceneColorUV;
		}

		// --- White Balance ---
		P->Temperature = ActiveComp->bEnableWhiteBalance ? ActiveComp->Temperature : 0.0f;
		P->Tint        = ActiveComp->bEnableWhiteBalance ? ActiveComp->Tint : 0.0f;

		// --- Exposure ---
		P->ExposureValue = ActiveComp->Exposure;

		float CameraEV = 0.0f;
		if (ActiveComp->bUseCameraExposure)
		{
			float N = FMath::Max(ActiveComp->Aperture, 1.0f);
			float t = 1.0f / FMath::Max(ActiveComp->ShutterSpeedDenominator, 1.0f); // 1/X seconds
			float S = FMath::Max(ActiveComp->CameraISO, 1.0f);
			float EV100 = FMath::Log2(N * N / t) + FMath::Log2(100.0f / S);
			const float ReferenceEV = FMath::Log2(5.6f * 5.6f / (1.0f / 125.0f)) + FMath::Log2(100.0f / 100.0f);
			// Negate: higher ISO / wider aperture / slower shutter = more light = positive compensation
			CameraEV = ReferenceEV - EV100;
		}
		P->CameraEV           = CameraEV;
		P->bUseCameraExposure = ActiveComp->bUseCameraExposure ? 1.0f : 0.0f;

		// --- Tone ---
		P->Contrast        = ActiveComp->Contrast;
		P->HighlightsValue = ActiveComp->bEnableToneAdjustments ? ActiveComp->Highlights : 0.0f;
		P->ShadowsValue    = ActiveComp->bEnableToneAdjustments ? ActiveComp->Shadows : 0.0f;
		P->WhitesValue     = ActiveComp->bEnableToneAdjustments ? ActiveComp->Whites : 0.0f;
		P->BlacksValue     = ActiveComp->bEnableToneAdjustments ? ActiveComp->Blacks : 0.0f;
		P->ToneSmoothingValue = ActiveComp->ToneSmoothing;
		P->ContrastMidpoint   = ActiveComp->ContrastMidpoint;

		// --- Presence ---
		P->ClarityStrength    = ActiveComp->Clarity;
		P->VibranceStrength   = ActiveComp->Vibrance;
		P->SaturationStrength = ActiveComp->Saturation;

		// --- Dynamic Contrast fine/coarse blur textures ---
		P->BlurredFineTexture   = BlurredFineTexture;
		P->BlurredFineSampler   = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		P->BlurredCoarseTexture = BlurredCoarseTexture;
		P->BlurredCoarseSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

		// Fine/coarse blur textures: same layout as Clarity blurred (ViewportSize, rect 0→W,H)
		P->SvPositionToBlurredFineUV = (
			FScreenTransform::ChangeTextureBasisFromTo(OutputVP, FScreenTransform::ETextureBasis::TexelPosition, FScreenTransform::ETextureBasis::ViewportUV) *
			FScreenTransform::ChangeTextureBasisFromTo(BlurredVP, FScreenTransform::ETextureBasis::ViewportUV, FScreenTransform::ETextureBasis::TextureUV));
		P->SvPositionToBlurredCoarseUV = (
			FScreenTransform::ChangeTextureBasisFromTo(OutputVP, FScreenTransform::ETextureBasis::TexelPosition, FScreenTransform::ETextureBasis::ViewportUV) *
			FScreenTransform::ChangeTextureBasisFromTo(BlurredVP, FScreenTransform::ETextureBasis::ViewportUV, FScreenTransform::ETextureBasis::TextureUV));

		// --- Dynamic Contrast strengths ---
		P->DynamicContrastStrength    = ActiveComp->DynamicContrast;
		P->CorrectContrastStrength    = ActiveComp->CorrectContrast;
		P->CorrectColorCastStrength   = ActiveComp->CorrectColorCast;

		// --- Tone Curve ---
		P->ToneCurveParams = FVector4f(
			ActiveComp->CurveHighlights,
			ActiveComp->CurveLights,
			ActiveComp->CurveDarks,
			ActiveComp->CurveShadows);

		// --- HSL (packed float4) ---
		P->HueShift1 = FVector4f(ActiveComp->HueReds,   ActiveComp->HueOranges,   ActiveComp->HueYellows,   ActiveComp->HueGreens);
		P->HueShift2 = FVector4f(ActiveComp->HueAquas,  ActiveComp->HueBlues,     ActiveComp->HuePurples,   ActiveComp->HueMagentas);
		P->SatAdj1   = FVector4f(ActiveComp->SatReds,    ActiveComp->SatOranges,    ActiveComp->SatYellows,    ActiveComp->SatGreens);
		P->SatAdj2   = FVector4f(ActiveComp->SatAquas,   ActiveComp->SatBlues,      ActiveComp->SatPurples,    ActiveComp->SatMagentas);
		P->LumAdj1   = FVector4f(ActiveComp->LumReds,    ActiveComp->LumOranges,    ActiveComp->LumYellows,    ActiveComp->LumGreens);
		P->LumAdj2   = FVector4f(ActiveComp->LumAquas,   ActiveComp->LumBlues,      ActiveComp->LumPurples,    ActiveComp->LumMagentas);

		// --- HSL Smoothing ---
		P->HSLSmoothing = ActiveComp->HSLSmoothing;
		P->HSLMode = static_cast<float>(static_cast<uint8>(ActiveComp->HSLMode));

		// --- Feature toggles ---
		P->bEnableHSL    = ActiveComp->IsAnyHSLActive()   ? 1.0f : 0.0f;
		P->bEnableCurves = ActiveComp->IsAnyCurveActive()  ? 1.0f : 0.0f;
		P->DitherQuantization = bToneMapIsLast ? DitherQuantizationValue : 0.0f;

		P->RenderTargets[0] = FRenderTargetBinding(OutputTarget.Texture, OutputTarget.LoadAction);

		TShaderMapRef<FToneMapProcessPS> ProcessShader(ViewInfo.ShaderMap);
		FPixelShaderUtils::AddFullscreenPass(
			GraphBuilder, ViewInfo.ShaderMap,
			RDG_EVENT_NAME("ToneMapProcess"),
			ProcessShader, P,
			OutputTarget.ViewRect);
	}
	else
	{
		// =================================================================
		// LUT PATH — Bake non-spatial ops into 32^3 LUT, then apply
		// =================================================================
		const float LUTDim = 32.0f;
		const FIntPoint LUTTextureSize(
			(int32)(LUTDim * LUTDim),  // 1024
			(int32)LUTDim);            // 32

		// --- Step 1: Generate the baked LUT ---
		FRDGTextureRef BakedLUTTexture = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(
				LUTTextureSize, PF_FloatRGBA,
				FClearValueBinding::None,
				TexCreate_ShaderResource | TexCreate_RenderTargetable),
			TEXT("ToneMap.BakedLUT"));

		{
			auto* LP = GraphBuilder.AllocParameters<FToneMapCombineLUTPS::FParameters>();
			LP->View = ViewInfo.ViewUniformBuffer;
			LP->LUTSize = LUTDim;
			LP->bReplaceTonemap = bIsReplaceTonemap ? 1.0f : 0.0f;
			LP->bLinearHDROutput = bLinearHDROutput ? 1.0f : 0.0f;

			// Film Curve
			LP->FilmCurveMode = GetShaderFilmCurveMode(*ActiveComp);
			LP->HableParams1 = FVector4f(
				ActiveComp->HableShoulderStrength,
				ActiveComp->HableLinearStrength,
				ActiveComp->HableLinearAngle,
				ActiveComp->HableToeStrength);
			LP->HableParams2 = FVector4f(
				ActiveComp->HableToeNumerator,
				ActiveComp->HableToeDenominator,
				ActiveComp->HableWhitePoint,
				0.0f);
			LP->ReinhardWhitePoint = ActiveComp->ReinhardWhitePoint;
			LP->CustomCurveTexture = RegisterExternalTextureOrFallback(
				GraphBuilder,
				ActiveComp->CustomToneCurveTexture,
				SceneColor.Texture,
				TEXT("ToneMapCustomToneCurveLUT"));
			LP->CustomCurveSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			LP->CustomCurveParams = FVector4f(
				ActiveComp->CustomCurveMinEV,
				1.0f / FMath::Max(ActiveComp->CustomCurveMaxEV - ActiveComp->CustomCurveMinEV, 0.01f),
				0.0f,
				0.0f);
			LP->HDRSaturation = ActiveComp->HDRSaturation;
			LP->HDRColorBalance = FVector3f(
				ActiveComp->HDRColorBalance.R,
				ActiveComp->HDRColorBalance.G,
				ActiveComp->HDRColorBalance.B);
			LP->AgXParams = FVector4f(
				ActiveComp->AgXMinEV,
				ActiveComp->AgXMaxEV,
				(float)static_cast<uint8>(ActiveComp->AgXLook),
				0.0f);
			LP->bPreToneMapped = bPreToneMapped ? 1.0f : 0.0f;

			// White Balance
			LP->Temperature = ActiveComp->bEnableWhiteBalance ? ActiveComp->Temperature : 0.0f;
			LP->Tint        = ActiveComp->bEnableWhiteBalance ? ActiveComp->Tint : 0.0f;

			// Exposure
			LP->ExposureValue = ActiveComp->Exposure;
			float CameraEVLUT = 0.0f;
			if (ActiveComp->bUseCameraExposure)
			{
				float N = FMath::Max(ActiveComp->Aperture, 1.0f);
				float t = 1.0f / FMath::Max(ActiveComp->ShutterSpeedDenominator, 1.0f);
				float S = FMath::Max(ActiveComp->CameraISO, 1.0f);
				float EV100 = FMath::Log2(N * N / t) + FMath::Log2(100.0f / S);
				const float ReferenceEV = FMath::Log2(5.6f * 5.6f / (1.0f / 125.0f)) + FMath::Log2(100.0f / 100.0f);
				CameraEVLUT = ReferenceEV - EV100;
			}
			LP->CameraEV = CameraEVLUT;
			LP->bUseCameraExposure = ActiveComp->bUseCameraExposure ? 1.0f : 0.0f;

			// Tone
			LP->Contrast        = ActiveComp->Contrast;
			LP->HighlightsValue = ActiveComp->bEnableToneAdjustments ? ActiveComp->Highlights : 0.0f;
			LP->ShadowsValue    = ActiveComp->bEnableToneAdjustments ? ActiveComp->Shadows : 0.0f;
			LP->WhitesValue     = ActiveComp->bEnableToneAdjustments ? ActiveComp->Whites : 0.0f;
			LP->BlacksValue     = ActiveComp->bEnableToneAdjustments ? ActiveComp->Blacks : 0.0f;
			LP->ToneSmoothingValue = ActiveComp->ToneSmoothing;
			LP->ContrastMidpoint   = ActiveComp->ContrastMidpoint;

			// Presence (non-spatial)
			LP->VibranceStrength   = ActiveComp->Vibrance;
			LP->SaturationStrength = ActiveComp->Saturation;

			// Tone Curve
			LP->ToneCurveParams = FVector4f(
				ActiveComp->CurveHighlights,
				ActiveComp->CurveLights,
				ActiveComp->CurveDarks,
				ActiveComp->CurveShadows);

			// HSL
			LP->HueShift1 = FVector4f(ActiveComp->HueReds, ActiveComp->HueOranges, ActiveComp->HueYellows, ActiveComp->HueGreens);
			LP->HueShift2 = FVector4f(ActiveComp->HueAquas, ActiveComp->HueBlues, ActiveComp->HuePurples, ActiveComp->HueMagentas);
			LP->SatAdj1   = FVector4f(ActiveComp->SatReds, ActiveComp->SatOranges, ActiveComp->SatYellows, ActiveComp->SatGreens);
			LP->SatAdj2   = FVector4f(ActiveComp->SatAquas, ActiveComp->SatBlues, ActiveComp->SatPurples, ActiveComp->SatMagentas);
			LP->LumAdj1   = FVector4f(ActiveComp->LumReds, ActiveComp->LumOranges, ActiveComp->LumYellows, ActiveComp->LumGreens);
			LP->LumAdj2   = FVector4f(ActiveComp->LumAquas, ActiveComp->LumBlues, ActiveComp->LumPurples, ActiveComp->LumMagentas);
			LP->HSLSmoothing = ActiveComp->HSLSmoothing;
			LP->HSLMode = static_cast<float>(static_cast<uint8>(ActiveComp->HSLMode));

			// Feature toggles
			LP->bEnableHSL    = ActiveComp->IsAnyHSLActive()  ? 1.0f : 0.0f;
			LP->bEnableCurves = ActiveComp->IsAnyCurveActive() ? 1.0f : 0.0f;

			LP->RenderTargets[0] = FRenderTargetBinding(BakedLUTTexture, ERenderTargetLoadAction::ENoAction);

			TShaderMapRef<FToneMapCombineLUTPS> CombineLUTShader(ViewInfo.ShaderMap);
			FPixelShaderUtils::AddFullscreenPass(
				GraphBuilder, ViewInfo.ShaderMap,
				RDG_EVENT_NAME("ToneMapCombineLUT"),
				CombineLUTShader, LP,
				FIntRect(0, 0, LUTTextureSize.X, LUTTextureSize.Y));
		}

		// --- Step 2: Apply the baked LUT + spatial ops ---
		{
			auto* AP = GraphBuilder.AllocParameters<FToneMapApplyLUTPS::FParameters>();
			AP->View = ViewInfo.ViewUniformBuffer;

			AP->SceneColorTexture = SceneColor.Texture;
			AP->SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

			AP->BakedLUTTexture = BakedLUTTexture;
			AP->BakedLUTSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			AP->LUTSize = LUTDim;
			AP->InvLUTSize = 1.0f / LUTDim;

			AP->bReplaceTonemap = bIsReplaceTonemap ? 1.0f : 0.0f;
			AP->bLinearHDROutput = bLinearHDROutput ? 1.0f : 0.0f;

			// Build screen transforms (same as per-pixel path)
			const FIntPoint OutputExtent = FIntPoint(OutputTarget.Texture->Desc.Extent.X, OutputTarget.Texture->Desc.Extent.Y);
			const FIntRect  OutputViewRect = OutputTarget.ViewRect;
			const FScreenPassTextureViewport OutputVP(OutputExtent, OutputViewRect);

			const FScreenPassTextureViewport SceneColorInputVP(
				FIntPoint(SceneColor.Texture->Desc.Extent.X, SceneColor.Texture->Desc.Extent.Y),
				SceneColorViewport.Rect);

			AP->SvPositionToSceneColorUV = (
				FScreenTransform::ChangeTextureBasisFromTo(OutputVP, FScreenTransform::ETextureBasis::TexelPosition, FScreenTransform::ETextureBasis::ViewportUV) *
				FScreenTransform::ChangeTextureBasisFromTo(SceneColorInputVP, FScreenTransform::ETextureBasis::ViewportUV, FScreenTransform::ETextureBasis::TextureUV));

			const FScreenPassTextureViewport BlurredVP(
				ViewportSize, FIntRect(0, 0, ViewportSize.X, ViewportSize.Y));

			AP->SvPositionToBlurredUV = (
				FScreenTransform::ChangeTextureBasisFromTo(OutputVP, FScreenTransform::ETextureBasis::TexelPosition, FScreenTransform::ETextureBasis::ViewportUV) *
				FScreenTransform::ChangeTextureBasisFromTo(BlurredVP, FScreenTransform::ETextureBasis::ViewportUV, FScreenTransform::ETextureBasis::TextureUV));

			// Output viewport rect
			AP->OutputViewportRect = FVector4f(
				(float)OutputViewRect.Min.X, (float)OutputViewRect.Min.Y,
				(float)OutputViewRect.Max.X, (float)OutputViewRect.Max.Y);

			// Bloom
			if (bIsReplaceTonemap && BloomInput.IsValid())
			{
				AP->BloomTexture = BloomInput.Texture;
				AP->BloomSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
				const FScreenPassTextureViewport BloomVP(BloomInput);
				AP->SvPositionToBloomUV = (
					FScreenTransform::ChangeTextureBasisFromTo(OutputVP, FScreenTransform::ETextureBasis::TexelPosition, FScreenTransform::ETextureBasis::ViewportUV) *
					FScreenTransform::ChangeTextureBasisFromTo(BloomVP, FScreenTransform::ETextureBasis::ViewportUV, FScreenTransform::ETextureBasis::TextureUV));
			}
			else
			{
				AP->BloomTexture = SceneColor.Texture;
				AP->BloomSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
				AP->SvPositionToBloomUV = AP->SvPositionToSceneColorUV;
			}

			// Exposure removal
			AP->OneOverPreExposure = 1.0f / FMath::Max(ViewInfo.PreExposure, 0.001f);
			AP->GlobalExposure = FMath::Max(View.GetLastEyeAdaptationExposure(), 0.001f);

			// Auto-Exposure
			AP->AutoExposureMode = (float)static_cast<uint8>(ActiveComp->AutoExposureMode);
			if (bNeedKrawczyk && AdaptedLumTexture)
			{
				AP->AdaptedLumTexture = AdaptedLumTexture;
			}
			else
			{
				AP->AdaptedLumTexture = SceneColor.Texture;
			}
			AP->AdaptedLumSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			AP->MinAutoExposure = ActiveComp->MinAutoExposure;
			AP->MaxAutoExposure = ActiveComp->MaxAutoExposure;

			// Clarity blur
			AP->BlurredTexture = BlurredTexture;
			AP->BlurredSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			AP->ClarityStrength = ActiveComp->Clarity;

			// Dynamic Contrast blur textures
			AP->BlurredFineTexture = BlurredFineTexture;
			AP->BlurredFineSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			AP->BlurredCoarseTexture = BlurredCoarseTexture;
			AP->BlurredCoarseSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			AP->SvPositionToBlurredFineUV = (
				FScreenTransform::ChangeTextureBasisFromTo(OutputVP, FScreenTransform::ETextureBasis::TexelPosition, FScreenTransform::ETextureBasis::ViewportUV) *
				FScreenTransform::ChangeTextureBasisFromTo(BlurredVP, FScreenTransform::ETextureBasis::ViewportUV, FScreenTransform::ETextureBasis::TextureUV));
			AP->SvPositionToBlurredCoarseUV = (
				FScreenTransform::ChangeTextureBasisFromTo(OutputVP, FScreenTransform::ETextureBasis::TexelPosition, FScreenTransform::ETextureBasis::ViewportUV) *
				FScreenTransform::ChangeTextureBasisFromTo(BlurredVP, FScreenTransform::ETextureBasis::ViewportUV, FScreenTransform::ETextureBasis::TextureUV));

			AP->DynamicContrastStrength  = ActiveComp->DynamicContrast;
			AP->CorrectContrastStrength  = ActiveComp->CorrectContrast;
			AP->CorrectColorCastStrength = ActiveComp->CorrectColorCast;

			// Pre-tone-mapped (Durand/Fattal)
			AP->bPreToneMapped = bPreToneMapped ? 1.0f : 0.0f;
			if (bPreToneMapped && PreToneMappedTexture)
			{
				AP->PreToneMappedTexture = PreToneMappedTexture;
				AP->PreToneMappedSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
				const FScreenPassTextureViewport PreTMVP(
					FIntPoint(PreToneMappedTexture->Desc.Extent.X, PreToneMappedTexture->Desc.Extent.Y),
					FIntRect(0, 0, PreToneMappedTexture->Desc.Extent.X, PreToneMappedTexture->Desc.Extent.Y));
				AP->SvPositionToPreToneMappedUV = (
					FScreenTransform::ChangeTextureBasisFromTo(OutputVP, FScreenTransform::ETextureBasis::TexelPosition, FScreenTransform::ETextureBasis::ViewportUV) *
					FScreenTransform::ChangeTextureBasisFromTo(PreTMVP, FScreenTransform::ETextureBasis::ViewportUV, FScreenTransform::ETextureBasis::TextureUV));
			}
			else
			{
				AP->PreToneMappedTexture = SceneColor.Texture;
				AP->PreToneMappedSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
				AP->SvPositionToPreToneMappedUV = AP->SvPositionToSceneColorUV;
			}

			// Dithering
			AP->DitherQuantization = bToneMapIsLast ? DitherQuantizationValue : 0.0f;

			AP->RenderTargets[0] = FRenderTargetBinding(OutputTarget.Texture, OutputTarget.LoadAction);

			TShaderMapRef<FToneMapApplyLUTPS> ApplyLUTShader(ViewInfo.ShaderMap);
			FPixelShaderUtils::AddFullscreenPass(
				GraphBuilder, ViewInfo.ShaderMap,
				RDG_EVENT_NAME("ToneMapApplyLUT"),
				ApplyLUTShader, AP,
				OutputTarget.ViewRect);
		}
	}

	// =====================================================================
	// Sharpening pass — runs after ToneMapProcess, before LUT
	// =====================================================================
	if (bNeedSharpening)
	{
		// Determine output: another intermediate if LUT/Vignette/HDR follows,
		// otherwise write directly to FinalOutputTarget
		FScreenPassRenderTarget SharpenOutputTarget;
		if (bNeedLUT || bNeedVignette || bNeedHDREncode)
		{
			SharpenOutputTarget = FScreenPassRenderTarget(
				GraphBuilder.CreateTexture(
					FRDGTextureDesc::Create2D(
						ViewportSize, PF_FloatRGBA,
						FClearValueBinding::None,
						TexCreate_ShaderResource | TexCreate_RenderTargetable),
					TEXT("ToneMap.PreLUT")),
				FIntRect(0, 0, ViewportSize.X, ViewportSize.Y),
				ERenderTargetLoadAction::ENoAction);
		}
		else
		{
			SharpenOutputTarget = FinalOutputTarget;
		}

		auto* SP = GraphBuilder.AllocParameters<FToneMapSharpenPS::FParameters>();
		SP->View              = ViewInfo.ViewUniformBuffer;
		SP->SceneColorTexture = OutputTarget.Texture;  // ToneMapProcess output
		SP->SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

		// UV transform: SvPosition in SharpenOutputTarget → UV in ToneMapProcess intermediate
		const FIntPoint SharpenOutExtent(SharpenOutputTarget.Texture->Desc.Extent.X,
		                                 SharpenOutputTarget.Texture->Desc.Extent.Y);
		const FScreenPassTextureViewport SharpenOutVP(SharpenOutExtent, SharpenOutputTarget.ViewRect);
		const FScreenPassTextureViewport PreSharpenVP(
			ViewportSize, FIntRect(0, 0, ViewportSize.X, ViewportSize.Y));

		SP->SvPositionToSceneColorUV = (
			FScreenTransform::ChangeTextureBasisFromTo(SharpenOutVP,
				FScreenTransform::ETextureBasis::TexelPosition,
				FScreenTransform::ETextureBasis::ViewportUV) *
			FScreenTransform::ChangeTextureBasisFromTo(PreSharpenVP,
				FScreenTransform::ETextureBasis::ViewportUV,
				FScreenTransform::ETextureBasis::TextureUV));

		SP->SharpenAmount = ActiveComp->SharpenAmount;
		SP->SharpenRadius = ActiveComp->SharpenRadius;
		SP->SharpenMethod = static_cast<float>(static_cast<uint8>(ActiveComp->SharpenMethod));
		SP->CASInputRange = bLinearHDROutput
			? FMath::Max(HDRMaxDisplayNits / FMath::Max(ActiveComp->PaperWhiteNits, 1.0f), 1.0f)
			: 1.0f;
		SP->TexelSize     = FVector2f(1.0f / ViewportSize.X, 1.0f / ViewportSize.Y);
		SP->bLinearHDROutput = bLinearHDROutput ? 1.0f : 0.0f;
		SP->DitherQuantization = bSharpenIsLast ? DitherQuantizationValue : 0.0f;

		SP->RenderTargets[0] = FRenderTargetBinding(SharpenOutputTarget.Texture, SharpenOutputTarget.LoadAction);

		TShaderMapRef<FToneMapSharpenPS> SharpenShader(ViewInfo.ShaderMap);
		FPixelShaderUtils::AddFullscreenPass(
			GraphBuilder, ViewInfo.ShaderMap,
			RDG_EVENT_NAME("ToneMapSharpen"),
			SharpenShader, SP,
			SharpenOutputTarget.ViewRect);

		// Update OutputTarget so LUT/Vignette/HDR reads from sharpened output
		OutputTarget = SharpenOutputTarget;
	}

	// =====================================================================
	// LUT pass — runs after ToneMapProcess (post-tonemap, post-sRGB)
	// =====================================================================
	if (bNeedLUT)
	{
		// Determine LUT output target: another intermediate if vignette follows,
		// otherwise write directly to FinalOutputTarget
		FScreenPassRenderTarget LUTOutputTarget;
		if (bNeedVignette || bNeedHDREncode)
		{
			LUTOutputTarget = FScreenPassRenderTarget(
				GraphBuilder.CreateTexture(
					FRDGTextureDesc::Create2D(
						ViewportSize, PF_FloatRGBA,
						FClearValueBinding::None,
						TexCreate_ShaderResource | TexCreate_RenderTargetable),
					TEXT("ToneMap.PreVignette")),
				FIntRect(0, 0, ViewportSize.X, ViewportSize.Y),
				ERenderTargetLoadAction::ENoAction);
		}
		else
		{
			LUTOutputTarget = FinalOutputTarget;
		}

		auto* LP = GraphBuilder.AllocParameters<FToneMapLUTPS::FParameters>();
		LP->View              = ViewInfo.ViewUniformBuffer;
		LP->SceneColorTexture = OutputTarget.Texture;  // ToneMapProcess output
		LP->SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

		// UV transform: SvPosition in LUTOutputTarget → UV in ToneMapProcess intermediate
		const FIntPoint LUTOutExtent(LUTOutputTarget.Texture->Desc.Extent.X,
		                             LUTOutputTarget.Texture->Desc.Extent.Y);
		const FScreenPassTextureViewport LUTOutVP(LUTOutExtent, LUTOutputTarget.ViewRect);
		const FScreenPassTextureViewport PreLUTVP(
			ViewportSize, FIntRect(0, 0, ViewportSize.X, ViewportSize.Y));

		LP->SvPositionToSceneColorUV = (
			FScreenTransform::ChangeTextureBasisFromTo(LUTOutVP,
				FScreenTransform::ETextureBasis::TexelPosition,
				FScreenTransform::ETextureBasis::ViewportUV) *
			FScreenTransform::ChangeTextureBasisFromTo(PreLUTVP,
				FScreenTransform::ETextureBasis::ViewportUV,
				FScreenTransform::ETextureBasis::TextureUV));

		// LUT texture — detect dimensions to determine cube size
		FRHITexture* LUTRHI = ActiveComp->LUTTexture->GetResource()->TextureRHI;
		FRDGTextureRef LUTTex = GraphBuilder.RegisterExternalTexture(
			CreateRenderTarget(LUTRHI, TEXT("ToneMapLUTTex")));
		LP->LUTTexture = LUTTex;
		LP->LUTSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

		// LUT cube dimension = texture height (256×16→16, 1024×32→32, 4096×64→64)
		const float LUTSize = (float)LUTRHI->GetSizeXYZ().Y;
		LP->LUTSize      = LUTSize;
		LP->InvLUTSize    = 1.0f / FMath::Max(LUTSize, 1.0f);
		LP->LUTIntensity  = ActiveComp->LUTIntensity;
		LP->DitherQuantization = bLUTIsLast ? DitherQuantizationValue : 0.0f;

		LP->RenderTargets[0] = FRenderTargetBinding(LUTOutputTarget.Texture, LUTOutputTarget.LoadAction);

		TShaderMapRef<FToneMapLUTPS> LUTShader(ViewInfo.ShaderMap);
		FPixelShaderUtils::AddFullscreenPass(
			GraphBuilder, ViewInfo.ShaderMap,
			RDG_EVENT_NAME("ToneMapLUT"),
			LUTShader, LP,
			LUTOutputTarget.ViewRect);

		// Update OutputTarget so the next pass (vignette) reads from LUT output
		OutputTarget = LUTOutputTarget;
	}

	// =====================================================================
	// Vignette pass — runs after LUT (or ToneMapProcess if no LUT)
	// =====================================================================
	if (bNeedVignette)
	{
		// Determine Vignette output: another intermediate if HDR encode follows,
		// otherwise write directly to FinalOutputTarget.
		FScreenPassRenderTarget VignetteOutputTarget;
		if (bNeedHDREncode)
		{
			VignetteOutputTarget = FScreenPassRenderTarget(
				GraphBuilder.CreateTexture(
					FRDGTextureDesc::Create2D(
						ViewportSize, PF_FloatRGBA,
						FClearValueBinding::None,
						TexCreate_ShaderResource | TexCreate_RenderTargetable),
					TEXT("ToneMap.PreHDREncode")),
				FIntRect(0, 0, ViewportSize.X, ViewportSize.Y),
				ERenderTargetLoadAction::ENoAction);
		}
		else
		{
			VignetteOutputTarget = FinalOutputTarget;
		}

		auto* VP = GraphBuilder.AllocParameters<FToneMapVignettePS::FParameters>();
		VP->View              = ViewInfo.ViewUniformBuffer;
		VP->SceneColorTexture = OutputTarget.Texture;  // LUT output (or ToneMapProcess if no LUT)
		VP->SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

		// UV transform: SvPosition in VignetteOutputTarget → UV in previous pass intermediate
		const FIntPoint VigOutExtent(VignetteOutputTarget.Texture->Desc.Extent.X,
		                               VignetteOutputTarget.Texture->Desc.Extent.Y);
		const FScreenPassTextureViewport VigOutVP(VigOutExtent, VignetteOutputTarget.ViewRect);
		const FScreenPassTextureViewport PreVignetteVP(
			ViewportSize, FIntRect(0, 0, ViewportSize.X, ViewportSize.Y));

		VP->SvPositionToSceneColorUV = (
			FScreenTransform::ChangeTextureBasisFromTo(VigOutVP,
				FScreenTransform::ETextureBasis::TexelPosition,
				FScreenTransform::ETextureBasis::ViewportUV) *
			FScreenTransform::ChangeTextureBasisFromTo(PreVignetteVP,
				FScreenTransform::ETextureBasis::ViewportUV,
				FScreenTransform::ETextureBasis::TextureUV));

		// Vignette parameters: Mode, Size, Intensity
		VP->VignetteParams = FVector4f(
			(float)static_cast<uint8>(ActiveComp->VignetteMode),
			ActiveComp->VignetteSize,
			ActiveComp->VignetteIntensity,
			(float)static_cast<uint8>(ActiveComp->VignetteFalloff));
		VP->FalloffExponent = ActiveComp->VignetteFalloffExponent;
		VP->bLinearHDROutput = bLinearHDROutput ? 1.0f : 0.0f;

		// Alpha texture (optional)
		const bool bHasAlphaTex = ActiveComp->bVignetteUseAlphaTexture
			&& ActiveComp->VignetteAlphaTexture != nullptr
			&& ActiveComp->VignetteAlphaTexture->GetResource() != nullptr
			&& ActiveComp->VignetteAlphaTexture->GetResource()->TextureRHI != nullptr;

		VP->bUseAlphaTexture  = bHasAlphaTex ? 1.0f : 0.0f;
		VP->bAlphaTextureOnly = (bHasAlphaTex && ActiveComp->bVignetteAlphaTextureOnly) ? 1.0f : 0.0f;
		VP->TextureChannelIndex = (float)static_cast<uint8>(ActiveComp->VignetteTextureChannel);

		if (bHasAlphaTex)
		{
			FRHITexture* AlphaRHI = ActiveComp->VignetteAlphaTexture->GetResource()->TextureRHI;
			VP->AlphaTexture = GraphBuilder.RegisterExternalTexture(
				CreateRenderTarget(AlphaRHI, TEXT("VignetteAlphaTex")));
		}
		else
		{
			// Safe fallback — won't be sampled when bUseAlphaTexture == 0
			VP->AlphaTexture = OutputTarget.Texture;
		}
		VP->AlphaSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		VP->DitherQuantization = bVignetteIsLast ? DitherQuantizationValue : 0.0f;

		VP->RenderTargets[0] = FRenderTargetBinding(VignetteOutputTarget.Texture, VignetteOutputTarget.LoadAction);

		TShaderMapRef<FToneMapVignettePS> VignetteShader(ViewInfo.ShaderMap);
		FPixelShaderUtils::AddFullscreenPass(
			GraphBuilder, ViewInfo.ShaderMap,
			RDG_EVENT_NAME("ToneMapVignette"),
			VignetteShader, VP,
			VignetteOutputTarget.ViewRect);

		// Update OutputTarget so the HDR encode pass (if any) reads from Vignette output
		OutputTarget = VignetteOutputTarget;
	}

	// =====================================================================
	// HDR Output Encoding — final pass (ST2084/PQ or scRGB)
	//
	// Converts sRGB-encoded output to the display's native HDR format.
	// Only runs when the display is actually in HDR mode (OutputDevice >= 3).
	// =====================================================================
	if (bNeedHDREncode)
	{
		RDG_EVENT_SCOPE(GraphBuilder, "ToneMapFX_HDREncode");

		auto* HP = GraphBuilder.AllocParameters<FToneMapHDREncodePS::FParameters>();
		HP->View              = ViewInfo.ViewUniformBuffer;
		HP->SceneColorTexture = OutputTarget.Texture;
		HP->SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

		// UV transform: SvPosition in FinalOutputTarget → UV in previous pass
		const FIntPoint HDROutExtent(FinalOutputTarget.Texture->Desc.Extent.X,
		                              FinalOutputTarget.Texture->Desc.Extent.Y);
		const FScreenPassTextureViewport HDROutVP(HDROutExtent, FinalOutputTarget.ViewRect);
		const FScreenPassTextureViewport PreHDRVP(
			FIntPoint(OutputTarget.Texture->Desc.Extent.X, OutputTarget.Texture->Desc.Extent.Y),
			FIntRect(0, 0, OutputTarget.Texture->Desc.Extent.X, OutputTarget.Texture->Desc.Extent.Y));

		HP->SvPositionToSceneColorUV = (
			FScreenTransform::ChangeTextureBasisFromTo(HDROutVP,
				FScreenTransform::ETextureBasis::TexelPosition,
				FScreenTransform::ETextureBasis::ViewportUV) *
			FScreenTransform::ChangeTextureBasisFromTo(PreHDRVP,
				FScreenTransform::ETextureBasis::ViewportUV,
				FScreenTransform::ETextureBasis::TextureUV));

		HP->OutputDeviceType = (float)HDROutputDevice;
		HP->bInputIsLinearHDR = bLinearHDROutput ? 1.0f : 0.0f;
		HP->PaperWhiteNits   = ActiveComp->PaperWhiteNits;
		HP->MaxDisplayNits   = HDRMaxDisplayNits;
		HP->DitherQuantization = bHDREncodeIsLast ? DitherQuantizationValue : 0.0f;

		HP->RenderTargets[0] = FRenderTargetBinding(FinalOutputTarget.Texture, FinalOutputTarget.LoadAction);

		TShaderMapRef<FToneMapHDREncodePS> HDRShader(ViewInfo.ShaderMap);
		FPixelShaderUtils::AddFullscreenPass(
			GraphBuilder, ViewInfo.ShaderMap,
			RDG_EVENT_NAME("HDREncode"),
			HDRShader, HP,
			FinalOutputTarget.ViewRect);
	}

	return FScreenPassTexture(FinalOutputTarget.Texture, FinalOutputTarget.ViewRect);
}

// =============================================================================
// UToneMapSubsystem
// =============================================================================

void UToneMapSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	SceneViewExtension = FSceneViewExtensions::NewExtension<FToneMapSceneViewExtension>(this);
}

void UToneMapSubsystem::Deinitialize()
{
	SceneViewExtension.Reset();
	Super::Deinitialize();
}

void UToneMapSubsystem::RegisterComponent(UToneMapComponent* Component)
{
	if (Component)
	{
		Components.AddUnique(Component);
	}
}

void UToneMapSubsystem::UnregisterComponent(UToneMapComponent* Component)
{
	if (Component)
	{
		Components.Remove(Component);
	}
}
