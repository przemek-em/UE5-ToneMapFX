// Licensed under the zlib License. See LICENSE file in the project root.

#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "ScreenPass.h"

// =============================================================================
// Gaussian blur shader for Clarity (separable horizontal/vertical)
// =============================================================================
class FToneMapBlurPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FToneMapBlurPS);
	SHADER_USE_PARAMETER_STRUCT(FToneMapBlurPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SourceTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SourceSampler)
		SHADER_PARAMETER(FVector4f, BufferSizeAndInvSize)
		SHADER_PARAMETER(FVector2f, BlurDirection)
		SHADER_PARAMETER(float, BlurRadius)
		SHADER_PARAMETER(FVector4f, SourceViewportRect)
		SHADER_PARAMETER(FVector4f, SourceExtentInv)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

// =============================================================================
// Main Tone Map processing shader — all adjustments in a single pass
// =============================================================================
class FToneMapProcessPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FToneMapProcessPS);
	SHADER_USE_PARAMETER_STRUCT(FToneMapProcessPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneColorTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SceneColorSampler)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, BlurredTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, BlurredSampler)

		// FScreenTransform properly handles viewport offsets (fixes resize glitches)
		SHADER_PARAMETER(FScreenTransform, SvPositionToSceneColorUV)
		SHADER_PARAMETER(FScreenTransform, SvPositionToBlurredUV)
		SHADER_PARAMETER(FVector4f, OutputViewportRect) // xy = Min, zw = Max (for split screen)

		// Bloom (ReplaceTonemap mode)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, BloomTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, BloomSampler)
		SHADER_PARAMETER(FScreenTransform, SvPositionToBloomUV)

		// Mode: 0 = PostProcess (LDR), 1 = ReplaceTonemap (HDR)
		SHADER_PARAMETER(float, bReplaceTonemap)

		// Exposure removal (ReplaceTonemap mode)
		SHADER_PARAMETER(float, OneOverPreExposure)
		SHADER_PARAMETER(float, GlobalExposure)

		// Film Curve params (ReplaceTonemap mode)
		SHADER_PARAMETER(float, FilmCurveMode) // 0=Hable, 1=ReinhardLum, 2=ReinhardJodie, 3=ReinhardStd, 4=Durand, 5=Fattal
		SHADER_PARAMETER(FVector4f, HableParams1) // x=A(Shoulder), y=B(Linear), z=C(LinearAngle), w=D(ToeStrength)
		SHADER_PARAMETER(FVector4f, HableParams2) // x=E(ToeNum), y=F(ToeDenom), z=W(WhitePoint), w=unused
		SHADER_PARAMETER(float, ReinhardWhitePoint)
		SHADER_PARAMETER(float, HDRSaturation)
		SHADER_PARAMETER(FVector3f, HDRColorBalance)

		// Pre-tone-mapped bypass (Durand / Fattal multi-pass operators)
		// When bPreToneMapped > 0.5, ApplyFilmCurve is skipped and PreToneMappedTexture is
		// composited directly.  All color-grading, sRGB conversion, dithering still run.
		SHADER_PARAMETER(float, bPreToneMapped)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, PreToneMappedTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, PreToneMappedSampler)
		SHADER_PARAMETER(FScreenTransform, SvPositionToPreToneMappedUV)

		// Auto-Exposure (ReplaceTonemap mode)
		SHADER_PARAMETER(float, AutoExposureMode) // 0=None, 1=EngineDefault, 2=Krawczyk
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AdaptedLumTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, AdaptedLumSampler)
		SHADER_PARAMETER(float, MinAutoExposure)
		SHADER_PARAMETER(float, MaxAutoExposure)

		// White Balance
		SHADER_PARAMETER(float, Temperature)
		SHADER_PARAMETER(float, Tint)

		// Exposure
		SHADER_PARAMETER(float, ExposureValue)
		SHADER_PARAMETER(float, CameraEV)
		SHADER_PARAMETER(float, bUseCameraExposure)

		// Tone
		SHADER_PARAMETER(float, Contrast)
		SHADER_PARAMETER(float, HighlightsValue)
		SHADER_PARAMETER(float, ShadowsValue)
		SHADER_PARAMETER(float, WhitesValue)
		SHADER_PARAMETER(float, BlacksValue)
		SHADER_PARAMETER(float, ToneSmoothingValue)
		SHADER_PARAMETER(float, ContrastMidpoint)

		// Presence
		SHADER_PARAMETER(float, ClarityStrength)
		SHADER_PARAMETER(float, VibranceStrength)
		SHADER_PARAMETER(float, SaturationStrength)

		// Dynamic Contrast — multi-scale blur textures
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, BlurredFineTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, BlurredFineSampler)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, BlurredCoarseTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, BlurredCoarseSampler)
		SHADER_PARAMETER(FScreenTransform, SvPositionToBlurredFineUV)
		SHADER_PARAMETER(FScreenTransform, SvPositionToBlurredCoarseUV)

		// Dynamic Contrast — strengths
		SHADER_PARAMETER(float, DynamicContrastStrength)
		SHADER_PARAMETER(float, CorrectContrastStrength)
		SHADER_PARAMETER(float, CorrectColorCastStrength)

		// Tone Curve (x=Highlights, y=Lights, z=Darks, w=Shadows)
		SHADER_PARAMETER(FVector4f, ToneCurveParams)

		// HSL — packed per-color adjustments
		// float4(Reds, Oranges, Yellows, Greens)
		// float4(Aquas, Blues, Purples, Magentas)
		SHADER_PARAMETER(FVector4f, HueShift1)
		SHADER_PARAMETER(FVector4f, HueShift2)
		SHADER_PARAMETER(FVector4f, SatAdj1)
		SHADER_PARAMETER(FVector4f, SatAdj2)
		SHADER_PARAMETER(FVector4f, LumAdj1)
		SHADER_PARAMETER(FVector4f, LumAdj2)

		// HSL range smoothing
		SHADER_PARAMETER(float, HSLSmoothing)

		// Feature toggles & debug
		SHADER_PARAMETER(float, bEnableHSL)
		SHADER_PARAMETER(float, bEnableCurves)
		SHADER_PARAMETER(float, BlendAmount)
		SHADER_PARAMETER(float, bSplitScreen)

		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

// =============================================================================
// Luminance measurement shader (for Krawczyk auto-exposure)
// =============================================================================
class FToneMapLumMeasurePS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FToneMapLumMeasurePS);
	SHADER_USE_PARAMETER_STRUCT(FToneMapLumMeasurePS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneColorTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SceneColorSampler)
		SHADER_PARAMETER(FVector4f, SceneColorUVBounds)
		SHADER_PARAMETER(float, OneOverPreExposure)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

// =============================================================================
// Luminance temporal adaptation shader
// =============================================================================
class FToneMapLumAdaptPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FToneMapLumAdaptPS);
	SHADER_USE_PARAMETER_STRUCT(FToneMapLumAdaptPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, PrevAdaptedLumTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, PrevAdaptedLumSampler)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, CurrentLumTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, CurrentLumSampler)
		SHADER_PARAMETER(float, AdaptSpeedUp)
		SHADER_PARAMETER(float, AdaptSpeedDown)
		SHADER_PARAMETER(float, DeltaTime)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};
