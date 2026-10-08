// Licensed under the zlib License. See LICENSE file in the project root.

#pragma once

#include "ScreenPass.h"

struct FToneMapCustomLocalExposureSettings
{
	float Strength = 1.0f;
	float SpatialSigma = 16.0f;
	float RangeSigmaEV = 1.16f;
	float HighlightContrast = 0.85f;
	float ShadowContrast = 0.85f;
	float MaxLiftEV = 1.0f;
	float MaxDarkenEV = 1.0f;
	float MidtoneProtectionEV = 1.0f;
	float MiddleGreyBias = 0.0f;
	bool bProtectDeepShadows = true;
};

// Spatial Durand-style prototype. Returns pre-exposed HDR scene color so the
// caller can continue through its normal exposure, grading and film curve.
FScreenPassTexture AddToneMapCustomLocalExposurePass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	FScreenPassTexture SceneColor,
	uint8 AutoExposureMode,
	FRDGTextureRef AdaptedLumTexture,
	float MinAutoExposure,
	float MaxAutoExposure,
	const FToneMapCustomLocalExposureSettings& Settings);
