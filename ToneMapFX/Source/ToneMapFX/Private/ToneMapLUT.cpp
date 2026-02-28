// Licensed under the zlib License. See LICENSE file in the project root.

#include "ToneMapLUTShaders.h"

IMPLEMENT_GLOBAL_SHADER(FToneMapLUTPS, "/Plugin/ToneMapFX/Private/ToneMapLUT.usf", "LUTPS", SF_Pixel);
