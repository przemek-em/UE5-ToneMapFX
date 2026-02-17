// Licensed under the zlib License. See LICENSE file in the project root.

#include "ClassicBloomShaders.h"
#include "ShaderParameterUtils.h"

// Implement the pixel shaders (now residing under ToneMapFX shader directory)
IMPLEMENT_GLOBAL_SHADER(FClassicBloomBrightPassPS, "/Plugin/ToneMapFX/Private/ClassicBloomShaders.usf", "BrightPassPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FClassicBloomBlurPS, "/Plugin/ToneMapFX/Private/ClassicBloomBlur.usf", "GaussianBlurPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FClassicBloomCompositePS, "/Plugin/ToneMapFX/Private/ClassicBloomComposite.usf", "CompositeBloomPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FClassicBloomGlareStreakPS, "/Plugin/ToneMapFX/Private/ClassicBloomGlare.usf", "GlareStreakPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FClassicBloomGlareAccumulatePS, "/Plugin/ToneMapFX/Private/ClassicBloomGlare.usf", "GlareAccumulatePS", SF_Pixel);

// Kawase bloom shaders
IMPLEMENT_GLOBAL_SHADER(FClassicBloomKawaseDownsamplePS, "/Plugin/ToneMapFX/Private/ClassicBloomKawase.usf", "KawaseDownsamplePS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FClassicBloomKawaseUpsamplePS, "/Plugin/ToneMapFX/Private/ClassicBloomKawase.usf", "KawaseUpsamplePS", SF_Pixel);
