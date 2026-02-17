// Licensed under the zlib License. See LICENSE file in the project root.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FToneMapFXModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
