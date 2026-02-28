// Licensed under the zlib License. See LICENSE file in the project root.

#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "ScreenPass.h"

// =============================================================================
// LUT — Color Grading Look-Up Table
//   Applies a standard UE LUT texture (256×16 / 1024×32 / 4096×64 unwrapped,
//   or any Size²×Size strip) as a post-tonemap color grading step.
//   Input: sRGB display-referred scene color
//   Output: LUT-graded sRGB color
// =============================================================================
class FToneMapLUTPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FToneMapLUTPS);
	SHADER_USE_PARAMETER_STRUCT(FToneMapLUTPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneColorTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SceneColorSampler)
		SHADER_PARAMETER(FScreenTransform, SvPositionToSceneColorUV)

		// LUT texture and sampling parameters
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, LUTTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, LUTSampler)
		SHADER_PARAMETER(float, LUTSize)      // Cube dimension (16, 32, or 64)
		SHADER_PARAMETER(float, InvLUTSize)    // 1.0 / LUTSize
		SHADER_PARAMETER(float, LUTIntensity)  // 0 = bypass, 1 = full LUT

		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};
