// Licensed under the zlib License. See LICENSE file in the project root.

#include "ToneMapUchimura.h"

namespace ToneMapFX::Uchimura
{
FParameters BuildParameters(
	float MaxBrightness,
	float Contrast,
	float LinearStart,
	float LinearLength,
	float BlackTightness,
	float Pedestal,
	float OutputHeadroom)
{
	const float SafeHeadroom = FMath::Clamp(OutputHeadroom, 0.1f, 16.0f);
	const float Peak = FMath::Clamp(MaxBrightness, 0.1f, SafeHeadroom);
	const float SafeContrast = FMath::Clamp(Contrast, 0.05f, 4.0f);
	const float SafeLinearStart = FMath::Clamp(LinearStart, 0.001f, Peak - 0.001f);
	const float SafeLinearLength = FMath::Clamp(LinearLength, 0.0f, 0.99f);
	const float SafeBlackTightness = FMath::Clamp(BlackTightness, 0.1f, 4.0f);

	// A pedestal at or above the linear-section start can invert the toe.
	const float SafePedestal = FMath::Clamp(Pedestal, 0.0f, FMath::Min(0.5f, SafeLinearStart * 0.99f));

	return {
		FVector4f(Peak, SafeContrast, SafeLinearStart, SafeLinearLength),
		FVector2f(SafeBlackTightness, SafePedestal)
	};
}
}
