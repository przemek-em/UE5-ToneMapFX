// Licensed under the zlib License. See LICENSE file in the project root.

#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "ScreenPass.h"

// =============================================================================
// Vignette — Screen-space darken / lighten from edges
//   Modes: Circular (Euclidean distance) or Square (Chebyshev distance)
//   Intensity: positive = darken edges, negative = lighten edges
//   Supports optional alpha texture mask and texture-only bypass mode.
//   Output: RGBA with vignette applied
// =============================================================================
class FToneMapVignettePS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FToneMapVignettePS);
	SHADER_USE_PARAMETER_STRUCT(FToneMapVignettePS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneColorTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SceneColorSampler)
		SHADER_PARAMETER(FScreenTransform, SvPositionToSceneColorUV)

		// x = Mode (0=Circular, 1=Square), y = Size (0-100), z = Intensity (-100..100), w = FalloffMode (0-4)
		SHADER_PARAMETER(FVector4f, VignetteParams)
		SHADER_PARAMETER(float, FalloffExponent) // Custom power curve exponent

		// Alpha texture mask (optional)
		SHADER_PARAMETER(float, bUseAlphaTexture)
		SHADER_PARAMETER(float, bAlphaTextureOnly)
		SHADER_PARAMETER(float, TextureChannelIndex) // 0=A, 1=R, 2=G, 3=B
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AlphaTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, AlphaSampler)

		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};
