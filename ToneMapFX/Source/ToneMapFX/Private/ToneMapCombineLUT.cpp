// Licensed under the zlib License. See LICENSE file in the project root.

#include "ToneMapCombineLUTShaders.h"

IMPLEMENT_GLOBAL_SHADER(FToneMapCombineLUTPS, "/Plugin/ToneMapFX/Private/ToneMapCombineLUT.usf", "CombineLUTPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FToneMapApplyLUTPS,   "/Plugin/ToneMapFX/Private/ToneMapApplyLUT.usf",   "ApplyLUTPS",   SF_Pixel);
