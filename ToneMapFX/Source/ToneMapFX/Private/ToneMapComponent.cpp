// Licensed under the zlib License. See LICENSE file in the project root.

#include "ToneMapComponent.h"
#include "ToneMapSubsystem.h"
#include "Curves/CurveFloat.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PrimitiveSceneProxy.h"
#include "PrimitiveViewRelevance.h"
#include "SceneManagement.h"
#include "SceneView.h"
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
	bUseEditorCompositing = true;
}

void UToneMapComponent::PostLoad()
{
	Super::PostLoad();

	if (!bUseSceneColor && BloomColorMode == EBloomColorMode::SceneColor)
	{
		BloomColorMode = EBloomColorMode::Tint;
	}

	RebuildCustomToneCurveTexture();
}

namespace
{
FVector GetNonNegativeBlendExtent(const FVector& InExtent)
{
	return FVector(
		FMath::Max(InExtent.X, 0.0f),
		FMath::Max(InExtent.Y, 0.0f),
		FMath::Max(InExtent.Z, 0.0f));
}

class FToneMapBlendBoundsSceneProxy final : public FPrimitiveSceneProxy
{
public:
	FToneMapBlendBoundsSceneProxy(const UToneMapComponent* InComponent)
		: FPrimitiveSceneProxy(InComponent)
		, InnerExtent(GetNonNegativeBlendExtent(InComponent->BlendBoxExtent))
		, FalloffDistance(FMath::Max(InComponent->BlendFalloffDistance, 0.0f))
		, LineThickness(FMath::Max(InComponent->BlendBoundsLineThickness, 0.0f))
	{
		bWillEverBeLit = false;
	}

	SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}

	void GetDynamicMeshElements(
		const TArray<const FSceneView*>& Views,
		const FSceneViewFamily& ViewFamily,
		uint32 VisibilityMap,
		FMeshElementCollector& Collector) const override
	{
		const FMatrix& LocalToWorldMatrix = GetLocalToWorld();
		const FVector Origin = LocalToWorldMatrix.GetOrigin();
		const FVector XAxis = LocalToWorldMatrix.GetScaledAxis(EAxis::X);
		const FVector YAxis = LocalToWorldMatrix.GetScaledAxis(EAxis::Y);
		const FVector ZAxis = LocalToWorldMatrix.GetScaledAxis(EAxis::Z);

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
		{
			if ((VisibilityMap & (1 << ViewIndex)) == 0)
			{
				continue;
			}

			FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
			const FSceneView* View = Views[ViewIndex];
			const FLinearColor BoxColor = GetViewSelectionColor(
				FLinearColor(0.0f, 0.85f, 1.0f),
				*View,
				IsSelected(),
				IsHovered(),
				false,
				IsIndividuallySelected());

			DrawOrientedWireBox(PDI, Origin, XAxis, YAxis, ZAxis, InnerExtent, BoxColor, SDPG_World, LineThickness);

			if (FalloffDistance > KINDA_SMALL_NUMBER)
			{
				const FVector FalloffExtent = InnerExtent + FVector(FalloffDistance);
				DrawOrientedWireBox(PDI, Origin, XAxis, YAxis, ZAxis, FalloffExtent, FLinearColor(0.0f, 0.45f, 1.0f, 0.45f), SDPG_World, LineThickness);
			}
		}
	}

	FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View);
		Result.bDynamicRelevance = true;
		Result.bShadowRelevance = false;
		Result.bEditorPrimitiveRelevance = UseEditorCompositing(View);
		return Result;
	}

	uint32 GetMemoryFootprint() const override
	{
		return sizeof(*this) + GetAllocatedSize();
	}

	uint32 GetAllocatedSize() const
	{
		return FPrimitiveSceneProxy::GetAllocatedSize();
	}

private:
	const FVector InnerExtent;
	const float FalloffDistance;
	const float LineThickness;
};
} // namespace

FPrimitiveSceneProxy* UToneMapComponent::CreateSceneProxy()
{
	const FVector Extent = GetNonNegativeBlendExtent(BlendBoxExtent);
	if (!bShowBlendBounds || BlendMode != EToneMapBlendMode::BoundingBox || Extent.SizeSquared() <= KINDA_SMALL_NUMBER)
	{
		return nullptr;
	}

	return new FToneMapBlendBoundsSceneProxy(this);
}

