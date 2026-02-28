// Licensed under the zlib License. See LICENSE file in the project root.

#include "ToneMapComponent.h"
#include "ToneMapSubsystem.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/UnrealType.h"
#include "UObject/TextProperty.h"
#include "UObject/EnumProperty.h"

#if WITH_EDITOR
#include "DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"
#endif

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

// ---------------------------------------------------------------------------
// Presets — reflection-driven save/load to plain .txt files
// ---------------------------------------------------------------------------

// Properties to skip when serialising (internal / non-user-facing)
static const TSet<FString> GPresetSkipProperties = {
	TEXT("bAutoActivate"),
	TEXT("PrimaryComponentTick"),
	TEXT("ComponentTags"),
	TEXT("AssetUserData"),
	TEXT("bReplicates"),
	TEXT("bNetAddressable"),
};

FString UToneMapComponent::GetPresetDirectory()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ToneMapFX"));
}

#if WITH_EDITOR
void UToneMapComponent::SavePresetAs()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform) return;

	const void* ParentWindow = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
	const FString DefaultDir = GetPresetDirectory();
	const FString DefaultName = TEXT("Preset");

	TArray<FString> OutFiles;
	bool bPicked = DesktopPlatform->SaveFileDialog(
		ParentWindow,
		TEXT("Save ToneMapFX Preset"),
		DefaultDir,
		DefaultName + TEXT(".txt"),
		TEXT("ToneMapFX Preset (*.txt)|*.txt|All Files (*.*)|*.*"),
		0,
		OutFiles);

	if (bPicked && OutFiles.Num() > 0)
	{
		FString ChosenPath = OutFiles[0];
		// Ensure .txt extension
		if (!ChosenPath.EndsWith(TEXT(".txt")))
		{
			ChosenPath += TEXT(".txt");
		}
		SavePresetToPath(ChosenPath);
	}
}

void UToneMapComponent::LoadPresetBrowse()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform) return;

	const void* ParentWindow = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
	const FString DefaultDir = GetPresetDirectory();

	TArray<FString> OutFiles;
	bool bPicked = DesktopPlatform->OpenFileDialog(
		ParentWindow,
		TEXT("Load ToneMapFX Preset"),
		DefaultDir,
		TEXT(""),
		TEXT("ToneMapFX Preset (*.txt)|*.txt|All Files (*.*)|*.*"),
		0,
		OutFiles);

	if (bPicked && OutFiles.Num() > 0)
	{
		LoadPresetFromPath(OutFiles[0]);
	}
}
#endif

bool UToneMapComponent::SavePresetToPath(const FString& FilePath) const
{
	if (FilePath.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("ToneMapFX: SavePresetToPath called with empty path"));
		return false;
	}

	TArray<FString> Lines;
	Lines.Add(FString::Printf(TEXT("# ToneMapFX Preset: %s"), *FPaths::GetBaseFilename(FilePath)));
	Lines.Add(FString::Printf(TEXT("# Saved: %s"), *FDateTime::Now().ToString()));
	Lines.Add(TEXT("# --------------------------------------------------------"));

	// Iterate every UPROPERTY on this class via UE reflection
	for (TFieldIterator<FProperty> It(GetClass()); It; ++It)
	{
		FProperty* Prop = *It;
		if (!Prop) continue;

		const FString PropName = Prop->GetName();

		// Skip internal / engine properties
		if (GPresetSkipProperties.Contains(PropName)) continue;

		// Only serialise properties declared on UToneMapComponent (not inherited USceneComponent ones)
		if (Prop->GetOwnerClass() != UToneMapComponent::StaticClass()) continue;

		// Export value to string using UE's built-in text export
		FString ValueStr;
		const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(this);
		Prop->ExportTextItem_Direct(ValueStr, ValuePtr, nullptr, nullptr, PPF_None);

		Lines.Add(FString::Printf(TEXT("%s=%s"), *PropName, *ValueStr));
	}

	if (!FFileHelper::SaveStringArrayToFile(Lines, *FilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("ToneMapFX: Failed to write preset to %s"), *FilePath);
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("ToneMapFX: Preset saved → %s (%d properties)"), *FilePath, Lines.Num() - 3);
	return true;
}

bool UToneMapComponent::LoadPresetFromPath(const FString& FilePath)
{
	if (FilePath.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("ToneMapFX: LoadPresetFromPath called with empty path"));
		return false;
	}

	TArray<FString> Lines;
	if (!FFileHelper::LoadFileToStringArray(Lines, *FilePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("ToneMapFX: Preset file not found: %s"), *FilePath);
		return false;
	}

	// Build a lookup map: property name → FProperty*
	TMap<FString, FProperty*> PropMap;
	for (TFieldIterator<FProperty> It(GetClass()); It; ++It)
	{
		FProperty* Prop = *It;
		if (Prop && Prop->GetOwnerClass() == UToneMapComponent::StaticClass())
		{
			PropMap.Add(Prop->GetName(), Prop);
		}
	}

	int32 Applied = 0;
	int32 Skipped = 0;

	for (const FString& Line : Lines)
	{
		// Skip comments and empty lines
		FString Trimmed = Line.TrimStartAndEnd();
		if (Trimmed.IsEmpty() || Trimmed.StartsWith(TEXT("#"))) continue;

		// Parse "PropertyName=Value"
		FString Key, Value;
		if (!Trimmed.Split(TEXT("="), &Key, &Value, ESearchCase::CaseSensitive, ESearchDir::FromStart))
		{
			Skipped++;
			continue;
		}

		Key.TrimStartAndEndInline();
		Value.TrimStartAndEndInline();

		FProperty** FoundProp = PropMap.Find(Key);
		if (!FoundProp || !(*FoundProp))
		{
			// Property may have been removed in a newer/older version — skip gracefully
			UE_LOG(LogTemp, Verbose, TEXT("ToneMapFX: Preset key '%s' not found on component, skipping"), *Key);
			Skipped++;
			continue;
		}

		FProperty* Prop = *FoundProp;
		void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(this);

		// Import value from string using UE's built-in text import
		const TCHAR* Buffer = *Value;
		if (Prop->ImportText_Direct(Buffer, ValuePtr, this, PPF_None))
		{
			Applied++;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ToneMapFX: Failed to import '%s' = '%s'"), *Key, *Value);
			Skipped++;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("ToneMapFX: Preset loaded ← %s (%d applied, %d skipped)"), *FilePath, Applied, Skipped);

#if WITH_EDITOR
	// Notify the editor that all properties changed so the Details panel
	// refreshes EditCondition states (e.g. bEnableLUT → LUTTexture enabled).
	FPropertyChangedEvent ChangedEvent(nullptr, EPropertyChangeType::ValueSet);
	PostEditChangeProperty(ChangedEvent);
#endif

	return Applied > 0;
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

	// When switching to Soft Focus mode, auto-select Soft Light blend mode
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UToneMapComponent, BloomMode))
	{
		if (BloomMode == EBloomMode::SoftFocus)
		{
			BloomBlendMode = EBloomBlendMode::SoftLight;
		}
	}
}
#endif
