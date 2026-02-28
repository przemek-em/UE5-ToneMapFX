// Licensed under the zlib License. See LICENSE file in the project root.

#include "ToneMapVignetteShaders.h"

IMPLEMENT_GLOBAL_SHADER(FToneMapVignettePS, "/Plugin/ToneMapFX/Private/ToneMapVignette.usf", "VignettePS", SF_Pixel);
