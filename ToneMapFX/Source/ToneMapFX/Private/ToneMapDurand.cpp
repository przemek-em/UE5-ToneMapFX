// Licensed under the zlib License. See LICENSE file in the project root.

#include "ToneMapDurand.h"

IMPLEMENT_GLOBAL_SHADER(FToneMapDurandLogLumPS,       "/Plugin/ToneMapFX/Private/ToneMapDurandLogLum.usf",     "DurandLogLumPS",      SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FToneMapDurandBilateralPS,    "/Plugin/ToneMapFX/Private/ToneMapDurandBilateral.usf",  "DurandBilateralPS",   SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FToneMapDurandReconstructPS,  "/Plugin/ToneMapFX/Private/ToneMapDurandReconstruct.usf", "DurandReconstructPS", SF_Pixel);
