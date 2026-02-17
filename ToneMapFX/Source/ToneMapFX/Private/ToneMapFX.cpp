// Licensed under the zlib License. See LICENSE file in the project root.

#include "ToneMapFX.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

#define LOCTEXT_NAMESPACE "FToneMapFXModule"

void FToneMapFXModule::StartupModule()
{
	FString PluginShaderDir = FPaths::Combine(
		IPluginManager::Get().FindPlugin(TEXT("ToneMapFX"))->GetBaseDir(),
		TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/Plugin/ToneMapFX"), PluginShaderDir);
}

void FToneMapFXModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FToneMapFXModule, ToneMapFX)
