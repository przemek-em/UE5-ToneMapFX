// Licensed under the zlib License. See LICENSE file in the project root.

#include "ToneMapSharpenShaders.h"

IMPLEMENT_GLOBAL_SHADER(FToneMapSharpenPS, "/Plugin/ToneMapFX/Private/ToneMapSharpen.usf", "SharpenPS", SF_Pixel);
