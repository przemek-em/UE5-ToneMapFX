// Licensed under the zlib License. See LICENSE file in the project root.

#include "ToneMapSubsystem.h"
#include "ToneMapComponent.h"
#include "ToneMapShaders.h"
#include "ClassicBloomShaders.h"
#include "ToneMapDurand.h"
#include "ToneMapFattal.h"
#include "ToneMapLensEffects.h"
#include "ToneMapVignetteShaders.h"
#include "ToneMapLUTShaders.h"
#include "SceneView.h"
#include "SceneRendering.h"
#include "ScreenPass.h"
#include "PostProcess/PostProcessMaterialInputs.h"
#include "PostProcess/PostProcessTonemap.h"
#include "ToneMapHDREncode.h"
#include "RenderGraphUtils.h"
#include "PixelShaderUtils.h"

// =============================================================================
// FToneMapSceneViewExtension
// =============================================================================

FToneMapSceneViewExtension::FToneMapSceneViewExtension(
	const FAutoRegister& AutoRegister,
	UToneMapSubsystem* InSubsystem)
	: FSceneViewExtensionBase(AutoRegister)
	, WeakSubsystem(InSubsystem)
{
}

FToneMapSceneViewExtension::~FToneMapSceneViewExtension()
{
	// TRefCountPtr<IPooledRenderTarget> destructor handles cleanup
}

void FToneMapSceneViewExtension::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
	UToneMapSubsystem* Subsystem = WeakSubsystem.Get();
	if (!Subsystem) return;

	const TArray<TWeakObjectPtr<UToneMapComponent>>& Comps = Subsystem->GetComponents();
	for (const TWeakObjectPtr<UToneMapComponent>& Ptr : Comps)
	{
		if (Ptr.IsValid() && Ptr->IsActive() && Ptr->bEnabled)
		{
			bCachedReplaceTonemap = (Ptr->Mode == EToneMapMode::ReplaceTonemap);
			bCachedHDROutput = Ptr->bHDROutput;

			// Auto-toggle r.HDR.EnableHDROutput to match the UI checkbox.
			// IConsoleManager is available through CoreMinimal.h — no extra includes.
			{
				static IConsoleVariable* CVarHDR = IConsoleManager::Get().FindConsoleVariable(TEXT("r.HDR.EnableHDROutput"));
				if (CVarHDR)
				{
					const int32 DesiredValue = (bCachedReplaceTonemap && bCachedHDROutput) ? 1 : 0;
					if (CVarHDR->GetInt() != DesiredValue)
					{
						CVarHDR->Set(DesiredValue, ECVF_SetByCode);
					}
				}
			}

			// Cache delta time for render thread (temporal adaptation)
			LastDeltaTime = FApp::GetDeltaTime();

			if (bCachedReplaceTonemap)
			{
				// Disable UE's ACES tone curve, gamut expansion, and blue correction
				// so the LUT is built as a near-identity (white balance + color grading only)
				InView.FinalPostProcessSettings.bOverride_ToneCurveAmount = 1;
				InView.FinalPostProcessSettings.ToneCurveAmount = 0.0f;
				InView.FinalPostProcessSettings.bOverride_ExpandGamut = 1;
				InView.FinalPostProcessSettings.ExpandGamut = 0.0f;
				InView.FinalPostProcessSettings.bOverride_BlueCorrection = 1;
				InView.FinalPostProcessSettings.BlueCorrection = 0.0f;
			}

			// Disable UE's built-in bloom by zeroing its intensity
			if (Ptr->bDisableUnrealBloom)
			{
				InView.FinalPostProcessSettings.bOverride_BloomIntensity = 1;
				InView.FinalPostProcessSettings.BloomIntensity = 0.0f;
			}

			// Disable UE's built-in auto-exposure for Krawczyk and None modes.
			// Engine Default intentionally keeps UE exposure active (user wants it).
			//
			// We neutralise every path that feeds into PreExposure:
			//   AutoExposureMethod       → AEM_Manual   (no histogram/basic GPU pass)
			//   AutoExposureBias         → 0            (pow(2, bias) scales PreExposure)
			//   PhysicalCameraExposure   → false        (no ISO/aperture influence)
			//   LocalExposure contrasts  → 1.0          (average feeds back into PreExposure)
			const bool bNeedNeutralExposure = bCachedReplaceTonemap &&
				Ptr->AutoExposureMode != EToneMapAutoExposure::EngineDefault;

			if (bNeedNeutralExposure)
			{
				InView.FinalPostProcessSettings.bOverride_AutoExposureMethod = 1;
				InView.FinalPostProcessSettings.AutoExposureMethod = AEM_Manual;

				InView.FinalPostProcessSettings.bOverride_AutoExposureBias = 1;
				InView.FinalPostProcessSettings.AutoExposureBias = 0.0f;

				InView.FinalPostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = 1;
				InView.FinalPostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;

				// Neutralise local exposure so its average doesn't feed back into PreExposure
				InView.FinalPostProcessSettings.bOverride_LocalExposureHighlightContrastScale = 1;
				InView.FinalPostProcessSettings.LocalExposureHighlightContrastScale = 1.0f;
				InView.FinalPostProcessSettings.bOverride_LocalExposureShadowContrastScale = 1;
				InView.FinalPostProcessSettings.LocalExposureShadowContrastScale = 1.0f;
			}

			break;
		}
	}
}

// ---------------------------------------------------------------------------
// Subscribe to the correct post-process pass
// ---------------------------------------------------------------------------

void FToneMapSceneViewExtension::SubscribeToPostProcessingPass(
	EPostProcessingPass PassId,
	const FSceneView& View,
	FAfterPassCallbackDelegateArray& InOutPassCallbacks,
	bool bIsPassEnabled)
{
	const FSceneViewFamily* Family = View.Family;
	if (!Family) return;

	// Skip non-renderable worlds
	if (Family->Scene && Family->Scene->GetWorld())
	{
		UWorld* World = Family->Scene->GetWorld();
		if (World->WorldType != EWorldType::Game &&
			World->WorldType != EWorldType::Editor &&
			World->WorldType != EWorldType::PIE)
			return;
	}
	if (!Family->EngineShowFlags.PostProcessing) return;
	if (!Family->EngineShowFlags.Rendering || Family->EngineShowFlags.Wireframe) return;

	UToneMapSubsystem* Subsystem = WeakSubsystem.Get();
	if (!Subsystem) return;

	// Determine desired pass from the first active ToneMap component
	EPostProcessingPass DesiredPass = EPostProcessingPass::Tonemap;
	bool bFoundToneMap = false;
	const TArray<TWeakObjectPtr<UToneMapComponent>>& Comps = Subsystem->GetComponents();
	for (const TWeakObjectPtr<UToneMapComponent>& Ptr : Comps)
	{
		if (Ptr.IsValid() && Ptr->IsActive() && Ptr->bEnabled)
		{
			bFoundToneMap = true;
			if (Ptr->Mode == EToneMapMode::ReplaceTonemap)
			{
				// Replace the entire tonemapper
				DesiredPass = EPostProcessingPass::ReplacingTonemapper;
			}
			else
			{
				switch (Ptr->PostProcessPass)
				{
				case EToneMapPostProcessPass::Tonemap:    DesiredPass = EPostProcessingPass::Tonemap;    break;
				case EToneMapPostProcessPass::MotionBlur: DesiredPass = EPostProcessingPass::MotionBlur; break;
				case EToneMapPostProcessPass::FXAA:                  DesiredPass = EPostProcessingPass::FXAA;                  break;
				case EToneMapPostProcessPass::VisualizeDepthOfField: DesiredPass = EPostProcessingPass::VisualizeDepthOfField; break;
				default:                                               DesiredPass = EPostProcessingPass::Tonemap;               break;
				}
			}
			break;
		}
	}

	if (!bFoundToneMap) return;

	if (PassId == DesiredPass)
	{
		if (InOutPassCallbacks.Num() > 0) return; // prevent double-application in PIE
		InOutPassCallbacks.Add(
			FAfterPassCallbackDelegate::CreateRaw(
				this, &FToneMapSceneViewExtension::PostProcessPass_RenderThread));
	}
}

bool FToneMapSceneViewExtension::IsActiveThisFrame_Internal(
	const FSceneViewExtensionContext& Context) const
{
	if (!WeakSubsystem.IsValid()) return false;

	for (const TWeakObjectPtr<UToneMapComponent>& Ptr : WeakSubsystem->GetComponents())
	{
		if (Ptr.IsValid() && Ptr->IsActive() && Ptr->bEnabled) return true;
	}

	return false;
}

// ---------------------------------------------------------------------------
// Main render-thread entry — the full Tone Map pipeline
// ---------------------------------------------------------------------------

