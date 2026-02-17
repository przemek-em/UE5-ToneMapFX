// Licensed under the zlib License. See LICENSE file in the project root.

#include "ToneMapComponent.h"
#include "ToneMapSubsystem.h"
#include "Engine/World.h"

UToneMapComponent::UToneMapComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bAutoActivate = true;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void UToneMapComponent::OnRegister()
{
	Super::OnRegister();
	RegisterWithSubsystem();
}

void UToneMapComponent::OnUnregister()
{
	UnregisterFromSubsystem();
	Super::OnUnregister();
}

void UToneMapComponent::BeginPlay()
{
	Super::BeginPlay();
	RegisterWithSubsystem();
}

void UToneMapComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterFromSubsystem();
	Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------------------
// Subsystem registration
// ---------------------------------------------------------------------------

void UToneMapComponent::RegisterWithSubsystem()
{
	if (UWorld* World = GetWorld())
	{
		if (UToneMapSubsystem* Subsystem = World->GetSubsystem<UToneMapSubsystem>())
		{
			Subsystem->RegisterComponent(this);
		}
	}
}

void UToneMapComponent::UnregisterFromSubsystem()
{
	if (UWorld* World = GetWorld())
	{
		if (UToneMapSubsystem* Subsystem = World->GetSubsystem<UToneMapSubsystem>())
		{
			Subsystem->UnregisterComponent(this);
		}
	}
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

bool UToneMapComponent::IsAnyHSLActive() const
{
	const float Eps = 0.01f;
	return FMath::Abs(HueReds) > Eps || FMath::Abs(HueOranges) > Eps ||
		FMath::Abs(HueYellows) > Eps || FMath::Abs(HueGreens) > Eps ||
		FMath::Abs(HueAquas) > Eps || FMath::Abs(HueBlues) > Eps ||
		FMath::Abs(HuePurples) > Eps || FMath::Abs(HueMagentas) > Eps ||
		FMath::Abs(SatReds) > Eps || FMath::Abs(SatOranges) > Eps ||
		FMath::Abs(SatYellows) > Eps || FMath::Abs(SatGreens) > Eps ||
		FMath::Abs(SatAquas) > Eps || FMath::Abs(SatBlues) > Eps ||
		FMath::Abs(SatPurples) > Eps || FMath::Abs(SatMagentas) > Eps ||
		FMath::Abs(LumReds) > Eps || FMath::Abs(LumOranges) > Eps ||
		FMath::Abs(LumYellows) > Eps || FMath::Abs(LumGreens) > Eps ||
		FMath::Abs(LumAquas) > Eps || FMath::Abs(LumBlues) > Eps ||
		FMath::Abs(LumPurples) > Eps || FMath::Abs(LumMagentas) > Eps;
}

bool UToneMapComponent::IsAnyCurveActive() const
{
	const float Eps = 0.01f;
	return FMath::Abs(CurveHighlights) > Eps || FMath::Abs(CurveLights) > Eps ||
		FMath::Abs(CurveDarks) > Eps || FMath::Abs(CurveShadows) > Eps;
}

// ---------------------------------------------------------------------------
// Editor helpers
// ---------------------------------------------------------------------------

#if WITH_EDITOR
void UToneMapComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// When switching to Soft Focus mode, auto-select Overlay blend mode
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UToneMapComponent, BloomMode))
	{
		if (BloomMode == EBloomMode::SoftFocus)
		{
			BloomBlendMode = EBloomBlendMode::Overlay;
		}
	}
}
#endif
