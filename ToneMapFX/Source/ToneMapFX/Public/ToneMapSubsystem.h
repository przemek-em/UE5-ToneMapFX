// Licensed under the zlib License. See LICENSE file in the project root.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SceneViewExtension.h"
#include "RendererInterface.h"
#include "ToneMapSubsystem.generated.h"

class UToneMapComponent;

// =============================================================================
// Scene View Extension — hooks into the post-process pipeline
// =============================================================================

class FToneMapSceneViewExtension : public FSceneViewExtensionBase
{
public:
	FToneMapSceneViewExtension(const FAutoRegister& AutoRegister, UToneMapSubsystem* InSubsystem);
	virtual ~FToneMapSceneViewExtension();

	// FSceneViewExtensionBase interface
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override;
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override {}

	virtual void SubscribeToPostProcessingPass(
		EPostProcessingPass PassId,
		const FSceneView& View,
		FAfterPassCallbackDelegateArray& InOutPassCallbacks,
		bool bIsPassEnabled) override;

	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override;
	virtual int32 GetPriority() const override { return 50; }

private:
	TWeakObjectPtr<UToneMapSubsystem> WeakSubsystem;

	// Cached mode from game thread (read in SetupView)
	bool bCachedReplaceTonemap = false;

	// Persistent adapted luminance for Krawczyk auto-exposure (survives across frames)
	TRefCountPtr<IPooledRenderTarget> AdaptedLuminanceRT;

	// Delta time cached from game thread for render thread use
	float LastDeltaTime = 0.016f;

	FScreenPassTexture PostProcessPass_RenderThread(
		FRDGBuilder& GraphBuilder,
		const FSceneView& View,
		const FPostProcessMaterialInputs& Inputs);
};

// =============================================================================
// World Subsystem — owns the view extension and tracks components
// =============================================================================

UCLASS()
class TONEMAPFX_API UToneMapSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void RegisterComponent(UToneMapComponent* Component);
	void UnregisterComponent(UToneMapComponent* Component);

	const TArray<TWeakObjectPtr<UToneMapComponent>>& GetComponents() const { return Components; }

private:
	TSharedPtr<FToneMapSceneViewExtension, ESPMode::ThreadSafe> SceneViewExtension;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<UToneMapComponent>> Components;
};