FScreenPassTexture FToneMapSceneViewExtension::PostProcessPass_RenderThread(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	const FPostProcessMaterialInputs& Inputs)
{
	check(IsInRenderingThread());

	FScreenPassTexture SceneColor = FScreenPassTexture::CopyFromSlice(
		GraphBuilder, Inputs.GetInput(EPostProcessMaterialInput::SceneColor));
	if (!SceneColor.IsValid()) return SceneColor;

	const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(View);
	if (ViewInfo.bIsReflectionCapture || ViewInfo.bIsSceneCapture || !ViewInfo.bIsViewInfo)
		return SceneColor;
	if (!ViewInfo.Family->EngineShowFlags.Rendering ||
		!ViewInfo.Family->EngineShowFlags.PostProcessing ||
		ViewInfo.Family->EngineShowFlags.Wireframe)
		return SceneColor;
	if (!ViewInfo.ShaderMap)
		return SceneColor;

	UToneMapSubsystem* Subsystem = WeakSubsystem.Get();
	if (!Subsystem) return SceneColor;

	// Find first active ToneMap component
	UToneMapComponent* ActiveComp = nullptr;
	for (const TWeakObjectPtr<UToneMapComponent>& Ptr : Subsystem->GetComponents())
	{
		if (Ptr.IsValid() && Ptr->IsActive() && Ptr->bEnabled)
		{
			ActiveComp = Ptr.Get();
			break;
		}
	}

	// If nothing is active, return unchanged
	if (!ActiveComp) return SceneColor;

	const bool bIsReplaceTonemap = ActiveComp && (ActiveComp->Mode == EToneMapMode::ReplaceTonemap);

	RDG_EVENT_SCOPE(GraphBuilder, "ToneMapFX");

	// =====================================================================
	// ClassicBloom Pipeline — runs BEFORE tonemapping
	// In ReplaceTonemap mode: operates on HDR scene color
	// In PostProcess mode:   operates on LDR scene color  
	// =====================================================================
	bool bBloomApplied = false;

	if (ActiveComp->bEnableBloom && ActiveComp->BloomIntensity > 0.0f)
	{
		const FIntPoint SceneColorExtent = SceneColor.Texture->Desc.Extent;
		const FIntRect BloomViewRect = SceneColor.ViewRect;

		if (BloomViewRect.Width() > 0 && BloomViewRect.Height() > 0)
		{
			RDG_EVENT_SCOPE(GraphBuilder, "ClassicBloom");

			// Step 1: Downsample size calculation
			float DownsampleScale = FMath::Clamp(ActiveComp->DownsampleScale, 0.25f, 2.0f);
			int32 Divisor = FMath::Max(1, FMath::RoundToInt(2.0f / DownsampleScale));
			FIntPoint DownsampledExtent = FIntPoint::DivideAndRoundUp(FIntPoint(BloomViewRect.Width(), BloomViewRect.Height()), Divisor);
			FIntRect DownsampledRect = FIntRect(FIntPoint::ZeroValue, DownsampledExtent);

			if (DownsampledRect.Width() > 0 && DownsampledRect.Height() > 0)
			{
				FRDGTextureDesc BrightPassDesc = FRDGTextureDesc::Create2D(
					DownsampledExtent,
					PF_FloatRGBA,
					FClearValueBinding::Black,
					TexCreate_ShaderResource | TexCreate_RenderTargetable);

				// Step 2: Bright pass — extract bright pixels
				FRDGTextureRef BrightPassTexture = GraphBuilder.CreateTexture(BrightPassDesc, TEXT("ClassicBloom.BrightPass"));
				{
					TShaderMapRef<FClassicBloomBrightPassPS> PixelShader(ViewInfo.ShaderMap);
					if (PixelShader.IsValid())
					{
						float EffectiveThreshold = ActiveComp->BloomThreshold;
						bool bIsSoftFocusMode = (ActiveComp->BloomMode == EBloomMode::SoftFocus);
						if (bIsSoftFocusMode)
						{
							EffectiveThreshold = 0.01f;
						}

						FClassicBloomBrightPassPS::FParameters* BPParams = GraphBuilder.AllocParameters<FClassicBloomBrightPassPS::FParameters>();
						BPParams->View = View.ViewUniformBuffer;
						BPParams->SceneColorTexture = SceneColor.Texture;
						BPParams->SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
						BPParams->InputViewportSizeAndInvSize = FVector4f(BloomViewRect.Width(), BloomViewRect.Height(), 1.0f / BloomViewRect.Width(), 1.0f / BloomViewRect.Height());
						BPParams->OutputViewportSizeAndInvSize = FVector4f(DownsampledRect.Width(), DownsampledRect.Height(), 1.0f / DownsampledRect.Width(), 1.0f / DownsampledRect.Height());

						FScreenPassTextureViewport OutputViewport(DownsampledExtent, DownsampledRect);
						FScreenPassTextureViewport InputViewport(SceneColorExtent, SceneColor.ViewRect);
						BPParams->SvPositionToInputTextureUV = (
							FScreenTransform::ChangeTextureBasisFromTo(OutputViewport, FScreenTransform::ETextureBasis::TexelPosition, FScreenTransform::ETextureBasis::ViewportUV) *
							FScreenTransform::ChangeTextureBasisFromTo(InputViewport, FScreenTransform::ETextureBasis::ViewportUV, FScreenTransform::ETextureBasis::TextureUV));

						BPParams->BloomThreshold = EffectiveThreshold;
						BPParams->BloomIntensity = 1.0f;
						BPParams->ThresholdSoftness = FMath::Clamp(ActiveComp->BloomThresholdSoftness, 0.0f, 1.0f);
						BPParams->MaxBrightness = FMath::Max(ActiveComp->BloomMaxBrightness, 0.0f);
						BPParams->RenderTargets[0] = FRenderTargetBinding(BrightPassTexture, ERenderTargetLoadAction::EClear);

						FPixelShaderUtils::AddFullscreenPass(
							GraphBuilder, ViewInfo.ShaderMap,
							RDG_EVENT_NAME("BrightPass"),
							PixelShader, BPParams, DownsampledRect);
					}
				}

				// Step 3: Blur — Gaussian, Directional Glare, or Kawase
				FRDGTextureRef BlurredBloomTexture = nullptr;
				bool bUseSoftFocus = (ActiveComp->BloomMode == EBloomMode::SoftFocus);

				// --- Directional Glare ---
				if (ActiveComp->BloomMode == EBloomMode::DirectionalGlare)
				{
					int32 NumStreaks = FMath::Clamp(ActiveComp->GlareStreakCount, 2, 16);
					float StreakLength = FMath::Clamp((float)ActiveComp->GlareStreakLength, 5.0f, 200.0f);
					float ScaledStreakLength = StreakLength / (float)Divisor;
					float Falloff = FMath::Clamp(ActiveComp->GlareFalloff, 0.5f, 10.0f);
					float AngleStep = 360.0f / (float)NumStreaks;

					TShaderMapRef<FClassicBloomGlareStreakPS> GlareStreakShader(ViewInfo.ShaderMap);
					TShaderMapRef<FClassicBloomGlareAccumulatePS> GlareAccumShader(ViewInfo.ShaderMap);

					if (GlareStreakShader.IsValid() && GlareAccumShader.IsValid())
					{
						TArray<FRDGTextureRef> StreakTextures;
						StreakTextures.Reserve(NumStreaks);

						for (int32 i = 0; i < NumStreaks; ++i)
						{
							float Angle = (AngleStep * (float)i) + ActiveComp->GlareRotationOffset;
							float RadAngle = FMath::DegreesToRadians(Angle);
							FVector2f Direction(FMath::Cos(RadAngle), FMath::Sin(RadAngle));

							FRDGTextureRef StreakTexture = GraphBuilder.CreateTexture(BrightPassDesc, *FString::Printf(TEXT("ClassicBloom.Streak%d"), i));
							StreakTextures.Add(StreakTexture);

							FClassicBloomGlareStreakPS::FParameters* StreakParams = GraphBuilder.AllocParameters<FClassicBloomGlareStreakPS::FParameters>();
							StreakParams->View = View.ViewUniformBuffer;
							StreakParams->SourceTexture = BrightPassTexture;
							StreakParams->SourceSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
							StreakParams->BufferSizeAndInvSize = FVector4f(DownsampledExtent.X, DownsampledExtent.Y, 1.0f / DownsampledExtent.X, 1.0f / DownsampledExtent.Y);
							StreakParams->StreakDirection = Direction;
							StreakParams->StreakLength = ScaledStreakLength;
							StreakParams->StreakFalloff = Falloff;
							StreakParams->StreakSamples = FMath::Clamp(ActiveComp->GlareSamples, 8, 64);
							StreakParams->RenderTargets[0] = FRenderTargetBinding(StreakTexture, ERenderTargetLoadAction::EClear);

							FPixelShaderUtils::AddFullscreenPass(
								GraphBuilder, ViewInfo.ShaderMap,
								RDG_EVENT_NAME("GlareStreak%d", i),
								GlareStreakShader, StreakParams, DownsampledRect);
						}

						// Accumulate streaks
						int32 StreaksToProcess = FMath::Min(NumStreaks, 4);
						FRDGTextureRef AccumTexture = GraphBuilder.CreateTexture(BrightPassDesc, TEXT("ClassicBloom.GlareAccum"));

						FClassicBloomGlareAccumulatePS::FParameters* AccumParams = GraphBuilder.AllocParameters<FClassicBloomGlareAccumulatePS::FParameters>();
						AccumParams->View = View.ViewUniformBuffer;
						AccumParams->StreakTexture0 = StreakTextures[0];
						AccumParams->StreakTexture1 = StreaksToProcess >= 2 ? StreakTextures[1] : StreakTextures[0];
						AccumParams->StreakTexture2 = StreaksToProcess >= 3 ? StreakTextures[2] : StreakTextures[0];
						AccumParams->StreakTexture3 = StreaksToProcess >= 4 ? StreakTextures[3] : StreakTextures[0];
						AccumParams->StreakSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
						AccumParams->GlareViewportSizeAndInvSize = FVector4f(DownsampledRect.Width(), DownsampledRect.Height(), 1.0f / DownsampledRect.Width(), 1.0f / DownsampledRect.Height());
						AccumParams->NumStreaks = StreaksToProcess;
						AccumParams->RenderTargets[0] = FRenderTargetBinding(AccumTexture, ERenderTargetLoadAction::EClear);

						FPixelShaderUtils::AddFullscreenPass(
							GraphBuilder, ViewInfo.ShaderMap,
							RDG_EVENT_NAME("GlareAccumulate"),
							GlareAccumShader, AccumParams, DownsampledRect);

						// Additional batches for >4 streaks
						if (NumStreaks > 4)
						{
							FRDGTextureRef PrevAccum = AccumTexture;
							for (int32 BatchStart = 4; BatchStart < NumStreaks; BatchStart += 3)
							{
								FRDGTextureRef NextAccum = GraphBuilder.CreateTexture(BrightPassDesc, *FString::Printf(TEXT("ClassicBloom.GlareAccum%d"), BatchStart));
								int32 StreaksInBatch = FMath::Min(3, NumStreaks - BatchStart);

								AccumParams = GraphBuilder.AllocParameters<FClassicBloomGlareAccumulatePS::FParameters>();
								AccumParams->View = View.ViewUniformBuffer;
								AccumParams->StreakTexture0 = PrevAccum;
								AccumParams->StreakTexture1 = StreakTextures[BatchStart];
								AccumParams->StreakTexture2 = StreaksInBatch >= 2 ? StreakTextures[BatchStart + 1] : StreakTextures[BatchStart];
								AccumParams->StreakTexture3 = StreaksInBatch >= 3 ? StreakTextures[BatchStart + 2] : StreakTextures[BatchStart];
								AccumParams->StreakSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
								AccumParams->GlareViewportSizeAndInvSize = FVector4f(DownsampledRect.Width(), DownsampledRect.Height(), 1.0f / DownsampledRect.Width(), 1.0f / DownsampledRect.Height());
								AccumParams->NumStreaks = 1 + StreaksInBatch;
								AccumParams->RenderTargets[0] = FRenderTargetBinding(NextAccum, ERenderTargetLoadAction::EClear);

								FPixelShaderUtils::AddFullscreenPass(
									GraphBuilder, ViewInfo.ShaderMap,
									RDG_EVENT_NAME("GlareAccumulate%d", BatchStart),
									GlareAccumShader, AccumParams, DownsampledRect);

								PrevAccum = NextAccum;
							}
							AccumTexture = PrevAccum;
						}

						// Light Gaussian blur to smooth the glare
						FRDGTextureRef GlareBlurTemp = GraphBuilder.CreateTexture(BrightPassDesc, TEXT("ClassicBloom.GlareBlurTemp"));
						BlurredBloomTexture = GraphBuilder.CreateTexture(BrightPassDesc, TEXT("ClassicBloom.GlareBlurred"));

						TShaderMapRef<FClassicBloomBlurPS> BlurShader(ViewInfo.ShaderMap);

						// Horizontal blur
						{
							FClassicBloomBlurPS::FParameters* BlurParams = GraphBuilder.AllocParameters<FClassicBloomBlurPS::FParameters>();
							BlurParams->View = View.ViewUniformBuffer;
							BlurParams->SourceTexture = AccumTexture;
							BlurParams->SourceSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
							BlurParams->BufferSizeAndInvSize = FVector4f(DownsampledExtent.X, DownsampledExtent.Y, 1.0f / DownsampledExtent.X, 1.0f / DownsampledExtent.Y);
							BlurParams->BlurDirection = FVector2f(1.0f, 0.0f);
							BlurParams->BlurRadius = ActiveComp->BloomSize * 0.05f;
							BlurParams->RenderTargets[0] = FRenderTargetBinding(GlareBlurTemp, ERenderTargetLoadAction::EClear);

							FPixelShaderUtils::AddFullscreenPass(GraphBuilder, ViewInfo.ShaderMap, RDG_EVENT_NAME("GlareBlurH"), BlurShader, BlurParams, DownsampledRect);
						}

						// Vertical blur
						{
							FClassicBloomBlurPS::FParameters* BlurParams = GraphBuilder.AllocParameters<FClassicBloomBlurPS::FParameters>();
							BlurParams->View = View.ViewUniformBuffer;
							BlurParams->SourceTexture = GlareBlurTemp;
							BlurParams->SourceSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
							BlurParams->BufferSizeAndInvSize = FVector4f(DownsampledExtent.X, DownsampledExtent.Y, 1.0f / DownsampledExtent.X, 1.0f / DownsampledExtent.Y);
							BlurParams->BlurDirection = FVector2f(0.0f, 1.0f);
							BlurParams->BlurRadius = ActiveComp->BloomSize * 0.05f;
							BlurParams->RenderTargets[0] = FRenderTargetBinding(BlurredBloomTexture, ERenderTargetLoadAction::EClear);

							FPixelShaderUtils::AddFullscreenPass(GraphBuilder, ViewInfo.ShaderMap, RDG_EVENT_NAME("GlareBlurV"), BlurShader, BlurParams, DownsampledRect);
						}
					}
				}

				// --- Kawase Bloom ---
				if (ActiveComp->BloomMode == EBloomMode::Kawase && !BlurredBloomTexture)
				{
					TShaderMapRef<FClassicBloomKawaseDownsamplePS> KawaseDownsampleShader(ViewInfo.ShaderMap);
					TShaderMapRef<FClassicBloomKawaseUpsamplePS> KawaseUpsampleShader(ViewInfo.ShaderMap);

					if (KawaseDownsampleShader.IsValid() && KawaseUpsampleShader.IsValid())
					{
						int32 MipCount = FMath::Clamp(ActiveComp->KawaseMipCount, 3, 8);
						float FilterRadius = FMath::Clamp(ActiveComp->KawaseFilterRadius, 0.0001f, 0.01f);
						float ThresholdKnee = ActiveComp->bKawaseSoftThreshold ? FMath::Clamp(ActiveComp->KawaseThresholdKnee, 0.0f, 1.0f) : 0.0f;

						TArray<FRDGTextureRef> MipTextures;
						TArray<FIntPoint> MipExtents;
						TArray<FIntRect> MipRects;
						MipTextures.Reserve(MipCount);
						MipExtents.Reserve(MipCount);
						MipRects.Reserve(MipCount);

						FIntPoint CurrentExtent = DownsampledExtent;
						FIntRect CurrentRect = DownsampledRect;

						for (int32 Mip = 0; Mip < MipCount; ++Mip)
						{
							CurrentExtent = FIntPoint::DivideAndRoundUp(CurrentExtent, 2);
							CurrentRect = FIntRect(FIntPoint::ZeroValue, FIntPoint::DivideAndRoundUp(FIntPoint(CurrentRect.Width(), CurrentRect.Height()), 2));
							CurrentExtent.X = FMath::Max(CurrentExtent.X, 1);
							CurrentExtent.Y = FMath::Max(CurrentExtent.Y, 1);
							CurrentRect.Max.X = FMath::Max(CurrentRect.Max.X, 1);
							CurrentRect.Max.Y = FMath::Max(CurrentRect.Max.Y, 1);

							FRDGTextureDesc MipDesc = FRDGTextureDesc::Create2D(
								CurrentExtent, PF_FloatRGBA, FClearValueBinding::Black,
								TexCreate_ShaderResource | TexCreate_RenderTargetable);

							MipTextures.Add(GraphBuilder.CreateTexture(MipDesc, *FString::Printf(TEXT("ClassicBloom.KawaseMip%d"), Mip)));
							MipExtents.Add(CurrentExtent);
							MipRects.Add(CurrentRect);
						}

						// Downsample pass: create mip pyramid from scene color
						FRDGTextureRef DownsampleSource = SceneColor.Texture;
						FIntPoint SourceExtent = SceneColorExtent;
						FIntRect SourceRect = SceneColor.ViewRect;

						for (int32 Mip = 0; Mip < MipCount; ++Mip)
						{
							FClassicBloomKawaseDownsamplePS::FParameters* DownParams = GraphBuilder.AllocParameters<FClassicBloomKawaseDownsamplePS::FParameters>();
							DownParams->View = View.ViewUniformBuffer;
							DownParams->SourceTexture = DownsampleSource;
							DownParams->SourceSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
							DownParams->SourceSizeAndInvSize = FVector4f(SourceExtent.X, SourceExtent.Y, 1.0f / SourceExtent.X, 1.0f / SourceExtent.Y);
							DownParams->OutputSizeAndInvSize = FVector4f(MipExtents[Mip].X, MipExtents[Mip].Y, 1.0f / MipExtents[Mip].X, 1.0f / MipExtents[Mip].Y);

							FScreenPassTextureViewport OutVP(MipExtents[Mip], MipRects[Mip]);
							FScreenPassTextureViewport SrcVP(SourceExtent, SourceRect);
							DownParams->SvPositionToSourceUV = (
								FScreenTransform::ChangeTextureBasisFromTo(OutVP, FScreenTransform::ETextureBasis::TexelPosition, FScreenTransform::ETextureBasis::ViewportUV) *
								FScreenTransform::ChangeTextureBasisFromTo(SrcVP, FScreenTransform::ETextureBasis::ViewportUV, FScreenTransform::ETextureBasis::TextureUV));

							DownParams->BloomThreshold = ActiveComp->BloomThreshold;
							DownParams->ThresholdKnee = ThresholdKnee;
							DownParams->MipLevel = Mip;
							DownParams->bUseKarisAverage = (Mip == 0) ? 1 : 0;
							DownParams->RenderTargets[0] = FRenderTargetBinding(MipTextures[Mip], ERenderTargetLoadAction::EClear);

							FPixelShaderUtils::AddFullscreenPass(
								GraphBuilder, ViewInfo.ShaderMap,
								RDG_EVENT_NAME("KawaseDownsample_Mip%d", Mip),
								KawaseDownsampleShader, DownParams, MipRects[Mip]);

							DownsampleSource = MipTextures[Mip];
							SourceExtent = MipExtents[Mip];
							SourceRect = MipRects[Mip];
						}

						// Upsample pass: progressive upsample with additive blend
						TArray<FRDGTextureRef> UpsampleTextures;
						UpsampleTextures.Reserve(MipCount - 1);

						for (int32 Mip = MipCount - 2; Mip >= 0; --Mip)
						{
							FRDGTextureDesc UpsampleDesc = FRDGTextureDesc::Create2D(
								MipExtents[Mip], PF_FloatRGBA, FClearValueBinding::Black,
								TexCreate_ShaderResource | TexCreate_RenderTargetable);
							UpsampleTextures.Add(GraphBuilder.CreateTexture(UpsampleDesc, *FString::Printf(TEXT("ClassicBloom.KawaseUpsample%d"), Mip)));
						}

						FRDGTextureRef UpsampleSource = MipTextures[MipCount - 1];
						int32 UpsampleIdx = 0;
						for (int32 Mip = MipCount - 2; Mip >= 0; --Mip)
						{
							FClassicBloomKawaseUpsamplePS::FParameters* UpParams = GraphBuilder.AllocParameters<FClassicBloomKawaseUpsamplePS::FParameters>();
							UpParams->View = View.ViewUniformBuffer;
							UpParams->SourceTexture = UpsampleSource;
							UpParams->PreviousMipTexture = MipTextures[Mip];
							UpParams->SourceSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
							UpParams->OutputSizeAndInvSize = FVector4f(MipExtents[Mip].X, MipExtents[Mip].Y, 1.0f / MipExtents[Mip].X, 1.0f / MipExtents[Mip].Y);
							UpParams->FilterRadius = FilterRadius;
							UpParams->RenderTargets[0] = FRenderTargetBinding(UpsampleTextures[UpsampleIdx], ERenderTargetLoadAction::EClear);

							FPixelShaderUtils::AddFullscreenPass(
								GraphBuilder, ViewInfo.ShaderMap,
								RDG_EVENT_NAME("KawaseUpsample_Mip%d", Mip),
								KawaseUpsampleShader, UpParams, MipRects[Mip]);

							UpsampleSource = UpsampleTextures[UpsampleIdx];
							++UpsampleIdx;
						}

						// Final upsample to original downsampled size
						if (UpsampleTextures.Num() > 0)
						{
							BlurredBloomTexture = GraphBuilder.CreateTexture(BrightPassDesc, TEXT("ClassicBloom.KawaseBlurred"));

							FClassicBloomKawaseUpsamplePS::FParameters* FinalUpParams = GraphBuilder.AllocParameters<FClassicBloomKawaseUpsamplePS::FParameters>();
							FinalUpParams->View = View.ViewUniformBuffer;
							FinalUpParams->SourceTexture = UpsampleTextures.Last();
							FinalUpParams->PreviousMipTexture = MipTextures[0];
							FinalUpParams->SourceSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
							FinalUpParams->OutputSizeAndInvSize = FVector4f(DownsampledExtent.X, DownsampledExtent.Y, 1.0f / DownsampledExtent.X, 1.0f / DownsampledExtent.Y);
							FinalUpParams->FilterRadius = FilterRadius;
							FinalUpParams->RenderTargets[0] = FRenderTargetBinding(BlurredBloomTexture, ERenderTargetLoadAction::EClear);

							FPixelShaderUtils::AddFullscreenPass(
								GraphBuilder, ViewInfo.ShaderMap,
								RDG_EVENT_NAME("KawaseUpsample_Final"),
								KawaseUpsampleShader, FinalUpParams, DownsampledRect);
						}
						else
						{
							BlurredBloomTexture = MipTextures.Num() > 0 ? MipTextures[0] : BrightPassTexture;
						}
					}
				}

				// --- Standard Gaussian blur (or fallback) ---
				if (!BlurredBloomTexture)
				{
					int32 NumBlurPasses = FMath::Clamp(ActiveComp->BlurPasses, 1, 4);
					FRDGTextureRef BlurSource = BrightPassTexture;
					FRDGTextureRef BlurTempTexture = GraphBuilder.CreateTexture(BrightPassDesc, TEXT("ClassicBloom.BlurTemp"));
					BlurredBloomTexture = GraphBuilder.CreateTexture(BrightPassDesc, TEXT("ClassicBloom.Blurred"));

					TShaderMapRef<FClassicBloomBlurPS> BlurShader(ViewInfo.ShaderMap);
					for (int32 PassIndex = 0; PassIndex < NumBlurPasses; ++PassIndex)
					{
						// Horizontal
						{
							FClassicBloomBlurPS::FParameters* BlurParams = GraphBuilder.AllocParameters<FClassicBloomBlurPS::FParameters>();
							BlurParams->View = View.ViewUniformBuffer;
							BlurParams->SourceTexture = BlurSource;
							BlurParams->SourceSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
							BlurParams->BufferSizeAndInvSize = FVector4f(DownsampledExtent.X, DownsampledExtent.Y, 1.0f / DownsampledExtent.X, 1.0f / DownsampledExtent.Y);
							BlurParams->BlurDirection = FVector2f(1.0f, 0.0f);
							BlurParams->BlurRadius = ActiveComp->BloomSize * 0.1f;
							BlurParams->RenderTargets[0] = FRenderTargetBinding(BlurTempTexture, ERenderTargetLoadAction::EClear);

							FPixelShaderUtils::AddFullscreenPass(GraphBuilder, ViewInfo.ShaderMap, RDG_EVENT_NAME("BlurHorizontal"), BlurShader, BlurParams, DownsampledRect);
						}

						// Vertical
						{
							FClassicBloomBlurPS::FParameters* BlurParams = GraphBuilder.AllocParameters<FClassicBloomBlurPS::FParameters>();
							BlurParams->View = View.ViewUniformBuffer;
							BlurParams->SourceTexture = BlurTempTexture;
							BlurParams->SourceSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
							BlurParams->BufferSizeAndInvSize = FVector4f(DownsampledExtent.X, DownsampledExtent.Y, 1.0f / DownsampledExtent.X, 1.0f / DownsampledExtent.Y);
							BlurParams->BlurDirection = FVector2f(0.0f, 1.0f);
							BlurParams->BlurRadius = ActiveComp->BloomSize * 0.1f;
							BlurParams->RenderTargets[0] = FRenderTargetBinding(BlurredBloomTexture, ERenderTargetLoadAction::EClear);

							FPixelShaderUtils::AddFullscreenPass(GraphBuilder, ViewInfo.ShaderMap, RDG_EVENT_NAME("BlurVertical"), BlurShader, BlurParams, DownsampledRect);
						}

						BlurSource = BlurredBloomTexture;
					}
				}

				// Step 4: Composite bloom back onto scene color
				if (BlurredBloomTexture)
				{
					FRDGTextureDesc CompositeDesc = SceneColor.Texture->Desc;
					CompositeDesc.ClearValue = FClearValueBinding::Black;
					CompositeDesc.Flags |= TexCreate_RenderTargetable | TexCreate_ShaderResource;
					FRDGTextureRef CompositeOutput = GraphBuilder.CreateTexture(CompositeDesc, TEXT("ClassicBloom.Composite"));
					FIntRect CompositeViewRect = SceneColor.ViewRect;

					TShaderMapRef<FClassicBloomCompositePS> CompositeShader(ViewInfo.ShaderMap);
					if (CompositeShader.IsValid())
					{
						FClassicBloomCompositePS::FParameters* CParams = GraphBuilder.AllocParameters<FClassicBloomCompositePS::FParameters>();
						CParams->View = View.ViewUniformBuffer;
						CParams->SceneColorTexture = SceneColor.Texture;
						CParams->SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
						CParams->BloomTexture = BlurredBloomTexture;
						CParams->BloomSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
						CParams->OutputViewportSizeAndInvSize = FVector4f(CompositeViewRect.Width(), CompositeViewRect.Height(), 1.0f / CompositeViewRect.Width(), 1.0f / CompositeViewRect.Height());

						FScreenPassTextureViewport CompositeOutputVP(CompositeDesc.Extent, CompositeViewRect);
						FScreenPassTextureViewport SceneColorInputVP(SceneColorExtent, SceneColor.ViewRect);
						FScreenPassTextureViewport BloomVP(DownsampledExtent, DownsampledRect);

						CParams->SvPositionToSceneColorUV = (
							FScreenTransform::ChangeTextureBasisFromTo(CompositeOutputVP, FScreenTransform::ETextureBasis::TexelPosition, FScreenTransform::ETextureBasis::ViewportUV) *
							FScreenTransform::ChangeTextureBasisFromTo(SceneColorInputVP, FScreenTransform::ETextureBasis::ViewportUV, FScreenTransform::ETextureBasis::TextureUV));

						CParams->SvPositionToBloomUV = (
							FScreenTransform::ChangeTextureBasisFromTo(CompositeOutputVP, FScreenTransform::ETextureBasis::TexelPosition, FScreenTransform::ETextureBasis::ViewportUV) *
							FScreenTransform::ChangeTextureBasisFromTo(BloomVP, FScreenTransform::ETextureBasis::ViewportUV, FScreenTransform::ETextureBasis::TextureUV));

						CParams->BloomIntensity = bUseSoftFocus ? 0.0f : ActiveComp->BloomIntensity;

						FLinearColor TintWithFlag = ActiveComp->BloomTint;
						TintWithFlag.A = ActiveComp->bUseSceneColor ? 1.0f : 0.0f;
						CParams->BloomTint = FVector4f(TintWithFlag);

						CParams->BloomBlendMode = (float)ActiveComp->BloomBlendMode;
						CParams->BloomSaturation = ActiveComp->BloomSaturation;
						CParams->bProtectHighlights = ActiveComp->bProtectHighlights ? 1.0f : 0.0f;
						CParams->HighlightProtection = ActiveComp->HighlightProtection;
						CParams->SoftFocusIntensity = bUseSoftFocus ? ActiveComp->BloomIntensity : 0.0f;
						CParams->SoftFocusParams = FVector4f(
							ActiveComp->SoftFocusOverlayMultiplier,
							ActiveComp->SoftFocusBlendStrength,
							ActiveComp->SoftFocusSoftLightMultiplier,
							ActiveComp->SoftFocusFinalBlend);

						// Removed debug options — set safe defaults
						CParams->bUseAdaptiveScaling = 0.0f;
						CParams->bShowBloomOnly = 0.0f;
						CParams->bShowGammaCompensation = 0.0f;
						CParams->bIsGameWorld = (ViewInfo.Family->Scene && ViewInfo.Family->Scene->GetWorld() && ViewInfo.Family->Scene->GetWorld()->IsGameWorld()) ? 1.0f : 0.0f;
						CParams->GameModeBloomScale = 1.0f;

						CParams->bUseBrightnessCompensation = 0.0f;
						CParams->BrightnessCompensationMode = 0.0f;
						CParams->BrightnessCompensationStrength = 0.0f;
						CParams->ExposureCompensation = 0.0f;

						// EClear ensures pixels outside CompositeViewRect are black
						// (texture extent may be larger than viewport when window is not maximized)
						CParams->RenderTargets[0] = FRenderTargetBinding(CompositeOutput, ERenderTargetLoadAction::EClear);

						FPixelShaderUtils::AddFullscreenPass(
							GraphBuilder, ViewInfo.ShaderMap,
							RDG_EVENT_NAME("CompositeBloom"),
							CompositeShader, CParams, CompositeViewRect);

						// Replace SceneColor with the bloom-composited result for downstream ToneMap processing
						SceneColor = FScreenPassTexture(CompositeOutput, CompositeViewRect);
						bBloomApplied = true;
					}
				}
			} // DownsampledRect valid
		} // BloomViewRect valid
	} // Bloom active

	// bIsReplaceTonemap already determined above

	const FScreenPassTextureViewport SceneColorViewport(SceneColor);
	const FIntPoint ViewportSize = SceneColorViewport.Rect.Size();

	// =====================================================================
	// Get bloom texture (ReplaceTonemap mode only, skipped if ClassicBloom already composited)
	// =====================================================================
	FScreenPassTexture BloomInput;
	if (bIsReplaceTonemap && !bBloomApplied)
	{
		FScreenPassTextureSlice BloomSlice = Inputs.GetInput(EPostProcessMaterialInput::CombinedBloom);
		if (BloomSlice.IsValid())
		{
			BloomInput = FScreenPassTexture::CopyFromSlice(GraphBuilder, BloomSlice);
		}
	}

	// =====================================================================
	// Krawczyk Auto-Exposure — Luminance measurement & temporal adaptation
	// =====================================================================
	// Only runs when mode is Krawczyk and we're in ReplaceTonemap mode.
	// Pipeline:
	//   1. LuminanceMeasurePS  — 16x16 grid sampling → geometric mean (1x1)
	//   2. LuminanceAdaptPS    — Exponential blend with previous frame (1x1)
	//   3. Result passed to main shader as AdaptedLumTexture
	// =====================================================================

	const bool bNeedKrawczyk = bIsReplaceTonemap &&
		(ActiveComp->AutoExposureMode == EToneMapAutoExposure::Krawczyk);

	FRDGTextureRef AdaptedLumTexture = nullptr;

	if (bNeedKrawczyk)
	{
		const float OneOverPreExposure = 1.0f / FMath::Max(ViewInfo.PreExposure, 0.001f);

		// Compute scene color UV bounds (viewport rect in texture UV space)
		const FIntRect& SceneVR = SceneColorViewport.Rect;
		const FIntPoint SceneExt = SceneColor.Texture->Desc.Extent;
		FVector4f UVBounds(
			(float)SceneVR.Min.X / SceneExt.X,
			(float)SceneVR.Min.Y / SceneExt.Y,
			(float)SceneVR.Max.X / SceneExt.X,
			(float)SceneVR.Max.Y / SceneExt.Y);

		// --- Step 1: Measure scene luminance (→ 1x1 texture) ---
		FRDGTextureRef MeasuredLumTexture;
		{
			FRDGTextureDesc LumDesc = FRDGTextureDesc::Create2D(
				FIntPoint(1, 1), PF_R32_FLOAT, FClearValueBinding::None,
				TexCreate_ShaderResource | TexCreate_RenderTargetable);

			MeasuredLumTexture = GraphBuilder.CreateTexture(LumDesc, TEXT("ToneMap.MeasuredLum"));

			auto* P = GraphBuilder.AllocParameters<FToneMapLumMeasurePS::FParameters>();
			P->View                = ViewInfo.ViewUniformBuffer;
			P->SceneColorTexture   = SceneColor.Texture;
			P->SceneColorSampler   = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			P->SceneColorUVBounds  = UVBounds;
			P->OneOverPreExposure  = OneOverPreExposure;
			P->RenderTargets[0]    = FRenderTargetBinding(MeasuredLumTexture, ERenderTargetLoadAction::ENoAction);

			TShaderMapRef<FToneMapLumMeasurePS> MeasureShader(ViewInfo.ShaderMap);
			FPixelShaderUtils::AddFullscreenPass(
				GraphBuilder, ViewInfo.ShaderMap,
				RDG_EVENT_NAME("ToneMap_LuminanceMeasure"),
				MeasureShader, P,
				FIntRect(0, 0, 1, 1));
		}

		// --- Step 2: Temporal adaptation (→ 1x1 persistent texture) ---
		if (AdaptedLuminanceRT.IsValid())
		{
			// Blend previous adapted luminance with new measurement
			FRDGTextureRef PrevAdaptedLum = GraphBuilder.RegisterExternalTexture(
				AdaptedLuminanceRT, TEXT("ToneMap.PrevAdaptedLum"));

			FRDGTextureDesc AdaptDesc = FRDGTextureDesc::Create2D(
				FIntPoint(1, 1), PF_R32_FLOAT, FClearValueBinding::None,
				TexCreate_ShaderResource | TexCreate_RenderTargetable);

			AdaptedLumTexture = GraphBuilder.CreateTexture(AdaptDesc, TEXT("ToneMap.AdaptedLum"));

			auto* P = GraphBuilder.AllocParameters<FToneMapLumAdaptPS::FParameters>();
			P->View                   = ViewInfo.ViewUniformBuffer;
			P->PrevAdaptedLumTexture   = PrevAdaptedLum;
			P->PrevAdaptedLumSampler   = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			P->CurrentLumTexture       = MeasuredLumTexture;
			P->CurrentLumSampler       = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			P->AdaptSpeedUp            = ActiveComp->AdaptationSpeedUp;
			P->AdaptSpeedDown          = ActiveComp->AdaptationSpeedDown;
			P->DeltaTime               = FMath::Max(LastDeltaTime, 0.001f);
			P->RenderTargets[0]        = FRenderTargetBinding(AdaptedLumTexture, ERenderTargetLoadAction::ENoAction);

			TShaderMapRef<FToneMapLumAdaptPS> AdaptShader(ViewInfo.ShaderMap);
			FPixelShaderUtils::AddFullscreenPass(
				GraphBuilder, ViewInfo.ShaderMap,
				RDG_EVENT_NAME("ToneMap_LuminanceAdapt"),
				AdaptShader, P,
				FIntRect(0, 0, 1, 1));
		}
		else
		{
			// First frame: use measured luminance directly (instant adaptation)
			AdaptedLumTexture = MeasuredLumTexture;
		}

		// Extract adapted luminance for next frame's temporal blending
		GraphBuilder.QueueTextureExtraction(AdaptedLumTexture, &AdaptedLuminanceRT);
	}

	// =====================================================================
	// Clarity blur passes (skip when Clarity == 0 for performance)
	// =====================================================================

	FRDGTextureRef BlurredTexture = SceneColor.Texture; // default: no blur

	if (FMath::Abs(ActiveComp->Clarity) > 0.01f)
	{
		FRDGTextureDesc BlurDesc = FRDGTextureDesc::Create2D(
			ViewportSize, PF_FloatRGBA, FClearValueBinding::None,
			TexCreate_ShaderResource | TexCreate_RenderTargetable);

		// --- Horizontal blur ---
		FRDGTextureRef HBlurTexture = GraphBuilder.CreateTexture(BlurDesc, TEXT("ToneMap.HBlur"));
		{
			auto* P = GraphBuilder.AllocParameters<FToneMapBlurPS::FParameters>();
			P->View            = ViewInfo.ViewUniformBuffer;
			P->SourceTexture   = SceneColor.Texture;
			P->SourceSampler   = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			P->BufferSizeAndInvSize = FVector4f(
				ViewportSize.X, ViewportSize.Y,
				1.0f / ViewportSize.X, 1.0f / ViewportSize.Y);
			P->BlurDirection   = FVector2f(1.0f, 0.0f);
			P->BlurRadius      = ActiveComp->ClarityRadius;
			const FIntRect& SceneVR = SceneColorViewport.Rect;
			const FIntPoint SceneExt = SceneColor.Texture->Desc.Extent;
			P->SourceViewportRect = FVector4f(
				(float)SceneVR.Min.X, (float)SceneVR.Min.Y,
				(float)SceneVR.Max.X, (float)SceneVR.Max.Y);
			P->SourceExtentInv = FVector4f(
				1.0f / SceneExt.X, 1.0f / SceneExt.Y, 0.0f, 0.0f);
			P->RenderTargets[0] = FRenderTargetBinding(HBlurTexture, ERenderTargetLoadAction::ENoAction);

			TShaderMapRef<FToneMapBlurPS> BlurShaderH(ViewInfo.ShaderMap);
			FPixelShaderUtils::AddFullscreenPass(
				GraphBuilder, ViewInfo.ShaderMap,
				RDG_EVENT_NAME("ToneMap_HBlur"),
				BlurShaderH, P,
				FIntRect(0, 0, ViewportSize.X, ViewportSize.Y));
		}

		// --- Vertical blur ---
		FRDGTextureRef VBlurTexture = GraphBuilder.CreateTexture(BlurDesc, TEXT("ToneMap.VBlur"));
		{
			auto* P = GraphBuilder.AllocParameters<FToneMapBlurPS::FParameters>();
			P->View            = ViewInfo.ViewUniformBuffer;
			P->SourceTexture   = HBlurTexture;
			P->SourceSampler   = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			P->BufferSizeAndInvSize = FVector4f(
				ViewportSize.X, ViewportSize.Y,
				1.0f / ViewportSize.X, 1.0f / ViewportSize.Y);
			P->BlurDirection   = FVector2f(0.0f, 1.0f);
			P->BlurRadius      = ActiveComp->ClarityRadius;
			P->SourceViewportRect = FVector4f(
				0.0f, 0.0f, (float)ViewportSize.X, (float)ViewportSize.Y);
			P->SourceExtentInv = FVector4f(
				1.0f / ViewportSize.X, 1.0f / ViewportSize.Y, 0.0f, 0.0f);
			P->RenderTargets[0] = FRenderTargetBinding(VBlurTexture, ERenderTargetLoadAction::ENoAction);

			TShaderMapRef<FToneMapBlurPS> BlurShaderV(ViewInfo.ShaderMap);
			FPixelShaderUtils::AddFullscreenPass(
				GraphBuilder, ViewInfo.ShaderMap,
				RDG_EVENT_NAME("ToneMap_VBlur"),
				BlurShaderV, P,
				FIntRect(0, 0, ViewportSize.X, ViewportSize.Y));
		}

		BlurredTexture = VBlurTexture;
	}

	// =====================================================================
	// Dynamic Contrast blur passes — Fine (radius 2) and Coarse (radius 32)
	// Generates 3-scale blur pyramid for multi-scale local contrast.
	// Skipped entirely when all Dynamic Contrast sliders are zero.
	// =====================================================================

	const bool bNeedDynamicContrastBlurs =
		(ActiveComp->DynamicContrast > 0.01f ||
		 ActiveComp->CorrectContrast > 0.01f ||
		 ActiveComp->CorrectColorCast > 0.01f);

	FRDGTextureRef BlurredFineTexture  = SceneColor.Texture;  // fallback
	FRDGTextureRef BlurredCoarseTexture = SceneColor.Texture;  // fallback

	if (bNeedDynamicContrastBlurs)
	{
		FRDGTextureDesc BlurDesc = FRDGTextureDesc::Create2D(
			ViewportSize, PF_FloatRGBA, FClearValueBinding::None,
			TexCreate_ShaderResource | TexCreate_RenderTargetable);

		const FIntRect& SceneVR = SceneColorViewport.Rect;
		const FIntPoint SceneExt = SceneColor.Texture->Desc.Extent;

		// Helper lambda: run a separable H+V Gaussian blur pass pair
		auto RunBlurPair = [&](FRDGTextureRef InputTexture,
			const FVector4f& InputViewportRect, const FVector4f& InputExtentInv,
			const FIntPoint& InputSize, float Radius,
			const TCHAR* NameH, const TCHAR* NameV,
			const TCHAR* TexNameH, const TCHAR* TexNameV) -> FRDGTextureRef
		{
			// H pass
			FRDGTextureRef HTex = GraphBuilder.CreateTexture(BlurDesc, TexNameH);
			{
				auto* P = GraphBuilder.AllocParameters<FToneMapBlurPS::FParameters>();
				P->View              = ViewInfo.ViewUniformBuffer;
				P->SourceTexture     = InputTexture;
				P->SourceSampler     = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
				P->BufferSizeAndInvSize = FVector4f(
					InputSize.X, InputSize.Y,
					1.0f / InputSize.X, 1.0f / InputSize.Y);
				P->BlurDirection     = FVector2f(1.0f, 0.0f);
				P->BlurRadius        = Radius;
				P->SourceViewportRect = InputViewportRect;
				P->SourceExtentInv   = InputExtentInv;
				P->RenderTargets[0]  = FRenderTargetBinding(HTex, ERenderTargetLoadAction::ENoAction);

				TShaderMapRef<FToneMapBlurPS> Shader(ViewInfo.ShaderMap);
				FPixelShaderUtils::AddFullscreenPass(
					GraphBuilder, ViewInfo.ShaderMap,
					FRDGEventName(NameH), Shader, P,
					FIntRect(0, 0, ViewportSize.X, ViewportSize.Y));
			}

			// V pass
			FRDGTextureRef VTex = GraphBuilder.CreateTexture(BlurDesc, TexNameV);
			{
				auto* P = GraphBuilder.AllocParameters<FToneMapBlurPS::FParameters>();
				P->View              = ViewInfo.ViewUniformBuffer;
				P->SourceTexture     = HTex;
				P->SourceSampler     = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
				P->BufferSizeAndInvSize = FVector4f(
					ViewportSize.X, ViewportSize.Y,
					1.0f / ViewportSize.X, 1.0f / ViewportSize.Y);
				P->BlurDirection     = FVector2f(0.0f, 1.0f);
				P->BlurRadius        = Radius;
				P->SourceViewportRect = FVector4f(0.0f, 0.0f, (float)ViewportSize.X, (float)ViewportSize.Y);
				P->SourceExtentInv   = FVector4f(1.0f / ViewportSize.X, 1.0f / ViewportSize.Y, 0.0f, 0.0f);
				P->RenderTargets[0]  = FRenderTargetBinding(VTex, ERenderTargetLoadAction::ENoAction);

				TShaderMapRef<FToneMapBlurPS> Shader(ViewInfo.ShaderMap);
				FPixelShaderUtils::AddFullscreenPass(
					GraphBuilder, ViewInfo.ShaderMap,
					FRDGEventName(NameV), Shader, P,
					FIntRect(0, 0, ViewportSize.X, ViewportSize.Y));
			}

			return VTex;
		};

		// Fine blur: radius 2 — captures high-frequency surface detail
		BlurredFineTexture = RunBlurPair(
			SceneColor.Texture,
			FVector4f((float)SceneVR.Min.X, (float)SceneVR.Min.Y,
			          (float)SceneVR.Max.X, (float)SceneVR.Max.Y),
			FVector4f(1.0f / SceneExt.X, 1.0f / SceneExt.Y, 0.0f, 0.0f),
			ViewportSize, 2.0f,
			TEXT("ToneMap_DynamicContrast_FineH"), TEXT("ToneMap_DynamicContrast_FineV"),
			TEXT("ToneMap.DynamicContrast.FineH"), TEXT("ToneMap.DynamicContrast.FineV"));

		// Coarse blur: radius 32 — captures large-scale tonal structure
		BlurredCoarseTexture = RunBlurPair(
			SceneColor.Texture,
			FVector4f((float)SceneVR.Min.X, (float)SceneVR.Min.Y,
			          (float)SceneVR.Max.X, (float)SceneVR.Max.Y),
			FVector4f(1.0f / SceneExt.X, 1.0f / SceneExt.Y, 0.0f, 0.0f),
			ViewportSize, 32.0f,
			TEXT("ToneMap_DynamicContrast_CoarseH"), TEXT("ToneMap_DynamicContrast_CoarseV"),
			TEXT("ToneMap.DynamicContrast.CoarseH"), TEXT("ToneMap.DynamicContrast.CoarseV"));
	}

	// =====================================================================
	// Durand-Dorsey 2002 Bilateral Tone Mapping — pre-pass
	// Runs before ToneMapProcess; sets bPreToneMapped so the film curve is skipped.
	// =====================================================================
	FRDGTextureRef PreToneMappedTexture = nullptr;
	bool bPreToneMapped = false;

	if (bIsReplaceTonemap && ActiveComp->FilmCurve == EToneMapFilmCurve::Durand)
	{
		RDG_EVENT_SCOPE(GraphBuilder, "ToneMapFX_Durand");

		const FIntPoint WS = ViewportSize;
		const FVector4f BilateralBufferSize((float)WS.X, (float)WS.Y, 1.0f / WS.X, 1.0f / WS.Y);

		const FScreenPassTextureViewport DurandWorkVP(WS, FIntRect(0, 0, WS.X, WS.Y));
		const FScreenPassTextureViewport SceneColorInputVP_D(
			FIntPoint(SceneColor.Texture->Desc.Extent.X, SceneColor.Texture->Desc.Extent.Y),
			SceneColorViewport.Rect);
		const FScreenTransform DurandSceneColorUV = (
			FScreenTransform::ChangeTextureBasisFromTo(DurandWorkVP, FScreenTransform::ETextureBasis::TexelPosition, FScreenTransform::ETextureBasis::ViewportUV) *
			FScreenTransform::ChangeTextureBasisFromTo(SceneColorInputVP_D, FScreenTransform::ETextureBasis::ViewportUV, FScreenTransform::ETextureBasis::TextureUV));

		// --- Pass 1: log-luminance ---
		FRDGTextureRef LogLumTex = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(WS, PF_R32_FLOAT, FClearValueBinding::None,
			    TexCreate_ShaderResource | TexCreate_RenderTargetable),
			TEXT("ToneMapDurand.LogLum"));

		{
			auto* P1 = GraphBuilder.AllocParameters<FToneMapDurandLogLumPS::FParameters>();
			P1->View = ViewInfo.ViewUniformBuffer;
			P1->SceneColorTexture = SceneColor.Texture;
			P1->SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			P1->SvPositionToSceneColorUV = DurandSceneColorUV;
			P1->OneOverPreExposure = 1.0f / FMath::Max(ViewInfo.PreExposure, 0.001f);
			P1->RenderTargets[0] = FRenderTargetBinding(LogLumTex, ERenderTargetLoadAction::ENoAction);
			TShaderMapRef<FToneMapDurandLogLumPS> Shader1(ViewInfo.ShaderMap);
			FPixelShaderUtils::AddFullscreenPass(GraphBuilder, ViewInfo.ShaderMap,
				RDG_EVENT_NAME("DurandLogLum"), Shader1, P1, FIntRect(0, 0, WS.X, WS.Y));
		}

		// --- Pass 2a/2b: cross-bilateral filter (horizontal then vertical) ---
		FRDGTextureRef BasePing = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(WS, PF_R32_FLOAT, FClearValueBinding::None,
			    TexCreate_ShaderResource | TexCreate_RenderTargetable),
			TEXT("ToneMapDurand.BasePing"));
		FRDGTextureRef BasePong = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(WS, PF_R32_FLOAT, FClearValueBinding::None,
			    TexCreate_ShaderResource | TexCreate_RenderTargetable),
			TEXT("ToneMapDurand.BasePong"));

		auto RunDurandBilateral = [&](FRDGTextureRef InLogLum, FRDGTextureRef GuideLogLum,
		                              FRDGTextureRef OutTex, FVector2f Dir, const TCHAR* EventName)
		{
			auto* P2 = GraphBuilder.AllocParameters<FToneMapDurandBilateralPS::FParameters>();
			P2->View = ViewInfo.ViewUniformBuffer;
			P2->LogLumTexture = InLogLum;
			P2->LogLumSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			P2->GuideTexture  = GuideLogLum;
			P2->GuideSampler  = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			P2->BufferSizeAndInvSize = BilateralBufferSize;
			P2->BlurDirection        = Dir;
			P2->SpatialSigma         = ActiveComp->DurandSpatialSigma;
			P2->RangeSigma           = ActiveComp->DurandRangeSigma;
			P2->RenderTargets[0]     = FRenderTargetBinding(OutTex, ERenderTargetLoadAction::ENoAction);
			TShaderMapRef<FToneMapDurandBilateralPS> Shader2(ViewInfo.ShaderMap);
			FPixelShaderUtils::AddFullscreenPass(GraphBuilder, ViewInfo.ShaderMap,
				FRDGEventName(EventName), Shader2, P2, FIntRect(0, 0, WS.X, WS.Y));
		};

		RunDurandBilateral(LogLumTex, LogLumTex, BasePing, FVector2f(1.0f, 0.0f), TEXT("DurandBilateralH"));
		RunDurandBilateral(BasePing,  LogLumTex, BasePong, FVector2f(0.0f, 1.0f), TEXT("DurandBilateralV"));

		// --- Pass 3: reconstruct ---
		FRDGTextureRef DurandResult = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(WS, PF_FloatRGBA, FClearValueBinding::None,
			    TexCreate_ShaderResource | TexCreate_RenderTargetable),
			TEXT("ToneMapDurand.Result"));

		{
			auto* P3 = GraphBuilder.AllocParameters<FToneMapDurandReconstructPS::FParameters>();
			P3->View = ViewInfo.ViewUniformBuffer;
			P3->SceneColorTexture = SceneColor.Texture;
			P3->SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			P3->LogLumTexture    = LogLumTex;
			P3->LogLumSampler    = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			P3->BaseLayerTexture = BasePong;
			P3->BaseLayerSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			P3->SvPositionToSceneColorUV = DurandSceneColorUV;
			P3->BufferSizeAndInvSize   = BilateralBufferSize;
			P3->OneOverPreExposure = 1.0f / FMath::Max(ViewInfo.PreExposure, 0.001f);
			P3->BaseCompression    = ActiveComp->DurandBaseCompression;
			P3->DetailBoost        = ActiveComp->DurandDetailBoost;
			P3->RenderTargets[0]   = FRenderTargetBinding(DurandResult, ERenderTargetLoadAction::ENoAction);
			TShaderMapRef<FToneMapDurandReconstructPS> Shader3(ViewInfo.ShaderMap);
			FPixelShaderUtils::AddFullscreenPass(GraphBuilder, ViewInfo.ShaderMap,
				RDG_EVENT_NAME("DurandReconstruct"), Shader3, P3, FIntRect(0, 0, WS.X, WS.Y));
		}

		PreToneMappedTexture = DurandResult;
		bPreToneMapped = true;
	}
	// =====================================================================
	// Fattal et al. 2002 Gradient-Domain Tone Mapping — pre-pass
	//
	// All passes run at full viewport resolution.  Seeding Jacobi with
	// log(lum) ensures partial convergence produces a valid compression ratio:
	//   ratio = exp(I_final - logLumIn)  →  < 1 on contrast edges (attenuated)
	//                                       ≈ 1 in smooth areas (preserved)
	// =====================================================================
	else if (bIsReplaceTonemap && ActiveComp->FilmCurve == EToneMapFilmCurve::Fattal)
	{
		RDG_EVENT_SCOPE(GraphBuilder, "ToneMapFX_Fattal");

		const FIntPoint WS = ViewportSize;
		const FVector4f FattalBufferSize((float)WS.X, (float)WS.Y, 1.0f / WS.X, 1.0f / WS.Y);
		const float     FattalOneOverPreExposure = 1.0f / FMath::Max(ViewInfo.PreExposure, 0.001f);

		const FScreenPassTextureViewport FattalWorkVP(WS, FIntRect(0, 0, WS.X, WS.Y));
		const FScreenPassTextureViewport SceneColorInputVP_F(
			FIntPoint(SceneColor.Texture->Desc.Extent.X, SceneColor.Texture->Desc.Extent.Y),
			SceneColorViewport.Rect);
		const FScreenTransform FattalSceneColorUV = (
			FScreenTransform::ChangeTextureBasisFromTo(FattalWorkVP, FScreenTransform::ETextureBasis::TexelPosition, FScreenTransform::ETextureBasis::ViewportUV) *
			FScreenTransform::ChangeTextureBasisFromTo(SceneColorInputVP_F, FScreenTransform::ETextureBasis::ViewportUV, FScreenTransform::ETextureBasis::TextureUV));

		// --- Pass 0: log-luminance (Jacobi seed) ---
		FRDGTextureRef LogLumTex = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(WS, PF_R32_FLOAT, FClearValueBinding::None,
			    TexCreate_ShaderResource | TexCreate_RenderTargetable),
			TEXT("ToneMapFattal.LogLum"));
		{
			auto* Pl = GraphBuilder.AllocParameters<FToneMapFattalLogLumPS::FParameters>();
			Pl->View = ViewInfo.ViewUniformBuffer;
			Pl->SceneColorTexture = SceneColor.Texture;
			Pl->SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			Pl->SvPositionToSceneColorUV = FattalSceneColorUV;
			Pl->OneOverPreExposure = FattalOneOverPreExposure;
			Pl->RenderTargets[0] = FRenderTargetBinding(LogLumTex, ERenderTargetLoadAction::ENoAction);
			TShaderMapRef<FToneMapFattalLogLumPS> ShaderL(ViewInfo.ShaderMap);
			FPixelShaderUtils::AddFullscreenPass(GraphBuilder, ViewInfo.ShaderMap,
				RDG_EVENT_NAME("FattalLogLum"), ShaderL, Pl, FIntRect(0, 0, WS.X, WS.Y));
		}

		// --- Pass 1: attenuated gradient field (Hx, Hy) ---
		FRDGTextureRef GradientTex = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(WS, PF_G32R32F, FClearValueBinding::None,
			    TexCreate_ShaderResource | TexCreate_RenderTargetable),
			TEXT("ToneMapFattal.Gradient"));
		{
			auto* Pg = GraphBuilder.AllocParameters<FToneMapFattalGradientPS::FParameters>();
			Pg->View = ViewInfo.ViewUniformBuffer;
			Pg->SceneColorTexture = SceneColor.Texture;
			Pg->SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			Pg->SvPositionToSceneColorUV = FattalSceneColorUV;
			Pg->BufferSizeAndInvSize = FattalBufferSize;
			Pg->OneOverPreExposure   = FattalOneOverPreExposure;
			Pg->Alpha      = ActiveComp->FattalAlpha;
			Pg->Beta       = ActiveComp->FattalBeta;
			Pg->NoiseFloor = ActiveComp->FattalNoise;
			Pg->RenderTargets[0] = FRenderTargetBinding(GradientTex, ERenderTargetLoadAction::ENoAction);
			TShaderMapRef<FToneMapFattalGradientPS> ShaderG(ViewInfo.ShaderMap);
			FPixelShaderUtils::AddFullscreenPass(GraphBuilder, ViewInfo.ShaderMap,
				RDG_EVENT_NAME("FattalGradient"), ShaderG, Pg, FIntRect(0, 0, WS.X, WS.Y));
		}

		// --- Pass 2: divergence div(H) ---
		FRDGTextureRef DivHTex = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(WS, PF_R32_FLOAT, FClearValueBinding::None,
			    TexCreate_ShaderResource | TexCreate_RenderTargetable),
			TEXT("ToneMapFattal.DivH"));
		{
			auto* Pd = GraphBuilder.AllocParameters<FToneMapFattalDivergencePS::FParameters>();
			Pd->View = ViewInfo.ViewUniformBuffer;
			Pd->GradientTexture = GradientTex;
			Pd->GradientSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			Pd->BufferSizeAndInvSize = FattalBufferSize;
			Pd->RenderTargets[0] = FRenderTargetBinding(DivHTex, ERenderTargetLoadAction::ENoAction);
			TShaderMapRef<FToneMapFattalDivergencePS> ShaderD(ViewInfo.ShaderMap);
			FPixelShaderUtils::AddFullscreenPass(GraphBuilder, ViewInfo.ShaderMap,
				RDG_EVENT_NAME("FattalDivergence"), ShaderD, Pd, FIntRect(0, 0, WS.X, WS.Y));
		}

		// --- Pass 3: Jacobi Poisson solver (seeded with log-lum) ---
		FRDGTextureRef JPing = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(WS, PF_R32_FLOAT, FClearValueBinding::None,
			    TexCreate_ShaderResource | TexCreate_RenderTargetable),
			TEXT("ToneMapFattal.JPing"));
		FRDGTextureRef JPong = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(WS, PF_R32_FLOAT, FClearValueBinding::None,
			    TexCreate_ShaderResource | TexCreate_RenderTargetable),
			TEXT("ToneMapFattal.JPong"));

		FRDGTextureRef JCurrent = LogLumTex; // seed: logLum gives useful partial-convergence
		const int32 FattalIters = FMath::Clamp(ActiveComp->FattalJacobiIterations, 1, 200);
		for (int32 It = 0; It < FattalIters; ++It)
		{
			FRDGTextureRef JOut = (It % 2 == 0) ? JPing : JPong;
			auto* Pj = GraphBuilder.AllocParameters<FToneMapFattalJacobiPS::FParameters>();
			Pj->View = ViewInfo.ViewUniformBuffer;
			Pj->CurrentITexture = JCurrent;
			Pj->CurrentISampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			Pj->DivHTexture     = DivHTex;
			Pj->DivHSampler     = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			Pj->BufferSizeAndInvSize = FattalBufferSize;
			Pj->RenderTargets[0] = FRenderTargetBinding(JOut, ERenderTargetLoadAction::ENoAction);
			TShaderMapRef<FToneMapFattalJacobiPS> ShaderJ(ViewInfo.ShaderMap);
			FPixelShaderUtils::AddFullscreenPass(GraphBuilder, ViewInfo.ShaderMap,
				RDG_EVENT_NAME("FattalJacobi"), ShaderJ, Pj, FIntRect(0, 0, WS.X, WS.Y));
			JCurrent = JOut;
		}

		// --- Pass 4: reconstruct ---
		FRDGTextureRef FattalResult = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(WS, PF_FloatRGBA, FClearValueBinding::None,
			    TexCreate_ShaderResource | TexCreate_RenderTargetable),
			TEXT("ToneMapFattal.Result"));
		{
			auto* Pr = GraphBuilder.AllocParameters<FToneMapFattalReconstructPS::FParameters>();
			Pr->View = ViewInfo.ViewUniformBuffer;
			Pr->SceneColorTexture = SceneColor.Texture;
			Pr->SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			Pr->SolvedITexture    = JCurrent;
			Pr->SolvedISampler    = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			Pr->SvPositionToSceneColorUV = FattalSceneColorUV;
			Pr->BufferSizeAndInvSize     = FattalBufferSize;
			Pr->OneOverPreExposure = FattalOneOverPreExposure;
			Pr->OutputSaturation   = ActiveComp->FattalSaturation;
			Pr->RenderTargets[0]   = FRenderTargetBinding(FattalResult, ERenderTargetLoadAction::ENoAction);
			TShaderMapRef<FToneMapFattalReconstructPS> ShaderR(ViewInfo.ShaderMap);
			FPixelShaderUtils::AddFullscreenPass(GraphBuilder, ViewInfo.ShaderMap,
				RDG_EVENT_NAME("FattalReconstruct"), ShaderR, Pr, FIntRect(0, 0, WS.X, WS.Y));
		}

		PreToneMappedTexture = FattalResult;
		bPreToneMapped = true;
	}

	// =====================================================================
	// Lens Effects — Ciliary Corona and Lenticular Halo
	// Runs after bloom composite; composites the effects onto current SceneColor.
	// =====================================================================
	{
		const bool bRunLensEffects = ActiveComp->bEnableCiliaryCorona || ActiveComp->bEnableLenticularHalo;
		if (bRunLensEffects)
		{
			RDG_EVENT_SCOPE(GraphBuilder, "ToneMapFX_LensEffects");

			const FIntPoint WS = ViewportSize;
			const FVector4f LensBufferSize((float)WS.X, (float)WS.Y, 1.0f / WS.X, 1.0f / WS.Y);

			const FScreenPassTextureViewport LensWorkVP(WS, FIntRect(0, 0, WS.X, WS.Y));
			const FScreenPassTextureViewport SceneColorInputVP_L(
				FIntPoint(SceneColor.Texture->Desc.Extent.X, SceneColor.Texture->Desc.Extent.Y),
				SceneColorViewport.Rect);
			const FScreenTransform LensSceneColorUV = (
				FScreenTransform::ChangeTextureBasisFromTo(LensWorkVP, FScreenTransform::ETextureBasis::TexelPosition, FScreenTransform::ETextureBasis::ViewportUV) *
				FScreenTransform::ChangeTextureBasisFromTo(SceneColorInputVP_L, FScreenTransform::ETextureBasis::ViewportUV, FScreenTransform::ETextureBasis::TextureUV));
			const FScreenTransform LensBrightPassUV = (
				FScreenTransform::ChangeTextureBasisFromTo(LensWorkVP, FScreenTransform::ETextureBasis::TexelPosition, FScreenTransform::ETextureBasis::ViewportUV) *
				FScreenTransform::ChangeTextureBasisFromTo(LensWorkVP, FScreenTransform::ETextureBasis::ViewportUV, FScreenTransform::ETextureBasis::TextureUV));

			// Use the lower of the two thresholds for the shared bright-pass
			const float BrightPassThreshold = (ActiveComp->bEnableCiliaryCorona && ActiveComp->bEnableLenticularHalo)
				? FMath::Min(ActiveComp->CoronaThreshold, ActiveComp->HaloThreshold)
				: (ActiveComp->bEnableCiliaryCorona ? ActiveComp->CoronaThreshold : ActiveComp->HaloThreshold);

			FRDGTextureRef BrightPassTex = GraphBuilder.CreateTexture(
				FRDGTextureDesc::Create2D(WS, PF_FloatRGBA, FClearValueBinding::None,
				    TexCreate_ShaderResource | TexCreate_RenderTargetable),
				TEXT("ToneMapLens.BrightPass"));

			{
				auto* Pb = GraphBuilder.AllocParameters<FToneMapLensBrightPassPS::FParameters>();
				Pb->View = ViewInfo.ViewUniformBuffer;
				Pb->SceneColorTexture = SceneColor.Texture;
				Pb->SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
				Pb->SvPositionToSceneColorUV = LensSceneColorUV;
				Pb->Threshold = BrightPassThreshold;
				Pb->RenderTargets[0] = FRenderTargetBinding(BrightPassTex, ERenderTargetLoadAction::ENoAction);
				TShaderMapRef<FToneMapLensBrightPassPS> ShaderBP(ViewInfo.ShaderMap);
				FPixelShaderUtils::AddFullscreenPass(GraphBuilder, ViewInfo.ShaderMap,
					RDG_EVENT_NAME("LensBrightPass"), ShaderBP, Pb, FIntRect(0, 0, WS.X, WS.Y));
			}

			FRDGTextureRef LensCoronaTex = SceneColor.Texture; // fallback
			FRDGTextureRef LensHaloTex   = SceneColor.Texture; // fallback

			// Corona streaks
			if (ActiveComp->bEnableCiliaryCorona)
			{
				FRDGTextureRef CoronaOut = GraphBuilder.CreateTexture(
					FRDGTextureDesc::Create2D(WS, PF_FloatRGBA, FClearValueBinding::None,
					    TexCreate_ShaderResource | TexCreate_RenderTargetable),
					TEXT("ToneMapLens.Corona"));

				auto* Pc = GraphBuilder.AllocParameters<FToneMapCoronaStreakPS::FParameters>();
				Pc->View = ViewInfo.ViewUniformBuffer;
				Pc->BrightPassTexture = BrightPassTex;
				Pc->BrightPassSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
				Pc->SvPositionToBrightPassUV = LensBrightPassUV;
				Pc->BufferSizeAndInvSize = LensBufferSize;
				Pc->SpikeCount           = ActiveComp->CoronaSpikeCount;
				Pc->SpikeLength          = ActiveComp->CoronaSpikeLength;
				Pc->CoronaIntensity      = ActiveComp->CoronaIntensity;
				Pc->RenderTargets[0] = FRenderTargetBinding(CoronaOut, ERenderTargetLoadAction::ENoAction);
				TShaderMapRef<FToneMapCoronaStreakPS> ShaderC(ViewInfo.ShaderMap);
				FPixelShaderUtils::AddFullscreenPass(GraphBuilder, ViewInfo.ShaderMap,
					RDG_EVENT_NAME("CoronaStreaks"), ShaderC, Pc, FIntRect(0, 0, WS.X, WS.Y));

				LensCoronaTex = CoronaOut;
			}

			// Lenticular halo ring
			if (ActiveComp->bEnableLenticularHalo)
			{
				FRDGTextureRef HaloOut = GraphBuilder.CreateTexture(
					FRDGTextureDesc::Create2D(WS, PF_FloatRGBA, FClearValueBinding::None,
					    TexCreate_ShaderResource | TexCreate_RenderTargetable),
					TEXT("ToneMapLens.Halo"));

				auto* Ph = GraphBuilder.AllocParameters<FToneMapHaloRingPS::FParameters>();
				Ph->View = ViewInfo.ViewUniformBuffer;
				Ph->BrightPassTexture = BrightPassTex;
				Ph->BrightPassSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
				Ph->SvPositionToBrightPassUV = LensBrightPassUV;
				Ph->BufferSizeAndInvSize = LensBufferSize;
				Ph->HaloRadius    = ActiveComp->HaloRadius;
				Ph->HaloThickness = ActiveComp->HaloThickness;
				Ph->HaloIntensity = ActiveComp->HaloIntensity;
				Ph->HaloTint      = FVector3f(ActiveComp->HaloTint.R, ActiveComp->HaloTint.G, ActiveComp->HaloTint.B);
				Ph->RenderTargets[0] = FRenderTargetBinding(HaloOut, ERenderTargetLoadAction::ENoAction);
				TShaderMapRef<FToneMapHaloRingPS> ShaderH(ViewInfo.ShaderMap);
				FPixelShaderUtils::AddFullscreenPass(GraphBuilder, ViewInfo.ShaderMap,
					RDG_EVENT_NAME("HaloRing"), ShaderH, Ph, FIntRect(0, 0, WS.X, WS.Y));

				LensHaloTex = HaloOut;
			}

			// Composite lens effects onto scene color
			FRDGTextureRef LensCompositeOut = GraphBuilder.CreateTexture(
				FRDGTextureDesc::Create2D(WS, SceneColor.Texture->Desc.Format, FClearValueBinding::None,
				    TexCreate_ShaderResource | TexCreate_RenderTargetable),
				TEXT("ToneMapLens.Composite"));

			{
				auto* Plc = GraphBuilder.AllocParameters<FToneMapLensCompositePS::FParameters>();
				Plc->View = ViewInfo.ViewUniformBuffer;
				Plc->SceneColorTexture = SceneColor.Texture;
				Plc->SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
				Plc->CoronaTexture     = LensCoronaTex;
				Plc->CoronaSampler     = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
				Plc->HaloTexture       = LensHaloTex;
				Plc->HaloSampler       = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
				Plc->SvPositionToSceneColorUV = LensSceneColorUV;
				Plc->SvPositionToLensUV       = LensBrightPassUV;
				Plc->bEnableCorona = ActiveComp->bEnableCiliaryCorona  ? 1.0f : 0.0f;
				Plc->bEnableHalo   = ActiveComp->bEnableLenticularHalo ? 1.0f : 0.0f;
				Plc->RenderTargets[0] = FRenderTargetBinding(LensCompositeOut, ERenderTargetLoadAction::ENoAction);
				TShaderMapRef<FToneMapLensCompositePS> ShaderLC(ViewInfo.ShaderMap);
				FPixelShaderUtils::AddFullscreenPass(GraphBuilder, ViewInfo.ShaderMap,
					RDG_EVENT_NAME("LensEffectsComposite"), ShaderLC, Plc, FIntRect(0, 0, WS.X, WS.Y));
			}

			// Replace SceneColor so downstream ToneMapProcess sees the lens-composited image
			SceneColor = FScreenPassTexture(LensCompositeOut, FIntRect(0, 0, WS.X, WS.Y));
		}
	}

	// =====================================================================
	// Determine output target
	// =====================================================================

	FScreenPassRenderTarget OutputTarget;
	if (bIsReplaceTonemap && Inputs.OverrideOutput.IsValid())
	{
		// ReplacingTonemapper: engine provides the final backbuffer as OverrideOutput
		OutputTarget = Inputs.OverrideOutput;
	}
	else
	{
		OutputTarget = FScreenPassRenderTarget(
			GraphBuilder.CreateTexture(
				FRDGTextureDesc::Create2D(
					ViewportSize, SceneColor.Texture->Desc.Format,
					FClearValueBinding::None,
					TexCreate_ShaderResource | TexCreate_RenderTargetable),
				TEXT("ToneMap.Output")),
			FIntRect(0, 0, ViewportSize.X, ViewportSize.Y),
			ERenderTargetLoadAction::ENoAction);
	}

	// =====================================================================
	// Post-pass chain: LUT → Vignette (each redirects through intermediates)
	// =====================================================================
	const bool bNeedLUT = ActiveComp->bEnableLUT
		&& ActiveComp->LUTTexture != nullptr
		&& ActiveComp->LUTTexture->GetResource() != nullptr
		&& ActiveComp->LUTTexture->GetResource()->TextureRHI != nullptr
		&& ActiveComp->LUTIntensity > 0.001f;

	const bool bNeedVignette = ActiveComp->bEnableVignette
		&& FMath::Abs(ActiveComp->VignetteIntensity) > 0.01f;

	// HDR output encoding as a final pass: requires ReplaceTonemap + HDR checkbox +
	// an HDR-capable display (OutputDevice >= 3 in EDisplayOutputFormat).
	const bool bWantHDREncode = bIsReplaceTonemap && ActiveComp->bHDROutput && bCachedHDROutput;
	bool bNeedHDREncode = false;
	uint32 HDROutputDevice = 0;
	float  HDRMaxDisplayNits = 80.0f;
	if (bWantHDREncode)
	{
		FTonemapperOutputDeviceParameters OutDevParams = GetTonemapperOutputDeviceParameters(*ViewInfo.Family);
		HDROutputDevice = OutDevParams.OutputDevice;
		HDRMaxDisplayNits = FMath::Max(OutDevParams.OutputMaxLuminance, 80.0f);
		// Only add the HDR encode pass when the display is actually HDR (device >= 3)
		bNeedHDREncode = (HDROutputDevice >= 3);
	}

	FScreenPassRenderTarget FinalOutputTarget = OutputTarget;

	// If any post-passes follow ToneMapProcess, redirect it to an intermediate
	if (bNeedLUT || bNeedVignette || bNeedHDREncode)
	{
		OutputTarget = FScreenPassRenderTarget(
			GraphBuilder.CreateTexture(
				FRDGTextureDesc::Create2D(
					ViewportSize, SceneColor.Texture->Desc.Format,
					FClearValueBinding::None,
					TexCreate_ShaderResource | TexCreate_RenderTargetable),
				TEXT("ToneMap.PrePostPasses")),
			FIntRect(0, 0, ViewportSize.X, ViewportSize.Y),
			ERenderTargetLoadAction::ENoAction);
	}

	// =====================================================================
	// Main Tone Map processing pass
	// =====================================================================

	{
		auto* P = GraphBuilder.AllocParameters<FToneMapProcessPS::FParameters>();
		P->View              = ViewInfo.ViewUniformBuffer;
		P->SceneColorTexture = SceneColor.Texture;
		P->SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		P->BlurredTexture    = BlurredTexture;
		P->BlurredSampler    = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

		// Build FScreenTransform for proper SvPosition → texture UV mapping.
		// This correctly handles viewport offsets (e.g. OverrideOutput with non-zero
		// Min in ReplaceTonemap mode) that caused glitches on viewport resize.
		const FIntPoint OutputExtent = FIntPoint(OutputTarget.Texture->Desc.Extent.X, OutputTarget.Texture->Desc.Extent.Y);
		const FIntRect  OutputViewRect = OutputTarget.ViewRect;
		const FScreenPassTextureViewport OutputVP(OutputExtent, OutputViewRect);

		// SceneColor viewport (engine-provided, may have non-zero offset)
		const FScreenPassTextureViewport SceneColorInputVP(
			FIntPoint(SceneColor.Texture->Desc.Extent.X, SceneColor.Texture->Desc.Extent.Y),
			SceneColorViewport.Rect);

		// SvPosition → ViewportUV [0,1] → SceneColor TextureUV
		P->SvPositionToSceneColorUV = (
			FScreenTransform::ChangeTextureBasisFromTo(OutputVP, FScreenTransform::ETextureBasis::TexelPosition, FScreenTransform::ETextureBasis::ViewportUV) *
			FScreenTransform::ChangeTextureBasisFromTo(SceneColorInputVP, FScreenTransform::ETextureBasis::ViewportUV, FScreenTransform::ETextureBasis::TextureUV));

		// Blurred texture: created at ViewportSize with rect (0,0)->(W,H), no offset
		const FScreenPassTextureViewport BlurredVP(
			ViewportSize, FIntRect(0, 0, ViewportSize.X, ViewportSize.Y));

		// SvPosition → ViewportUV [0,1] → Blurred TextureUV
		P->SvPositionToBlurredUV = (
			FScreenTransform::ChangeTextureBasisFromTo(OutputVP, FScreenTransform::ETextureBasis::TexelPosition, FScreenTransform::ETextureBasis::ViewportUV) *
			FScreenTransform::ChangeTextureBasisFromTo(BlurredVP, FScreenTransform::ETextureBasis::ViewportUV, FScreenTransform::ETextureBasis::TextureUV));

		// Output viewport rect for split-screen comparison
		P->OutputViewportRect = FVector4f(
			(float)OutputViewRect.Min.X, (float)OutputViewRect.Min.Y,
			(float)OutputViewRect.Max.X, (float)OutputViewRect.Max.Y);

		// ---- Bloom texture (ReplaceTonemap mode) ----
		if (bIsReplaceTonemap && BloomInput.IsValid())
		{
			P->BloomTexture = BloomInput.Texture;
			P->BloomSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			const FScreenPassTextureViewport BloomVP(BloomInput);

			// SvPosition → ViewportUV [0,1] → Bloom TextureUV
			P->SvPositionToBloomUV = (
				FScreenTransform::ChangeTextureBasisFromTo(OutputVP, FScreenTransform::ETextureBasis::TexelPosition, FScreenTransform::ETextureBasis::ViewportUV) *
				FScreenTransform::ChangeTextureBasisFromTo(BloomVP, FScreenTransform::ETextureBasis::ViewportUV, FScreenTransform::ETextureBasis::TextureUV));
		}
		else
		{
			// Provide a valid fallback (scene color itself, won't be sampled when bReplaceTonemap==0)
			P->BloomTexture = SceneColor.Texture;
			P->BloomSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			P->SvPositionToBloomUV = P->SvPositionToSceneColorUV;
		}

		// ---- ReplaceTonemap mode flag & exposure ----
		P->bReplaceTonemap    = bIsReplaceTonemap ? 1.0f : 0.0f;
		P->OneOverPreExposure = 1.0f / FMath::Max(ViewInfo.PreExposure, 0.001f);
		P->GlobalExposure     = FMath::Max(View.GetLastEyeAdaptationExposure(), 0.001f);

		// ---- Auto-Exposure mode & Krawczyk adapted luminance ----
		P->AutoExposureMode = (float)static_cast<uint8>(ActiveComp->AutoExposureMode);
		if (bNeedKrawczyk && AdaptedLumTexture)
		{
			P->AdaptedLumTexture = AdaptedLumTexture;
		}
		else
		{
			// Provide a valid fallback (won't be sampled when mode != Krawczyk)
			P->AdaptedLumTexture = SceneColor.Texture;
		}
		P->AdaptedLumSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		P->MinAutoExposure = ActiveComp->MinAutoExposure;
		P->MaxAutoExposure = ActiveComp->MaxAutoExposure;

		// ---- Film Curve mode & Hable params ----
		P->FilmCurveMode = (float)static_cast<uint8>(ActiveComp->FilmCurve);
		P->HableParams1 = FVector4f(
			ActiveComp->HableShoulderStrength,  // A
			ActiveComp->HableLinearStrength,    // B
			ActiveComp->HableLinearAngle,       // C
			ActiveComp->HableToeStrength);      // D
		P->HableParams2 = FVector4f(
			ActiveComp->HableToeNumerator,      // E
			ActiveComp->HableToeDenominator,    // F
			ActiveComp->HableWhitePoint,        // W
			0.0f);                              // unused
		P->ReinhardWhitePoint = ActiveComp->ReinhardWhitePoint;
		P->HDRSaturation  = ActiveComp->HDRSaturation;
		P->HDRColorBalance = FVector3f(
			ActiveComp->HDRColorBalance.R,
			ActiveComp->HDRColorBalance.G,
			ActiveComp->HDRColorBalance.B);

		// ---- AgX params ----
		P->AgXParams = FVector4f(
			ActiveComp->AgXMinEV,
			ActiveComp->AgXMaxEV,
			(float)static_cast<uint8>(ActiveComp->AgXLook),
			0.0f);

		// ---- Pre-tone-mapped texture (Durand / Fattal bypass) ----
		P->bPreToneMapped = bPreToneMapped ? 1.0f : 0.0f;
		if (bPreToneMapped && PreToneMappedTexture)
		{
			P->PreToneMappedTexture = PreToneMappedTexture;
			P->PreToneMappedSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

			// Pre-mapped texture lives in ViewportSize space with rect (0,0)→(W,H)
			const FScreenPassTextureViewport PreTMVP(
				FIntPoint(PreToneMappedTexture->Desc.Extent.X, PreToneMappedTexture->Desc.Extent.Y),
				FIntRect(0, 0, PreToneMappedTexture->Desc.Extent.X, PreToneMappedTexture->Desc.Extent.Y));
			P->SvPositionToPreToneMappedUV = (
				FScreenTransform::ChangeTextureBasisFromTo(OutputVP, FScreenTransform::ETextureBasis::TexelPosition, FScreenTransform::ETextureBasis::ViewportUV) *
				FScreenTransform::ChangeTextureBasisFromTo(PreTMVP, FScreenTransform::ETextureBasis::ViewportUV, FScreenTransform::ETextureBasis::TextureUV));
		}
		else
		{
			// Fallback: provide valid texture to prevent RDG null-binding assert (won't be sampled)
			P->PreToneMappedTexture = SceneColor.Texture;
			P->PreToneMappedSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			P->SvPositionToPreToneMappedUV = P->SvPositionToSceneColorUV;
		}

		// --- White Balance ---
		P->Temperature = ActiveComp->Temperature;
		P->Tint        = ActiveComp->Tint;

		// --- Exposure ---
		P->ExposureValue = ActiveComp->Exposure;

		float CameraEV = 0.0f;
		if (ActiveComp->bUseCameraExposure)
		{
			float N = FMath::Max(ActiveComp->Aperture, 1.0f);
			float t = FMath::Max(ActiveComp->ShutterSpeed, 0.00001f);
			float S = FMath::Max(ActiveComp->CameraISO, 1.0f);
			CameraEV = FMath::Log2(N * N / t) + FMath::Log2(100.0f / S);
			const float ReferenceEV = FMath::Log2(5.6f * 5.6f / 0.008f) + FMath::Log2(100.0f / 100.0f);
			CameraEV -= ReferenceEV;
		}
		P->CameraEV           = CameraEV;
		P->bUseCameraExposure = ActiveComp->bUseCameraExposure ? 1.0f : 0.0f;

		// --- Tone ---
		P->Contrast        = ActiveComp->Contrast;
		P->HighlightsValue = ActiveComp->Highlights;
		P->ShadowsValue    = ActiveComp->Shadows;
		P->WhitesValue     = ActiveComp->Whites;
		P->BlacksValue     = ActiveComp->Blacks;
		P->ToneSmoothingValue = ActiveComp->ToneSmoothing;
		P->ContrastMidpoint   = ActiveComp->ContrastMidpoint;

		// --- Presence ---
		P->ClarityStrength    = ActiveComp->Clarity;
		P->VibranceStrength   = ActiveComp->Vibrance;
		P->SaturationStrength = ActiveComp->Saturation;

		// --- Dynamic Contrast fine/coarse blur textures ---
		P->BlurredFineTexture   = BlurredFineTexture;
		P->BlurredFineSampler   = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		P->BlurredCoarseTexture = BlurredCoarseTexture;
		P->BlurredCoarseSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

		// Fine/coarse blur textures: same layout as Clarity blurred (ViewportSize, rect 0→W,H)
		P->SvPositionToBlurredFineUV = (
			FScreenTransform::ChangeTextureBasisFromTo(OutputVP, FScreenTransform::ETextureBasis::TexelPosition, FScreenTransform::ETextureBasis::ViewportUV) *
			FScreenTransform::ChangeTextureBasisFromTo(BlurredVP, FScreenTransform::ETextureBasis::ViewportUV, FScreenTransform::ETextureBasis::TextureUV));
		P->SvPositionToBlurredCoarseUV = (
			FScreenTransform::ChangeTextureBasisFromTo(OutputVP, FScreenTransform::ETextureBasis::TexelPosition, FScreenTransform::ETextureBasis::ViewportUV) *
			FScreenTransform::ChangeTextureBasisFromTo(BlurredVP, FScreenTransform::ETextureBasis::ViewportUV, FScreenTransform::ETextureBasis::TextureUV));

		// --- Dynamic Contrast strengths ---
		P->DynamicContrastStrength    = ActiveComp->DynamicContrast;
		P->CorrectContrastStrength    = ActiveComp->CorrectContrast;
		P->CorrectColorCastStrength   = ActiveComp->CorrectColorCast;

		// --- Tone Curve ---
		P->ToneCurveParams = FVector4f(
			ActiveComp->CurveHighlights,
			ActiveComp->CurveLights,
			ActiveComp->CurveDarks,
			ActiveComp->CurveShadows);

		// --- HSL (packed float4) ---
		P->HueShift1 = FVector4f(ActiveComp->HueReds,   ActiveComp->HueOranges,   ActiveComp->HueYellows,   ActiveComp->HueGreens);
		P->HueShift2 = FVector4f(ActiveComp->HueAquas,  ActiveComp->HueBlues,     ActiveComp->HuePurples,   ActiveComp->HueMagentas);
		P->SatAdj1   = FVector4f(ActiveComp->SatReds,    ActiveComp->SatOranges,    ActiveComp->SatYellows,    ActiveComp->SatGreens);
		P->SatAdj2   = FVector4f(ActiveComp->SatAquas,   ActiveComp->SatBlues,      ActiveComp->SatPurples,    ActiveComp->SatMagentas);
		P->LumAdj1   = FVector4f(ActiveComp->LumReds,    ActiveComp->LumOranges,    ActiveComp->LumYellows,    ActiveComp->LumGreens);
		P->LumAdj2   = FVector4f(ActiveComp->LumAquas,   ActiveComp->LumBlues,      ActiveComp->LumPurples,    ActiveComp->LumMagentas);

		// --- HSL Smoothing ---
		P->HSLSmoothing = ActiveComp->HSLSmoothing;

		// --- Feature toggles ---
		P->bEnableHSL    = ActiveComp->IsAnyHSLActive()   ? 1.0f : 0.0f;
		P->bEnableCurves = ActiveComp->IsAnyCurveActive()  ? 1.0f : 0.0f;

		P->RenderTargets[0] = FRenderTargetBinding(OutputTarget.Texture, OutputTarget.LoadAction);

		TShaderMapRef<FToneMapProcessPS> ProcessShader(ViewInfo.ShaderMap);
		FPixelShaderUtils::AddFullscreenPass(
			GraphBuilder, ViewInfo.ShaderMap,
			RDG_EVENT_NAME("ToneMapProcess"),
			ProcessShader, P,
			OutputTarget.ViewRect);
	}

	// =====================================================================
	// LUT pass — runs after ToneMapProcess (post-tonemap, post-sRGB)
	// =====================================================================
	if (bNeedLUT)
	{
		// Determine LUT output target: another intermediate if vignette follows,
		// otherwise write directly to FinalOutputTarget
		FScreenPassRenderTarget LUTOutputTarget;
		if (bNeedVignette || bNeedHDREncode)
		{
			LUTOutputTarget = FScreenPassRenderTarget(
				GraphBuilder.CreateTexture(
					FRDGTextureDesc::Create2D(
						ViewportSize, SceneColor.Texture->Desc.Format,
						FClearValueBinding::None,
						TexCreate_ShaderResource | TexCreate_RenderTargetable),
					TEXT("ToneMap.PreVignette")),
				FIntRect(0, 0, ViewportSize.X, ViewportSize.Y),
				ERenderTargetLoadAction::ENoAction);
		}
		else
		{
			LUTOutputTarget = FinalOutputTarget;
		}

		auto* LP = GraphBuilder.AllocParameters<FToneMapLUTPS::FParameters>();
		LP->View              = ViewInfo.ViewUniformBuffer;
		LP->SceneColorTexture = OutputTarget.Texture;  // ToneMapProcess output
		LP->SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

		// UV transform: SvPosition in LUTOutputTarget → UV in ToneMapProcess intermediate
		const FIntPoint LUTOutExtent(LUTOutputTarget.Texture->Desc.Extent.X,
		                             LUTOutputTarget.Texture->Desc.Extent.Y);
		const FScreenPassTextureViewport LUTOutVP(LUTOutExtent, LUTOutputTarget.ViewRect);
		const FScreenPassTextureViewport PreLUTVP(
			ViewportSize, FIntRect(0, 0, ViewportSize.X, ViewportSize.Y));

		LP->SvPositionToSceneColorUV = (
			FScreenTransform::ChangeTextureBasisFromTo(LUTOutVP,
				FScreenTransform::ETextureBasis::TexelPosition,
				FScreenTransform::ETextureBasis::ViewportUV) *
			FScreenTransform::ChangeTextureBasisFromTo(PreLUTVP,
				FScreenTransform::ETextureBasis::ViewportUV,
				FScreenTransform::ETextureBasis::TextureUV));

		// LUT texture — detect dimensions to determine cube size
		FRHITexture* LUTRHI = ActiveComp->LUTTexture->GetResource()->TextureRHI;
		FRDGTextureRef LUTTex = GraphBuilder.RegisterExternalTexture(
			CreateRenderTarget(LUTRHI, TEXT("ToneMapLUTTex")));
		LP->LUTTexture = LUTTex;
		LP->LUTSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

		// LUT cube dimension = texture height (256×16→16, 1024×32→32, 4096×64→64)
		const float LUTSize = (float)LUTRHI->GetSizeXYZ().Y;
		LP->LUTSize      = LUTSize;
		LP->InvLUTSize    = 1.0f / FMath::Max(LUTSize, 1.0f);
		LP->LUTIntensity  = ActiveComp->LUTIntensity;

		LP->RenderTargets[0] = FRenderTargetBinding(LUTOutputTarget.Texture, LUTOutputTarget.LoadAction);

		TShaderMapRef<FToneMapLUTPS> LUTShader(ViewInfo.ShaderMap);
		FPixelShaderUtils::AddFullscreenPass(
			GraphBuilder, ViewInfo.ShaderMap,
			RDG_EVENT_NAME("ToneMapLUT"),
			LUTShader, LP,
			LUTOutputTarget.ViewRect);

		// Update OutputTarget so the next pass (vignette) reads from LUT output
		OutputTarget = LUTOutputTarget;
	}

	// =====================================================================
	// Vignette pass — runs after LUT (or ToneMapProcess if no LUT)
	// =====================================================================
	if (bNeedVignette)
	{
		// Determine Vignette output: another intermediate if HDR encode follows,
		// otherwise write directly to FinalOutputTarget.
		FScreenPassRenderTarget VignetteOutputTarget;
		if (bNeedHDREncode)
		{
			VignetteOutputTarget = FScreenPassRenderTarget(
				GraphBuilder.CreateTexture(
					FRDGTextureDesc::Create2D(
						ViewportSize, SceneColor.Texture->Desc.Format,
						FClearValueBinding::None,
						TexCreate_ShaderResource | TexCreate_RenderTargetable),
					TEXT("ToneMap.PreHDREncode")),
				FIntRect(0, 0, ViewportSize.X, ViewportSize.Y),
				ERenderTargetLoadAction::ENoAction);
		}
		else
		{
			VignetteOutputTarget = FinalOutputTarget;
		}

		auto* VP = GraphBuilder.AllocParameters<FToneMapVignettePS::FParameters>();
		VP->View              = ViewInfo.ViewUniformBuffer;
		VP->SceneColorTexture = OutputTarget.Texture;  // LUT output (or ToneMapProcess if no LUT)
		VP->SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

		// UV transform: SvPosition in VignetteOutputTarget → UV in previous pass intermediate
		const FIntPoint VigOutExtent(VignetteOutputTarget.Texture->Desc.Extent.X,
		                               VignetteOutputTarget.Texture->Desc.Extent.Y);
		const FScreenPassTextureViewport VigOutVP(VigOutExtent, VignetteOutputTarget.ViewRect);
		const FScreenPassTextureViewport PreVignetteVP(
			ViewportSize, FIntRect(0, 0, ViewportSize.X, ViewportSize.Y));

		VP->SvPositionToSceneColorUV = (
			FScreenTransform::ChangeTextureBasisFromTo(VigOutVP,
				FScreenTransform::ETextureBasis::TexelPosition,
				FScreenTransform::ETextureBasis::ViewportUV) *
			FScreenTransform::ChangeTextureBasisFromTo(PreVignetteVP,
				FScreenTransform::ETextureBasis::ViewportUV,
				FScreenTransform::ETextureBasis::TextureUV));

		// Vignette parameters: Mode, Size, Intensity
		VP->VignetteParams = FVector4f(
			(float)static_cast<uint8>(ActiveComp->VignetteMode),
			ActiveComp->VignetteSize,
			ActiveComp->VignetteIntensity,
			(float)static_cast<uint8>(ActiveComp->VignetteFalloff));
		VP->FalloffExponent = ActiveComp->VignetteFalloffExponent;

		// Alpha texture (optional)
		const bool bHasAlphaTex = ActiveComp->bVignetteUseAlphaTexture
			&& ActiveComp->VignetteAlphaTexture != nullptr
			&& ActiveComp->VignetteAlphaTexture->GetResource() != nullptr
			&& ActiveComp->VignetteAlphaTexture->GetResource()->TextureRHI != nullptr;

		VP->bUseAlphaTexture  = bHasAlphaTex ? 1.0f : 0.0f;
		VP->bAlphaTextureOnly = (bHasAlphaTex && ActiveComp->bVignetteAlphaTextureOnly) ? 1.0f : 0.0f;
		VP->TextureChannelIndex = (float)static_cast<uint8>(ActiveComp->VignetteTextureChannel);

		if (bHasAlphaTex)
		{
			FRHITexture* AlphaRHI = ActiveComp->VignetteAlphaTexture->GetResource()->TextureRHI;
			VP->AlphaTexture = GraphBuilder.RegisterExternalTexture(
				CreateRenderTarget(AlphaRHI, TEXT("VignetteAlphaTex")));
		}
		else
		{
			// Safe fallback — won't be sampled when bUseAlphaTexture == 0
			VP->AlphaTexture = OutputTarget.Texture;
		}
		VP->AlphaSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

		VP->RenderTargets[0] = FRenderTargetBinding(VignetteOutputTarget.Texture, VignetteOutputTarget.LoadAction);

		TShaderMapRef<FToneMapVignettePS> VignetteShader(ViewInfo.ShaderMap);
		FPixelShaderUtils::AddFullscreenPass(
			GraphBuilder, ViewInfo.ShaderMap,
			RDG_EVENT_NAME("ToneMapVignette"),
			VignetteShader, VP,
			VignetteOutputTarget.ViewRect);

		// Update OutputTarget so the HDR encode pass (if any) reads from Vignette output
		OutputTarget = VignetteOutputTarget;
	}

	// =====================================================================
	// HDR Output Encoding — final pass (ST2084/PQ or scRGB)
	//
	// Converts sRGB-encoded output to the display's native HDR format.
	// Only runs when the display is actually in HDR mode (OutputDevice >= 3).
	// =====================================================================
	if (bNeedHDREncode)
	{
		RDG_EVENT_SCOPE(GraphBuilder, "ToneMapFX_HDREncode");

		auto* HP = GraphBuilder.AllocParameters<FToneMapHDREncodePS::FParameters>();
		HP->View              = ViewInfo.ViewUniformBuffer;
		HP->SceneColorTexture = OutputTarget.Texture;
		HP->SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

		// UV transform: SvPosition in FinalOutputTarget → UV in previous pass
		const FIntPoint HDROutExtent(FinalOutputTarget.Texture->Desc.Extent.X,
		                              FinalOutputTarget.Texture->Desc.Extent.Y);
		const FScreenPassTextureViewport HDROutVP(HDROutExtent, FinalOutputTarget.ViewRect);
		const FScreenPassTextureViewport PreHDRVP(
			FIntPoint(OutputTarget.Texture->Desc.Extent.X, OutputTarget.Texture->Desc.Extent.Y),
			FIntRect(0, 0, OutputTarget.Texture->Desc.Extent.X, OutputTarget.Texture->Desc.Extent.Y));

		HP->SvPositionToSceneColorUV = (
			FScreenTransform::ChangeTextureBasisFromTo(HDROutVP,
				FScreenTransform::ETextureBasis::TexelPosition,
				FScreenTransform::ETextureBasis::ViewportUV) *
			FScreenTransform::ChangeTextureBasisFromTo(PreHDRVP,
				FScreenTransform::ETextureBasis::ViewportUV,
				FScreenTransform::ETextureBasis::TextureUV));

		HP->OutputDeviceType = (float)HDROutputDevice;
		HP->PaperWhiteNits   = ActiveComp->PaperWhiteNits;
		HP->MaxDisplayNits   = HDRMaxDisplayNits;

		HP->RenderTargets[0] = FRenderTargetBinding(FinalOutputTarget.Texture, FinalOutputTarget.LoadAction);

		TShaderMapRef<FToneMapHDREncodePS> HDRShader(ViewInfo.ShaderMap);
		FPixelShaderUtils::AddFullscreenPass(
			GraphBuilder, ViewInfo.ShaderMap,
			RDG_EVENT_NAME("HDREncode"),
			HDRShader, HP,
			FinalOutputTarget.ViewRect);
	}

	return FScreenPassTexture(FinalOutputTarget.Texture, FinalOutputTarget.ViewRect);
}

// =============================================================================
// UToneMapSubsystem
// =============================================================================

void UToneMapSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	SceneViewExtension = FSceneViewExtensions::NewExtension<FToneMapSceneViewExtension>(this);
}

void UToneMapSubsystem::Deinitialize()
{
	SceneViewExtension.Reset();
	Super::Deinitialize();
}

void UToneMapSubsystem::RegisterComponent(UToneMapComponent* Component)
{
	if (Component)
	{
		Components.AddUnique(Component);
	}
}

void UToneMapSubsystem::UnregisterComponent(UToneMapComponent* Component)
{
	if (Component)
	{
		Components.Remove(Component);
	}
}
