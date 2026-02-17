// Licensed under the zlib License. See LICENSE file in the project root.

#include "ToneMapShaders.h"
#include "ShaderParameterUtils.h"

IMPLEMENT_GLOBAL_SHADER(FToneMapBlurPS,       "/Plugin/ToneMapFX/Private/ToneMapBlur.usf",       "GaussianBlurPS",       SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FToneMapProcessPS,    "/Plugin/ToneMapFX/Private/ToneMapProcess.usf",    "ToneMapProcessPS",    SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FToneMapLumMeasurePS, "/Plugin/ToneMapFX/Private/ToneMapLuminance.usf", "LuminanceMeasurePS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FToneMapLumAdaptPS,   "/Plugin/ToneMapFX/Private/ToneMapLuminance.usf", "LuminanceAdaptPS",   SF_Pixel);
