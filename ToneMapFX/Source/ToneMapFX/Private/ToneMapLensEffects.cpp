// Licensed under the zlib License. See LICENSE file in the project root.

#include "ToneMapLensEffects.h"

IMPLEMENT_GLOBAL_SHADER(FToneMapLensBrightPassPS, "/Plugin/ToneMapFX/Private/ToneMapLensBrightPass.usf", "LensBrightPassPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FToneMapCoronaStreakPS,   "/Plugin/ToneMapFX/Private/ToneMapLensCorona.usf",    "CoronaStreakPS",   SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FToneMapHaloRingPS,       "/Plugin/ToneMapFX/Private/ToneMapLensHalo.usf",      "HaloRingPS",       SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FToneMapLensCompositePS,  "/Plugin/ToneMapFX/Private/ToneMapLensComposite.usf", "LensCompositePS",  SF_Pixel);
