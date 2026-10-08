// Licensed under the zlib License. See LICENSE file in the project root.

#pragma once

#include "CoreMinimal.h"

namespace ToneMapFX::Uchimura
{
struct FParameters
{
	FVector4f Curve; // x=peak, y=contrast, z=linear start, w=linear length
	FVector2f Toe;   // x=black tightness, y=pedestal
};

FParameters BuildParameters(
	float MaxBrightness,
	float Contrast,
	float LinearStart,
	float LinearLength,
	float BlackTightness,
	float Pedestal,
	float OutputHeadroom);
}
