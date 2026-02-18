// Licensed under the zlib License. See LICENSE file in the project root.

#include "ToneMapFattal.h"

IMPLEMENT_GLOBAL_SHADER(FToneMapFattalLogLumPS,      "/Plugin/ToneMapFX/Private/ToneMapFattalLogLum.usf",     "FattalLogLumPS",     SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FToneMapFattalGradientPS,    "/Plugin/ToneMapFX/Private/ToneMapFattalGradient.usf",   "FattalGradientPS",    SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FToneMapFattalDivergencePS,  "/Plugin/ToneMapFX/Private/ToneMapFattalDivergence.usf", "FattalDivergencePS",  SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FToneMapFattalJacobiPS,      "/Plugin/ToneMapFX/Private/ToneMapFattalJacobi.usf",     "FattalJacobiPS",      SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FToneMapFattalReconstructPS, "/Plugin/ToneMapFX/Private/ToneMapFattalReconstruct.usf", "FattalReconstructPS", SF_Pixel);