FBoxSphereBounds UToneMapComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	if (!bShowBlendBounds || BlendMode != EToneMapBlendMode::BoundingBox)
	{
		return Super::CalcBounds(LocalToWorld);
	}

	const FVector Extent = GetNonNegativeBlendExtent(BlendBoxExtent);
	const float FalloffDistance = FMath::Max(BlendFalloffDistance, 0.0f);
	const FVector BoundsExtent = Extent + FVector(FalloffDistance);
	return FBoxSphereBounds(FBox(-BoundsExtent, BoundsExtent)).TransformBy(LocalToWorld);
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void UToneMapComponent::OnRegister()
{
	Super::OnRegister();
#if WITH_EDITOR
	BindCustomToneCurveUpdate();
#endif
	RebuildCustomToneCurveTexture();
	RegisterWithSubsystem();
}

void UToneMapComponent::OnUnregister()
{
#if WITH_EDITOR
	UnbindCustomToneCurveUpdate();
#endif
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

float UToneMapComponent::GetBlendWeightAtLocation(const FVector& ViewLocation) const
{
	const float ClampedWeight = FMath::Clamp(BlendWeight, 0.0f, 1.0f);
	if (!bEnabled || ClampedWeight <= 0.0f)
	{
		return 0.0f;
	}

	if (BlendMode == EToneMapBlendMode::Global)
	{
		return ClampedWeight;
	}

	const FVector LocalPosition = GetComponentTransform().InverseTransformPosition(ViewLocation);
	const FVector AbsLocal(FMath::Abs(LocalPosition.X), FMath::Abs(LocalPosition.Y), FMath::Abs(LocalPosition.Z));
	const FVector Extent(
		FMath::Max(BlendBoxExtent.X, 0.0f),
		FMath::Max(BlendBoxExtent.Y, 0.0f),
		FMath::Max(BlendBoxExtent.Z, 0.0f));
	const FVector Outside(
		FMath::Max(AbsLocal.X - Extent.X, 0.0f),
		FMath::Max(AbsLocal.Y - Extent.Y, 0.0f),
		FMath::Max(AbsLocal.Z - Extent.Z, 0.0f));

	const float OutsideDistance = Outside.Size();
	if (OutsideDistance <= KINDA_SMALL_NUMBER)
	{
		return ClampedWeight;
	}

	const float FalloffDistance = FMath::Max(BlendFalloffDistance, 0.0f);
	if (FalloffDistance <= KINDA_SMALL_NUMBER || OutsideDistance >= FalloffDistance)
	{
		return 0.0f;
	}

	const float LinearAlpha = 1.0f - (OutsideDistance / FalloffDistance);
	const float SmoothAlpha = LinearAlpha * LinearAlpha * (3.0f - 2.0f * LinearAlpha);
	return ClampedWeight * SmoothAlpha;
}

void UToneMapComponent::RebuildCustomToneCurveTexture()
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		CustomToneCurveTexture = nullptr;
		return;
	}

	const int32 SampleCount = FMath::Clamp(CustomCurveSampleCount, 64, 4096);
	const float MinEV = FMath::Clamp(CustomCurveMinEV, -24.0f, 23.99f);
	const float MaxEV = FMath::Clamp(FMath::Max(CustomCurveMaxEV, MinEV + 0.01f), MinEV + 0.01f, 24.0f);
	const float EVRange = FMath::Max(MaxEV - MinEV, 0.01f);

	CustomCurveSampleCount = SampleCount;
	CustomCurveMinEV = MinEV;
	CustomCurveMaxEV = MaxEV;

	TArray<uint8> TextureBytes;
	TextureBytes.SetNumUninitialized(SampleCount * sizeof(float));

	float* Samples = reinterpret_cast<float*>(TextureBytes.GetData());
	for (int32 Index = 0; Index < SampleCount; ++Index)
	{
		const float U = (SampleCount > 1) ? ((float)Index / (float)(SampleCount - 1)) : 0.0f;
		const float EV = MinEV + EVRange * U;
		const float SceneLuminance = FMath::Max(FMath::Pow(2.0f, EV), 0.0f);
		float MappedLuminance = CustomToneCurve ? CustomToneCurve->GetFloatValue(U) : (SceneLuminance / (1.0f + SceneLuminance));

		if (!FMath::IsFinite(MappedLuminance))
		{
			MappedLuminance = 0.0f;
		}

		Samples[Index] = FMath::Max(MappedLuminance, 0.0f);
	}

	CustomToneCurveTexture = UTexture2D::CreateTransient(
		SampleCount,
		1,
		PF_R32_FLOAT,
		NAME_None,
		TConstArrayView64<uint8>(TextureBytes.GetData(), TextureBytes.Num()));

	if (CustomToneCurveTexture)
	{
		CustomToneCurveTexture->NeverStream = true;
		CustomToneCurveTexture->SRGB = false;
		CustomToneCurveTexture->Filter = TF_Bilinear;
		CustomToneCurveTexture->AddressX = TA_Clamp;
		CustomToneCurveTexture->AddressY = TA_Clamp;
		CustomToneCurveTexture->UpdateResource();
	}

	if (IsRegistered())
	{
		MarkRenderStateDirty();
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
	TEXT("CustomToneCurveTexture"),
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

		if (GPresetSkipProperties.Contains(Key))
		{
			Skipped++;
			continue;
		}

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

	RebuildCustomToneCurveTexture();

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

#if WITH_EDITOR
void UToneMapComponent::BindCustomToneCurveUpdate()
{
	UCurveFloat* CurrentCurve = CustomToneCurve.Get();
	if (BoundCustomToneCurve.Get() == CurrentCurve && CustomToneCurveUpdateHandle.IsValid())
	{
		return;
	}

	UnbindCustomToneCurveUpdate();

	if (CurrentCurve)
	{
		CustomToneCurveUpdateHandle = CurrentCurve->OnUpdateCurve.AddUObject(this, &UToneMapComponent::HandleCustomToneCurveUpdated);
		BoundCustomToneCurve = CurrentCurve;
	}
}

void UToneMapComponent::UnbindCustomToneCurveUpdate()
{
	if (UCurveFloat* PreviousCurve = BoundCustomToneCurve.Get())
	{
		if (CustomToneCurveUpdateHandle.IsValid())
		{
			PreviousCurve->OnUpdateCurve.Remove(CustomToneCurveUpdateHandle);
		}
	}

	CustomToneCurveUpdateHandle.Reset();
	BoundCustomToneCurve.Reset();
}

void UToneMapComponent::HandleCustomToneCurveUpdated(UCurveBase* Curve, EPropertyChangeType::Type ChangeType)
{
	if (Curve == CustomToneCurve.Get())
	{
		RebuildCustomToneCurveTexture();
	}
}
#endif

// ---------------------------------------------------------------------------
// Editor helpers
// ---------------------------------------------------------------------------

#if WITH_EDITOR
void UToneMapComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	const bool bAnyPropertyChanged = PropertyName.IsNone();

	// When switching to a soft focus mode, auto-select the matching blend mode.
	if (bAnyPropertyChanged || PropertyName == GET_MEMBER_NAME_CHECKED(UToneMapComponent, BloomMode))
	{
		if (BloomMode == EBloomMode::SoftFocus)
		{
			BloomBlendMode = EBloomBlendMode::SoftLight;
		}
		else if (BloomMode == EBloomMode::SoftFocusGlow)
		{
			BloomBlendMode = EBloomBlendMode::SoftLightGlow;
		}
	}

	if (bAnyPropertyChanged ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UToneMapComponent, CustomToneCurve) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UToneMapComponent, CustomCurveMinEV) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UToneMapComponent, CustomCurveMaxEV) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UToneMapComponent, CustomCurveSampleCount) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UToneMapComponent, FilmCurve))
	{
		BindCustomToneCurveUpdate();
		RebuildCustomToneCurveTexture();
	}

	if (bAnyPropertyChanged ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UToneMapComponent, BlendMode) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UToneMapComponent, BlendBoxExtent) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UToneMapComponent, BlendFalloffDistance) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UToneMapComponent, bShowBlendBounds) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UToneMapComponent, BlendBoundsLineThickness))
	{
		UpdateBounds();
		MarkRenderStateDirty();
	}
}
#endif
