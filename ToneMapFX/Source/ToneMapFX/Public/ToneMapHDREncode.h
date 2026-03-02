// Licensed under the zlib License. See LICENSE file in the project root.

#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "ScreenPass.h"

// =============================================================================
// HDR Output Encoding — Final Pass
//
// Converts sRGB-encoded output from ToneMapProcess (and optional LUT / Vignette)
// to the display's native HDR format: ST2084/PQ for HDR10, scRGB for Windows HDR.
//
// This pass runs only in ReplaceTonemap mode when HDR Output is enabled AND the
// engine detects an HDR display (OutputDevice >= 3).
// =============================================================================
class FToneMapHDREncodePS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FToneMapHDREncodePS);
	SHADER_USE_PARAMETER_STRUCT(FToneMapHDREncodePS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneColorTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SceneColorSampler)
		SHADER_PARAMETER(FScreenTransform, SvPositionToSceneColorUV)

		// HDR encoding parameters
		SHADER_PARAMETER(float, OutputDeviceType)  // EDisplayOutputFormat cast to float
		SHADER_PARAMETER(float, PaperWhiteNits)    // User paper-white brightness (cd/m²)
		SHADER_PARAMETER(float, MaxDisplayNits)    // Peak display luminance (cd/m²)

		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};
