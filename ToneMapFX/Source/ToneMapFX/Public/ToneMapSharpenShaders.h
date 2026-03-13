// Licensed under the zlib License. See LICENSE file in the project root.

#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "ScreenPass.h"

// =============================================================================
// Sharpening — Unsharp mask via 9-tap kernel
//   Amount: strength of sharpening (0-100)
//   Radius: pixel radius for neighbor sampling (0.5-5.0)
// =============================================================================
class FToneMapSharpenPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FToneMapSharpenPS);
	SHADER_USE_PARAMETER_STRUCT(FToneMapSharpenPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneColorTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SceneColorSampler)
		SHADER_PARAMETER(FScreenTransform, SvPositionToSceneColorUV)

		SHADER_PARAMETER(float, SharpenAmount)
		SHADER_PARAMETER(float, SharpenRadius)
		SHADER_PARAMETER(FVector2f, TexelSize)
		SHADER_PARAMETER(float, DitherQuantization) // 0=off, 1/255=8-bit, 1/1023=10-bit

		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};
