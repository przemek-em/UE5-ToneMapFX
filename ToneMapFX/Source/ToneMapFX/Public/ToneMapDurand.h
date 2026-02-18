// Licensed under the zlib License. See LICENSE file in the project root.

#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "ScreenPass.h"

// =============================================================================
// Durand & Dorsey 2002 — Pass 1: Compute log-luminance map
//   Input:  HDR scene color (Texture2D)
//   Output: R32F log-luminance texture (same or half resolution)
// =============================================================================
class FToneMapDurandLogLumPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FToneMapDurandLogLumPS);
	SHADER_USE_PARAMETER_STRUCT(FToneMapDurandLogLumPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneColorTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SceneColorSampler)
		SHADER_PARAMETER(FScreenTransform, SvPositionToSceneColorUV)
		SHADER_PARAMETER(float, OneOverPreExposure)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

// =============================================================================
// Durand & Dorsey 2002 — Pass 2: Cross-bilateral filter on log-lum (base layer)
//   The bilateral filter is separable-approximated with multiple 1-D passes.
//   Output: R32F blurred base-layer texture
// =============================================================================
class FToneMapDurandBilateralPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FToneMapDurandBilateralPS);
	SHADER_USE_PARAMETER_STRUCT(FToneMapDurandBilateralPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		// Log-lum input for current pass (ping-pong)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, LogLumTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, LogLumSampler)
		// Full-resolution guide image (scene color luminance, un-blurred)
		// Used as the range-weight guide to stay edge-aware
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, GuideTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, GuideSampler)
		SHADER_PARAMETER(FVector4f, BufferSizeAndInvSize)  // xy=size, zw=1/size
		SHADER_PARAMETER(FVector2f, BlurDirection)         // (1,0) or (0,1)
		SHADER_PARAMETER(float, SpatialSigma)             // σ_s in pixels
		SHADER_PARAMETER(float, RangeSigma)               // σ_r in log-lum units
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

// =============================================================================
// Durand & Dorsey 2002 — Pass 3: Reconstruction
//   detail = logLum - baseLayer
//   outputLogLum = baseLayer * compression + detail * detailBoost + offset
//   Then reconstruct linear RGB: apply exp(outputLogLum), restore chrominance.
//   Output: RGBA16F / RGBA8 tone-mapped image (bPreToneMapped = 1)
// =============================================================================
class FToneMapDurandReconstructPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FToneMapDurandReconstructPS);
	SHADER_USE_PARAMETER_STRUCT(FToneMapDurandReconstructPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneColorTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SceneColorSampler)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, LogLumTexture)      // original log-lum
		SHADER_PARAMETER_SAMPLER(SamplerState, LogLumSampler)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, BaseLayerTexture)   // bilateral-filtered base
		SHADER_PARAMETER_SAMPLER(SamplerState, BaseLayerSampler)
		SHADER_PARAMETER(FScreenTransform, SvPositionToSceneColorUV)
		SHADER_PARAMETER(FVector4f, BufferSizeAndInvSize)  // xy=size, zw=1/size for work-texture UV
		SHADER_PARAMETER(float, OneOverPreExposure)
		SHADER_PARAMETER(float, BaseCompression)     // scales base layer (< 1 compresses DR)
		SHADER_PARAMETER(float, DetailBoost)         // scales detail layer
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};
