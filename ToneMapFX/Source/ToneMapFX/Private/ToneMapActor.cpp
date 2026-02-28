// Licensed under the zlib License. See LICENSE file in the project root.

#include "ToneMapActor.h"

AToneMapActor::AToneMapActor()
{
	ToneMapComponent = CreateDefaultSubobject<UToneMapComponent>(TEXT("ToneMapFX"));
	RootComponent = ToneMapComponent;
}

#if WITH_EDITOR
void AToneMapActor::SavePresetAs()
{
	if (ToneMapComponent)
	{
		ToneMapComponent->SavePresetAs();
	}
}

void AToneMapActor::LoadPresetBrowse()
{
	if (ToneMapComponent)
	{
		ToneMapComponent->LoadPresetBrowse();
	}
}
#endif
