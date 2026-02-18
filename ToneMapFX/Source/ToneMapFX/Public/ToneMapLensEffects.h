// Licensed under the zlib License. See LICENSE file in the project root.

#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "ScreenPass.h"

// =============================================================================
// Lens Effects — Shared bright-pass for both corona and halo
//   Output: RGBA16F bright pixels only (below threshold = black)
// =============================================================================
class FToneMapLensBrightPassPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FToneMapLensBrightPassPS);
	SHADER_USE_PARAMETER_STRUCT(FToneMapLensBrightPassPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneColorTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SceneColorSampler)
		SHADER_PARAMETER(FScreenTransform, SvPositionToSceneColorUV)
		SHADER_PARAMETER(float, Threshold)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

// =============================================================================
// Ciliary Corona — directional spike streak accumulation
//   For each arm direction θ_i = i * π/SpikeCount, accumulates a 1-D gather
//   along that axis with sinc/Hann-windowed falloff.
//   All arms are summed into one pass (up to MAX_CORONA_SPIKES directions).
//   Output: RGBA16F corona layer
// =============================================================================
class FToneMapCoronaStreakPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FToneMapCoronaStreakPS);
	SHADER_USE_PARAMETER_STRUCT(FToneMapCoronaStreakPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, BrightPassTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, BrightPassSampler)
		SHADER_PARAMETER(FScreenTransform, SvPositionToBrightPassUV)
		SHADER_PARAMETER(FVector4f, BufferSizeAndInvSize)   // xy=bp size, zw=1/bp size
		SHADER_PARAMETER(int32, SpikeCount)                 // total arms (e.g. 6)
		SHADER_PARAMETER(int32, SpikeLength)                // half-length in bp pixels
		SHADER_PARAMETER(float, CoronaIntensity)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

// =============================================================================
// Lenticular Halo — ring-shaped scatter around bright sources
//   For each pixel, accumulates sample contributions from a circular annulus
//   around it (radius ±thickness) weighted by source brightness.
//   A fast approximation uses a large-kernel box + subtraction to isolate an annulus.
//   Output: RGBA16F halo layer
// =============================================================================
class FToneMapHaloRingPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FToneMapHaloRingPS);
	SHADER_USE_PARAMETER_STRUCT(FToneMapHaloRingPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, BrightPassTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, BrightPassSampler)
		SHADER_PARAMETER(FScreenTransform, SvPositionToBrightPassUV)
		SHADER_PARAMETER(FVector4f, BufferSizeAndInvSize)
		SHADER_PARAMETER(float, HaloRadius)     // UV units
		SHADER_PARAMETER(float, HaloThickness)  // UV units (ring width)
		SHADER_PARAMETER(float, HaloIntensity)
		SHADER_PARAMETER(FVector3f, HaloTint)   // RGB tint for the ring
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

// =============================================================================
// Composite: blend corona + halo layers back onto scene colour
//   Uses additive blending scaled by per-effect intensity;
//   avoids brightening already-white areas (uses screen blend cap).
//   Output: final composited RGBA
// =============================================================================
class FToneMapLensCompositePS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FToneMapLensCompositePS);
	SHADER_USE_PARAMETER_STRUCT(FToneMapLensCompositePS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneColorTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SceneColorSampler)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, CoronaTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, CoronaSampler)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, HaloTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, HaloSampler)
		SHADER_PARAMETER(FScreenTransform, SvPositionToSceneColorUV)
		SHADER_PARAMETER(FScreenTransform, SvPositionToLensUV)
		SHADER_PARAMETER(float, bEnableCorona)
		SHADER_PARAMETER(float, bEnableHalo)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};
