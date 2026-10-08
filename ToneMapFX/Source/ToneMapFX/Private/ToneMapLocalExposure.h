// Licensed under the zlib License. See LICENSE file in the project root.

#pragma once

#include "ScreenPass.h"

// Resolve the plugin's actual auto exposure for engine and custom local passes.
FRDGBufferRef AddToneMapLocalExposureBuffer(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	FScreenPassTexture SceneColor,
	uint8 AutoExposureMode,
	FRDGTextureRef AdaptedLumTexture,
	float MinAutoExposure,
	float MaxAutoExposure);

// Apply the standard PostProcessVolume local-exposure settings to HDR scene color.
// AutoExposureMode and AdaptedLumTexture match the plugin's tonemapping pass.
FScreenPassTexture AddToneMapLocalExposurePass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	FScreenPassTexture SceneColor,
	uint8 AutoExposureMode,
	FRDGTextureRef AdaptedLumTexture,
	float MinAutoExposure,
	float MaxAutoExposure);
