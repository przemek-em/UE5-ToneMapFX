// Licensed under the zlib License. See LICENSE file in the project root.

#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "ScreenPass.h"

// =============================================================================
// CombineLUT — Bakes all non-spatial color operations into a 32^3 LUT
//   Generates a 1024×32 (PF_FloatRGBA) texture where each texel encodes
//   the result of the full color-grading chain for a given input color.
//   Spatial operations (Clarity, Dynamic Contrast, etc.) are NOT baked.
// =============================================================================
class FToneMapCombineLUTPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FToneMapCombineLUTPS);
	SHADER_USE_PARAMETER_STRUCT(FToneMapCombineLUTPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)

		// LUT grid dimension (32)
		SHADER_PARAMETER(float, LUTSize)

		// Mode: 0 = PostProcess (LDR), 1 = ReplaceTonemap (HDR)
		SHADER_PARAMETER(float, bReplaceTonemap)

		// Film Curve params (ReplaceTonemap mode)
		SHADER_PARAMETER(float, FilmCurveMode)
		SHADER_PARAMETER(FVector4f, HableParams1)
		SHADER_PARAMETER(FVector4f, HableParams2)
		SHADER_PARAMETER(float, ReinhardWhitePoint)
		SHADER_PARAMETER(float, HDRSaturation)
		SHADER_PARAMETER(FVector3f, HDRColorBalance)
		SHADER_PARAMETER(FVector4f, AgXParams)
		SHADER_PARAMETER(float, bPreToneMapped)

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

		// Presence (non-spatial only)
		SHADER_PARAMETER(float, VibranceStrength)
		SHADER_PARAMETER(float, SaturationStrength)

		// Tone Curve
		SHADER_PARAMETER(FVector4f, ToneCurveParams)

		// HSL
		SHADER_PARAMETER(FVector4f, HueShift1)
		SHADER_PARAMETER(FVector4f, HueShift2)
		SHADER_PARAMETER(FVector4f, SatAdj1)
		SHADER_PARAMETER(FVector4f, SatAdj2)
		SHADER_PARAMETER(FVector4f, LumAdj1)
		SHADER_PARAMETER(FVector4f, LumAdj2)
		SHADER_PARAMETER(float, HSLSmoothing)

		// Feature toggles
		SHADER_PARAMETER(float, bEnableHSL)
		SHADER_PARAMETER(float, bEnableCurves)

		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

// =============================================================================
// ApplyLUT — Samples the baked LUT + applies spatial operations
//   Reads the 32^3 baked LUT and does a trilinear lookup for each pixel,
//   then composites spatial effects (Clarity, Dynamic Contrast) on top.
// =============================================================================
class FToneMapApplyLUTPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FToneMapApplyLUTPS);
	SHADER_USE_PARAMETER_STRUCT(FToneMapApplyLUTPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)

		// Scene color input
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneColorTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SceneColorSampler)
		SHADER_PARAMETER(FScreenTransform, SvPositionToSceneColorUV)

		// Baked LUT texture (1024×32, PF_FloatRGBA)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, BakedLUTTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, BakedLUTSampler)
		SHADER_PARAMETER(float, LUTSize)
		SHADER_PARAMETER(float, InvLUTSize)

		// Mode: 0 = PostProcess (LDR), 1 = ReplaceTonemap (HDR)
		SHADER_PARAMETER(float, bReplaceTonemap)

		// Bloom (ReplaceTonemap mode)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, BloomTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, BloomSampler)
		SHADER_PARAMETER(FScreenTransform, SvPositionToBloomUV)

		// Exposure removal (ReplaceTonemap mode)
		SHADER_PARAMETER(float, OneOverPreExposure)
		SHADER_PARAMETER(float, GlobalExposure)

		// Auto-Exposure
		SHADER_PARAMETER(float, AutoExposureMode)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AdaptedLumTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, AdaptedLumSampler)
		SHADER_PARAMETER(float, MinAutoExposure)
		SHADER_PARAMETER(float, MaxAutoExposure)

		// Clarity blur
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, BlurredTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, BlurredSampler)
		SHADER_PARAMETER(FScreenTransform, SvPositionToBlurredUV)
		SHADER_PARAMETER(float, ClarityStrength)

		// Dynamic Contrast blur textures
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, BlurredFineTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, BlurredFineSampler)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, BlurredCoarseTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, BlurredCoarseSampler)
		SHADER_PARAMETER(FScreenTransform, SvPositionToBlurredFineUV)
		SHADER_PARAMETER(FScreenTransform, SvPositionToBlurredCoarseUV)

		// Dynamic Contrast strengths
		SHADER_PARAMETER(float, DynamicContrastStrength)
		SHADER_PARAMETER(float, CorrectContrastStrength)
		SHADER_PARAMETER(float, CorrectColorCastStrength)

		// Pre-tone-mapped (Durand/Fattal bypass)
		SHADER_PARAMETER(float, bPreToneMapped)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, PreToneMappedTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, PreToneMappedSampler)
		SHADER_PARAMETER(FScreenTransform, SvPositionToPreToneMappedUV)

		// Dithering
		SHADER_PARAMETER(float, DitherQuantization)

		// Output viewport
		SHADER_PARAMETER(FVector4f, OutputViewportRect)

		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};
