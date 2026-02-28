// Licensed under the zlib License. See LICENSE file in the project root.

#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "ScreenPass.h"

// Bright pass shader - extracts bright pixels for bloom
class FClassicBloomBrightPassPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FClassicBloomBrightPassPS);
	SHADER_USE_PARAMETER_STRUCT(FClassicBloomBrightPassPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneColorTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SceneColorSampler)
		SHADER_PARAMETER(FVector4f, InputViewportSizeAndInvSize)
		SHADER_PARAMETER(FVector4f, OutputViewportSizeAndInvSize)
		SHADER_PARAMETER(FScreenTransform, SvPositionToInputTextureUV) // Transform SvPosition to scene color texture UV
		SHADER_PARAMETER(float, BloomThreshold)
		SHADER_PARAMETER(float, BloomIntensity)
		SHADER_PARAMETER(float, ThresholdSoftness) // 0..1 — 0=hard cutoff, 1=very wide/soft knee
		SHADER_PARAMETER(float, MaxBrightness)    // Clamp extreme HDR values (0=no clamp)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

// Gaussian blur shader - separable (does horizontal or vertical)
class FClassicBloomBlurPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FClassicBloomBlurPS);
	SHADER_USE_PARAMETER_STRUCT(FClassicBloomBlurPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SourceTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SourceSampler)
		SHADER_PARAMETER(FVector4f, BufferSizeAndInvSize)
		SHADER_PARAMETER(FVector2f, BlurDirection)
		SHADER_PARAMETER(float, BlurRadius)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

// Composite bloom shader - adds bloom back to scene
class FClassicBloomCompositePS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FClassicBloomCompositePS);
	SHADER_USE_PARAMETER_STRUCT(FClassicBloomCompositePS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneColorTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SceneColorSampler)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, BloomTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, BloomSampler)
		SHADER_PARAMETER(FVector4f, OutputViewportSizeAndInvSize)
		SHADER_PARAMETER(FScreenTransform, SvPositionToSceneColorUV) // Transform SvPosition to scene color texture UV
		SHADER_PARAMETER(FScreenTransform, SvPositionToBloomUV) // Transform SvPosition to bloom texture UV
		SHADER_PARAMETER(float, BloomIntensity)
		SHADER_PARAMETER(FVector4f, BloomTint)
		SHADER_PARAMETER(float, BloomBlendMode) // 0=Screen, 1=Overlay, 2=SoftLight, 3=HardLight, 4=Lighten, 5=Multiply
		SHADER_PARAMETER(float, BloomSaturation) // Saturation multiplier for bloom colors
		SHADER_PARAMETER(float, bProtectHighlights) // 1.0 = enabled, 0.0 = disabled
		SHADER_PARAMETER(float, HighlightProtection) // Strength of highlight protection (0.0-1.0)
		SHADER_PARAMETER(float, SoftFocusIntensity)
		SHADER_PARAMETER(FVector4f, SoftFocusParams) // x=OverlayMult, y=BlendStrength, z=SoftLightMult, w=FinalBlend
		SHADER_PARAMETER(float, bUseAdaptiveScaling) // Encoded as 1.0 or 0.0
		SHADER_PARAMETER(float, bShowBloomOnly) // Debug: show only bloom buffer
		SHADER_PARAMETER(float, bShowGammaCompensation) // Debug: visualize gamma compensation
		SHADER_PARAMETER(float, bIsGameWorld) // 1.0 if game/PIE world, 0.0 if editor
		SHADER_PARAMETER(float, GameModeBloomScale) // Manual compensation for game mode
		SHADER_PARAMETER(float, bUseBrightnessCompensation) // 1.0 = enabled, 0.0 = disabled
		SHADER_PARAMETER(float, BrightnessCompensationMode) // 0=EnergyConservation, 1=AutoIntensity, 2=ExposureAware
		SHADER_PARAMETER(float, BrightnessCompensationStrength) // 0.0-1.0
		SHADER_PARAMETER(float, ExposureCompensation) // Direct exposure offset (e.g., -1.0 to darken)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

// Directional glare streak shader
class FClassicBloomGlareStreakPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FClassicBloomGlareStreakPS);
	SHADER_USE_PARAMETER_STRUCT(FClassicBloomGlareStreakPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SourceTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SourceSampler)
		SHADER_PARAMETER(FVector4f, BufferSizeAndInvSize)
		SHADER_PARAMETER(FVector2f, StreakDirection) // Normalized direction vector
		SHADER_PARAMETER(float, StreakLength) // Length in texels
		SHADER_PARAMETER(float, StreakFalloff) // Exponential falloff rate
		SHADER_PARAMETER(int32, StreakSamples) // Samples per direction (8/16/32/48/64)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

// Glare streak accumulation shader - combines multiple streak directions
class FClassicBloomGlareAccumulatePS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FClassicBloomGlareAccumulatePS);
	SHADER_USE_PARAMETER_STRUCT(FClassicBloomGlareAccumulatePS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, StreakTexture0)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, StreakTexture1)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, StreakTexture2)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, StreakTexture3)
		SHADER_PARAMETER_SAMPLER(SamplerState, StreakSampler)
		SHADER_PARAMETER(FVector4f, GlareViewportSizeAndInvSize)
		SHADER_PARAMETER(int32, NumStreaks)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

// ============================================================================
// Kawase Bloom Shaders (Progressive Pyramid)
// Based on Masaki Kawase's GDC 2003 presentation
// ============================================================================

// Kawase downsample shader - 13-tap filter with Karis average for firefly reduction
class FClassicBloomKawaseDownsamplePS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FClassicBloomKawaseDownsamplePS);
	SHADER_USE_PARAMETER_STRUCT(FClassicBloomKawaseDownsamplePS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SourceTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SourceSampler)
		SHADER_PARAMETER(FVector4f, SourceSizeAndInvSize) // Source texture size for sampling offsets
		SHADER_PARAMETER(FVector4f, OutputSizeAndInvSize) // Output viewport size for UV calculation
		SHADER_PARAMETER(FScreenTransform, SvPositionToSourceUV) // Transform SvPosition to source texture UV
		SHADER_PARAMETER(float, BloomThreshold)
		SHADER_PARAMETER(float, ThresholdKnee)
		SHADER_PARAMETER(int32, MipLevel) // 0 = first downsample (apply threshold), >0 = subsequent
		SHADER_PARAMETER(int32, bUseKarisAverage) // 1 = apply Karis average (first mip only)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

// Kawase upsample shader - 9-tap tent filter with additive blend
class FClassicBloomKawaseUpsamplePS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FClassicBloomKawaseUpsamplePS);
	SHADER_USE_PARAMETER_STRUCT(FClassicBloomKawaseUpsamplePS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SourceTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, PreviousMipTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SourceSampler)
		SHADER_PARAMETER(FVector4f, OutputSizeAndInvSize) // Output viewport size for UV calculation
		SHADER_PARAMETER(float, FilterRadius) // Radius in texture coordinates
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};
