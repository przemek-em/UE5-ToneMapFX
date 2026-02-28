// Licensed under the zlib License. See LICENSE file in the project root.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ToneMapComponent.h"
#include "ToneMapActor.generated.h"

/**
 * Placeable actor that provides Tone Map FX post-processing.
 * Drag into the scene from Place Actors → search "Tone Map FX".
 * All settings are exposed on the ToneMapComponent sub-object in the Details panel.
 */
UCLASS(Blueprintable, meta=(DisplayName="Tone Map FX"))
class TONEMAPFX_API AToneMapActor : public AActor
{
	GENERATED_BODY()

public:
	AToneMapActor();

	/** The Tone Map FX component that holds all effect settings. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ToneMap")
	TObjectPtr<UToneMapComponent> ToneMapComponent;

	// =========================================================================
	// Presets (passthrough to component — CallInEditor buttons don't auto-
	// propagate from components, so we forward them here)
	// =========================================================================

#if WITH_EDITOR
	/** Open a Save File dialog to choose where to save the preset. */
	UFUNCTION(CallInEditor, Category = "Tone Map|Presets")
	void SavePresetAs();

	/** Open a file browser to load a preset from any location. */
	UFUNCTION(CallInEditor, Category = "Tone Map|Presets")
	void LoadPresetBrowse();
#endif
};
