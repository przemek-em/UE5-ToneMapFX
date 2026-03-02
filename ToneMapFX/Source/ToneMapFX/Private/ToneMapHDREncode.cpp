// Licensed under the zlib License. See LICENSE file in the project root.

#include "ToneMapHDREncode.h"

IMPLEMENT_GLOBAL_SHADER(FToneMapHDREncodePS, "/Plugin/ToneMapFX/Private/ToneMapHDREncode.usf", "HDREncodePS", SF_Pixel);
