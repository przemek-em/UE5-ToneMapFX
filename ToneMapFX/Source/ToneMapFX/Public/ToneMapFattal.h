// Licensed under the zlib License. See LICENSE file in the project root.

#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "ScreenPass.h"

// =============================================================================
// Fattal et al. 2002 — Pass 0: Compute ln(lum) at work resolution
//   Used to SEED the Jacobi solver so that partial convergence yields a valid
//   compression ratio: I_final ≈ attenuated(logLum), exp(I - logLumIn) < 1 for edges.
// =============================================================================
class FToneMapFattalLogLumPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FToneMapFattalLogLumPS);
	SHADER_USE_PARAMETER_STRUCT(FToneMapFattalLogLumPS, FGlobalShader);

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
// Fattal et al. 2002 — Pass 1: Compute gradient field and attenuation function
//   Input:  HDR scene color
//   Output: RG32F (Hx, Hy) — attenuated gradient of log-luminance
// =============================================================================
class FToneMapFattalGradientPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FToneMapFattalGradientPS);
	SHADER_USE_PARAMETER_STRUCT(FToneMapFattalGradientPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneColorTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SceneColorSampler)
		SHADER_PARAMETER(FScreenTransform, SvPositionToSceneColorUV)
		SHADER_PARAMETER(FVector4f, BufferSizeAndInvSize)  // xy=size, zw=1/size
		SHADER_PARAMETER(float, OneOverPreExposure)
		SHADER_PARAMETER(float, Alpha)      // threshold parameter
		SHADER_PARAMETER(float, Beta)       // attenuation exponent
		SHADER_PARAMETER(float, NoiseFloor) // tiny ε to avoid divide-by-zero
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

// =============================================================================
// Fattal et al. 2002 — Pass 2: Compute divergence of attenuated gradient field
//   Input:  RG32F (Hx, Hy) attenuated gradients
//   Output: R32F divergence field  div(H)
// =============================================================================
class FToneMapFattalDivergencePS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FToneMapFattalDivergencePS);
	SHADER_USE_PARAMETER_STRUCT(FToneMapFattalDivergencePS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, GradientTexture)   // RG = (Hx, Hy)
		SHADER_PARAMETER_SAMPLER(SamplerState, GradientSampler)
		SHADER_PARAMETER(FVector4f, BufferSizeAndInvSize)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

// =============================================================================
// Fattal et al. 2002 — Pass 3: Jacobi iteration for Poisson solve
//   Solves: ∇²I = div(H)  via repeated Gauss-Seidel / Jacobi averaging.
//   Run this pass N times (FattalJacobiIterations), ping-ponging two R32F targets.
//   Input:  R32F current I estimate, R32F divergence (rhs)
//   Output: R32F updated I estimate
// =============================================================================
class FToneMapFattalJacobiPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FToneMapFattalJacobiPS);
	SHADER_USE_PARAMETER_STRUCT(FToneMapFattalJacobiPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, CurrentITexture) // current I estimate
		SHADER_PARAMETER_SAMPLER(SamplerState, CurrentISampler)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DivHTexture)     // rhs: divergence
		SHADER_PARAMETER_SAMPLER(SamplerState, DivHSampler)
		SHADER_PARAMETER(FVector4f, BufferSizeAndInvSize)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

// =============================================================================
// Fattal et al. 2002 — Pass 4: Reconstruct tone-mapped image
//   Input:  HDR scene color, solved I map (log-lum solution)
//   Output: RGBA16F tone-mapped image  (bPreToneMapped = 1)
// =============================================================================
class FToneMapFattalReconstructPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FToneMapFattalReconstructPS);
	SHADER_USE_PARAMETER_STRUCT(FToneMapFattalReconstructPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneColorTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SceneColorSampler)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SolvedITexture)  // Poisson-solved log-lum
		SHADER_PARAMETER_SAMPLER(SamplerState, SolvedISampler)
		SHADER_PARAMETER(FScreenTransform, SvPositionToSceneColorUV)
		SHADER_PARAMETER(FVector4f, BufferSizeAndInvSize)  // xy=size, zw=1/size for work-texture UV
		SHADER_PARAMETER(float, OneOverPreExposure)
		SHADER_PARAMETER(float, OutputSaturation) // scales chroma after reconstruction
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};
