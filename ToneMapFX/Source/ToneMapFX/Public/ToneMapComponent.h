// Licensed under the zlib License. See LICENSE file in the project root.

#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Texture.h"
#include "ToneMapComponent.generated.h"

class UCurveFloat;
class UCurveBase;
class UTexture2D;

// ============================================================================
// Bloom enums
// ============================================================================

/** Blend mode for compositing bloom back onto the scene */
UENUM(BlueprintType)
enum class EBloomBlendMode : uint8
{
	/** Screen blend - Photographic glow effect */
	Screen UMETA(DisplayName = "Screen"),
	/** Overlay blend - High contrast glow */
	Overlay UMETA(DisplayName = "Overlay"),
	/** Soft light blend - Gentle, subtle glow */
	SoftLight UMETA(DisplayName = "Soft Light"),
	/** Hard light blend - Intense, punchy glow */
	HardLight UMETA(DisplayName = "Hard Light"),
	/** Lighten blend - Only brightens, never darkens */
	Lighten UMETA(DisplayName = "Lighten"),
	/** Multiply blend - Darkens scene with bloom */
	Multiply UMETA(DisplayName = "Multiply"),
	/** Additive blend - Pure light addition (classic game bloom) */
	Additive UMETA(DisplayName = "Additive (Classic)"),
	/** Soft light glow - soft-light character clamped so bloom never darkens the scene */
	SoftLightGlow UMETA(DisplayName = "Soft Light Glow (No Darken)")
};

/** Bloom effect mode */
UENUM(BlueprintType)
enum class EBloomMode : uint8
{
	/** Standard Gaussian blur bloom */
	Standard UMETA(DisplayName = "Standard (Gaussian)"),
	/** Directional glare - star/cross streaks from bright areas */
	DirectionalGlare UMETA(DisplayName = "Directional Glare (Streaks)"),
	/** Kawase bloom - Progressive pyramid blur */
	Kawase UMETA(DisplayName = "Kawase"),
	/** Soft Focus - Dreamy full-scene glow effect */
	SoftFocus UMETA(DisplayName = "Soft Focus (Dreamy Glow)"),
	/** Soft Focus Glow - full-scene soft focus with a non-darkening soft-light composite */
	SoftFocusGlow UMETA(DisplayName = "Soft Focus Glow (No Darken)")
};

/** Color treatment applied to the blurred bloom before intensity/composite. */
UENUM(BlueprintType)
enum class EBloomColorMode : uint8
{
	/** Preserve the source scene color in the bloom. */
	SceneColor UMETA(DisplayName = "Scene Color"),
	/** Convert bloom to grayscale luminance for the older monochrome bloom look. */
	LegacyLuminance UMETA(DisplayName = "Legacy Luminance"),
	/** Multiply the bloom by the Bloom Tint color. */
	Tint UMETA(DisplayName = "Tint")
};

// ============================================================================
// Tone Map enums
// ============================================================================

/** Post-process pass injection point (used in PostProcess mode only) */
UENUM(BlueprintType)
enum class EToneMapPostProcessPass : uint8
{
	Tonemap                UMETA(DisplayName = "After Tonemap (Recommended)"),
	MotionBlur             UMETA(DisplayName = "After Motion Blur (HDR, Pre-Tonemap)"),
	FXAA                   UMETA(DisplayName = "After FXAA"),
	VisualizeDepthOfField  UMETA(DisplayName = "After Everything (Latest Possible)")
};

/** Operating mode for ToneMapFX */
UENUM(BlueprintType)
enum class EToneMapMode : uint8
{
	PostProcess     UMETA(DisplayName = "Post-Process (After Tonemapper)",
		ToolTip = "Runs after UE tonemapper. LDR input. Safe, compatible."),
	ReplaceTonemap  UMETA(DisplayName = "Replace Tonemapper (HDR, Full Control)",
		ToolTip = "Replaces UE tonemapper entirely. HDR input. Disables ACES artifacts.")
};

/** HDR output transfer used when ReplaceTonemap is driving an HDR display. */
UENUM(BlueprintType)
enum class EToneMapHDROutputMode : uint8
{
	PaperWhiteNits UMETA(DisplayName = "Paper White Nits",
		ToolTip = "Encodes the existing SDR-style tone-mapped result for HDR output. 1.0 maps to Paper White Nits."),
	TrueHDR UMETA(DisplayName = "True HDR Output",
		ToolTip = "Preserves linear values above 1.0 through the final HDR encode. 1.0 maps to Paper White Nits; higher values use HDR headroom.")
};

/** Film curve / tonemapping operator used in ReplaceTonemap mode */
UENUM(BlueprintType)
enum class EToneMapFilmCurve : uint8
{
	Hable           UMETA(DisplayName = "Hable",
		ToolTip = "Hable filmic curve with configurable shoulder/linear/toe/whitepoint."),
	ReinhardLuminance UMETA(DisplayName = "Reinhard (Luminance)",
		ToolTip = "Reinhard applied to luminance only — preserves hue and saturation."),
	ReinhardJodie   UMETA(DisplayName = "Reinhard-Jodie",
		ToolTip = "Hybrid: blends per-channel Reinhard with luminance Reinhard. Subtle desaturation in brights."),
	ReinhardStandard UMETA(DisplayName = "Reinhard (Standard)",
		ToolTip = "Classic Reinhard applied per RGB channel. Simple, tends to desaturate."),
	Durand          UMETA(DisplayName = "Durand-Dorsey 2002 (Bilateral)",
		ToolTip = "Durand & Dorsey 2002 bilateral tone mapping. Compresses scene contrast while preserving local detail across luminance edges. Multi-pass: log-lum → bilateral base layer → base compression + detail restore."),
	Fattal          UMETA(DisplayName = "Fattal et al. 2002 (Gradient Domain)",
		ToolTip = "Fattal et al. 2002 gradient-domain tone mapping. Attenuates large luminance gradients while preserving fine detail. Multi-pass: gradient attenuation → divergence → iterative Poisson solve → reconstruct."),
	AgX             UMETA(DisplayName = "AgX (Sobotka)",
		ToolTip = "AgX display rendering transform by Troy Sobotka. Inset matrix → log2 encoding → sigmoid tone curve → outset matrix. Preserves hue and saturation through highlight compression with minimal color clipping."),
	ACESHill        UMETA(DisplayName = "ACES Hill Fit",
		ToolTip = "Stephen Hill ACES filmic approximation Filmic contrast with ACES-style color transforms."),
	ACESNarkowicz   UMETA(DisplayName = "ACES Narkowicz Fit",
		ToolTip = "Krzysztof Narkowicz ACES filmic approximation Fast high-contrast filmic curve."),
	CustomLuminanceCurve UMETA(DisplayName = "Custom Curve (Luminance)",
		ToolTip = "Samples a CurveFloat across the configured EV range and applies it to scene luminance while preserving color ratios."),
	GTUchimuraClassic UMETA(DisplayName = "GT / Uchimura (Classic)",
		ToolTip = "Hajime Uchimura's classic six-parameter GT Sport curve. Applied per RGB channel, as presented by Polyphony Digital in 2017/2018.")
};

/** Creative look applied after the AgX base rendering */
UENUM(BlueprintType)
enum class EAgXLook : uint8
{
	None    UMETA(DisplayName = "None (Base AgX)",
		ToolTip = "Pure AgX display rendering with no creative look applied."),
	Punchy  UMETA(DisplayName = "Punchy",
		ToolTip = "Increased contrast and saturation for a vivid, punchy look."),
	Golden  UMETA(DisplayName = "Golden",
		ToolTip = "Warm golden tint with gentle contrast — sunset/golden hour feel.")
};

/** Auto-exposure mode used in ReplaceTonemap mode.
 *  Krawczyk and None automatically disable UE's built-in exposure system
 *  (forces AEM_Manual with neutral EV) so that PreExposure is constant
 *  and only ToneMapFX controls exposure.  Engine Default leaves UE's
 *  eye-adaptation active and passes its exposure value through. */
UENUM(BlueprintType)
enum class EToneMapAutoExposure : uint8
{
	None            UMETA(DisplayName = "None (Manual Only)",
		ToolTip = "No automatic exposure — UE's built-in exposure is disabled. Only the manual Exposure slider applies."),
	EngineDefault   UMETA(DisplayName = "Engine Default (UE Eye Adaptation)",
		ToolTip = "Uses Unreal Engine's built-in eye adaptation (histogram or basic). UE's exposure system remains active."),
	Krawczyk        UMETA(DisplayName = "Krawczyk (Automatic Scene Key)",
		ToolTip = "(EXPERIMENTAL) Krawczyk et al. 2005: automatic scene key estimation from log-average luminance. UE's built-in exposure is disabled automatically.")
};

/** Local exposure source. Custom methods run before the selected global film curve. */
UENUM(BlueprintType)
enum class EToneMapLocalExposureMode : uint8
{
	Off UMETA(DisplayName = "Off",
		ToolTip = "Disable local exposure. Global/manual exposure remains available."),
	Unreal UMETA(DisplayName = "Unreal (Post Process Volume)",
		ToolTip = "Use the standard Post Process Volume local exposure settings, including Bilateral or Exposure Fusion."),
	Durand UMETA(DisplayName = "Durand (Prototype)",
		ToolTip = "Full-resolution edge-aware base/detail exposure correction before the selected film curve. Requires Replace Tonemapper and a film curve other than Durand or Fattal. Spatial prototype without temporal history.")
};

/** Processing path: how color grading operations are evaluated */
UENUM(BlueprintType)
enum class EToneMapProcessingPath : uint8
{
	PerPixel  UMETA(DisplayName = "Per-Pixel (Full Quality)",
		ToolTip = "Every color operation evaluated analytically per screen pixel. Maximum mathematical precision."),
	LUT       UMETA(DisplayName = "LUT (Performance)",
		ToolTip = "Non-spatial operations baked into a 32x32x32 3D LUT, sampled with one trilinear fetch per pixel. Trades ALU for texture bandwidth — same visual quality with lower GPU cost. Use Dither Quantization for anti-banding.")
};

/** How this component contributes to the resolved ToneMapFX settings for a view. */
UENUM(BlueprintType)
enum class EToneMapBlendMode : uint8
{
	/** Applies everywhere with the component's blend weight. Matches the original global behavior. */
	Global UMETA(DisplayName = "Global"),
	/** Applies only inside a local bounding box, fading out across Blend Falloff Distance. */
	BoundingBox UMETA(DisplayName = "Bounding Box")
};

/** Color space used when blending color settings across multiple Tone Map FX components. */
UENUM(BlueprintType)
enum class EToneMapColorBlendSpace : uint8
{
	/** Original component-wise interpolation in linear RGB. Preserves legacy project looks. */
	LegacyLinearRGB UMETA(DisplayName = "Legacy Linear RGB"),
	/** Perceptual interpolation that keeps transitions between different colors smoother. */
	Oklab UMETA(DisplayName = "Oklab (Perceptual)")
};

/** HSL range/adjustment algorithm used by the color grading shader. */
UENUM(BlueprintType)
enum class EToneMapHSLMode : uint8
{
	/** Current smoother hue-range weighting and soft lightness adjustment. */
	Smooth UMETA(DisplayName = "Smooth (Current)"),
	/** Legacy-style HSL weighting and direct lightness scaling. */
	Legacy UMETA(DisplayName = "Legacy")
};

/** Sharpening algorithm used by the Presence sharpening pass. */
UENUM(BlueprintType)
enum class EToneMapSharpenMethod : uint8
{
	/** Existing 9-tap unsharp mask. Preserves legacy project looks. */
	LegacyUnsharpMask UMETA(DisplayName = "Legacy Unsharp Mask"),
	/** AMD FidelityFX CAS-style contrast-adaptive sharpening. */
	AMDCAS UMETA(DisplayName = "AMD CAS")
};

/** Shape used by the lenticular halo scatter pass. */
UENUM(BlueprintType)
enum class EToneMapHaloPattern : uint8
{
	/** Annular halo with broken-up arc variation. */
	RingArcs UMETA(DisplayName = "Ring / Arcs"),
	/** Bilateral stretched line gathers through bright sources. */
	StretchedLines UMETA(DisplayName = "Stretched Lines")
};

// ============================================================================
// Vignette enums
// ============================================================================

/** Vignette shape mode */
UENUM(BlueprintType)
enum class EVignetteMode : uint8
{
	Circular UMETA(DisplayName = "Circular",
		ToolTip = "Radial vignette using Euclidean distance from screen center. Slightly elliptical on widescreen — mirrors real lens behaviour."),
	Square   UMETA(DisplayName = "Square",
		ToolTip = "Square vignette using Chebyshev distance (max of X/Y offset). Uniform falloff toward each screen edge.")
};

/** Falloff curve shape for the vignette gradient */
UENUM(BlueprintType)
enum class EVignetteFalloff : uint8
{
	Linear   UMETA(DisplayName = "Linear",
		ToolTip = "Simple linear ramp from clear zone to edge. No acceleration."),
	Smooth   UMETA(DisplayName = "Smooth (Smoothstep)",
		ToolTip = "Hermite S-curve (smoothstep). Gentle start and end with faster middle."),
	Soft     UMETA(DisplayName = "Soft",
		ToolTip = "Double-smoothstep (smootherstep). Very gradual, wide falloff — subtle, filmic."),
	Hard     UMETA(DisplayName = "Hard",
		ToolTip = "Square-root curve. Fast initial darkening, sharp visible boundary."),
	Custom   UMETA(DisplayName = "Custom (Power Curve)",
		ToolTip = "User-defined power exponent. <1 = hard edge, 1 = linear, >1 = soft/gradual.")
};

/** Which texture channel to read as the vignette mask value */
UENUM(BlueprintType)
enum class EVignetteTextureChannel : uint8
{
	Alpha UMETA(DisplayName = "Alpha (A)",
		ToolTip = "Read the alpha channel. Use with RGBA textures that store the mask in A."),
	Red   UMETA(DisplayName = "Red (R)",
		ToolTip = "Read the red channel. Use with single-channel / grayscale textures."),
	Green UMETA(DisplayName = "Green (G)",
		ToolTip = "Read the green channel."),
	Blue  UMETA(DisplayName = "Blue (B)",
		ToolTip = "Read the blue channel.")
};

/**
 * Primitive component that drives the Tone Map FX post-process effect.
 * Place on any actor to enable Photoshop Camera-Raw-style color grading.
 */
UCLASS(ClassGroup=(Rendering), meta=(BlueprintSpawnableComponent, DisplayName="Tone Map FX"))
class TONEMAPFX_API UToneMapComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UToneMapComponent();

	virtual void PostLoad() override;

	// =========================================================================
	// Master
	// =========================================================================

	/** Enable / disable the entire Tone Map effect */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map")
	bool bEnabled = true;

	// =========================================================================
	// Blending
	// =========================================================================

	/** Spatial blend mode for this component. Global preserves the original single-look behavior. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Blending")
	EToneMapBlendMode BlendMode = EToneMapBlendMode::Global;

	/** Priority used for non-blendable choices such as render mode, textures, and enum-only effects. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Blending")
	int32 BlendPriority = 0;

	/** Overall contribution multiplier for this component before spatial falloff. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Blending",
		meta=(ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float BlendWeight = 1.0f;

	/** Color space used only when blending color-valued actor parameters such as bloom or halo tint.
	    HSL smoothing is a separate per-pixel shader control. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Blending",
		meta=(DisplayName = "Actor Color Blend Space"))
	EToneMapColorBlendSpace ColorBlendSpace = EToneMapColorBlendSpace::LegacyLinearRGB;

	/** Local-space half extents of the box where the component has full strength. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Blending",
		meta=(EditCondition = "BlendMode == EToneMapBlendMode::BoundingBox",
		      ClampMin = "0.0", UIMin = "0.0"))
	FVector BlendBoxExtent = FVector(500.0f, 500.0f, 500.0f);

	/** Distance outside the box used to fade this component to zero. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Blending",
		meta=(EditCondition = "BlendMode == EToneMapBlendMode::BoundingBox",
		      ClampMin = "0.0", UIMin = "0.0"))
	float BlendFalloffDistance = 250.0f;

	/** Draw the bounding-box blend area as a wireframe box in the viewport. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Blending",
		meta=(EditCondition = "BlendMode == EToneMapBlendMode::BoundingBox"))
	bool bShowBlendBounds = false;

	/** Thickness of the viewport wireframe used by Show Blend Bounds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Blending",
		meta=(EditCondition = "BlendMode == EToneMapBlendMode::BoundingBox && bShowBlendBounds",
		      ClampMin = "0.0", UIMin = "0.0", UIMax = "20.0"))
	float BlendBoundsLineThickness = 2.0f;

	/** Enable HDR output when running in ReplaceTonemap mode on an HDR monitor.
	    When checked, the plugin outputs ST2084 (PQ) or scRGB instead of sRGB,
	    matching the display's expected HDR encoding. Has no effect in PostProcess
	    mode (UE's tonemapper already handles HDR encoding). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map",
		meta=(EditCondition = "Mode == EToneMapMode::ReplaceTonemap"))
	bool bHDROutput = false;

	/** Automatically enables HDR output only when the active RHI/display reports
	    HDR support and the engine allows HDR output. When unavailable, ToneMapFX
	    falls back to SDR output instead of forcing HDR encoding. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map",
		meta=(DisplayName = "Auto Detect HDR Display",
		      EditCondition = "bHDROutput && Mode == EToneMapMode::ReplaceTonemap"))
	bool bAutoDetectHDROutput = false;

	/** Logs HDR output decisions when they change. Useful for verifying whether
	    ToneMapFX requested HDR, whether the display/RHI allowed it, and whether
	    the final HDR encode pass is active. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map",
		meta=(DisplayName = "Debug HDR Logging",
		      EditCondition = "Mode == EToneMapMode::ReplaceTonemap"))
	bool bDebugHDRLogging = false;

	/** HDR output behavior. Paper White Nits preserves the old SDR-in-HDR path.
	    True HDR keeps tone-mapped linear values above 1.0 until PQ/scRGB encoding. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map",
		meta=(DisplayName = "HDR Output Mode",
		      EditCondition = "bHDROutput && Mode == EToneMapMode::ReplaceTonemap"))
	EToneMapHDROutputMode HDROutputMode = EToneMapHDROutputMode::PaperWhiteNits;

	/** Paper-white brightness in nits (cd/m2).  In Paper White mode this is the
	    peak white of the SDR-style result. In True HDR mode, linear value 1.0
	    maps to this brightness and values above 1.0 become HDR highlight headroom. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map",
		meta=(EditCondition = "bHDROutput && Mode == EToneMapMode::ReplaceTonemap",
		      ClampMin = "80.0", ClampMax = "500.0", UIMin = "80.0", UIMax = "500.0"))
	float PaperWhiteNits = 200.0f;

	// =========================================================================
	// White Balance
	// =========================================================================

	/** Enable white balance adjustments. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|White Balance")
	bool bEnableWhiteBalance = true;

	/** Color temperature shift. Negative = cooler (blue), Positive = warmer (amber). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|White Balance",
		meta=(ClampMin = "-1000.0", ClampMax = "1000.0", UIMin = "-1000.0", UIMax = "1000.0",
		      EditCondition = "bEnableWhiteBalance"))
	float Temperature = 0.0f;

	/** Tint shift. Negative = green, Positive = magenta. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|White Balance",
		meta=(ClampMin = "-1000.0", ClampMax = "1000.0", UIMin = "-1000.0", UIMax = "1000.0",
		      EditCondition = "bEnableWhiteBalance"))
	float Tint = 0.0f;

	// =========================================================================
	// Tone
	// =========================================================================

	/** Enable Highlights / Shadows / Whites / Blacks adjustments. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Tone")
	bool bEnableToneAdjustments = true;

	/** Exposure adjustment in photographic stops (EV). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Tone",
		meta=(ClampMin = "-20.0", ClampMax = "20.0", UIMin = "-20.0", UIMax = "20.0"))
	float Exposure = 0.0f;

	/** Overall contrast. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Tone",
		meta=(ClampMin = "-100.0", ClampMax = "100.0", UIMin = "-100.0", UIMax = "100.0"))
	float Contrast = 0.0f;

	/** Adjust highlight (bright) tonal range. Negative recovers, positive brightens. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Tone",
		meta=(ClampMin = "-100.0", ClampMax = "100.0", UIMin = "-100.0", UIMax = "100.0",
		      EditCondition = "bEnableToneAdjustments"))
	float Highlights = 0.0f;

	/** Adjust shadow (dark) tonal range. Positive lifts, negative crushes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Tone",
		meta=(ClampMin = "-100.0", ClampMax = "100.0", UIMin = "-100.0", UIMax = "100.0",
		      EditCondition = "bEnableToneAdjustments"))
	float Shadows = 0.0f;

	/** Adjust white-point clipping. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Tone",
		meta=(ClampMin = "-100.0", ClampMax = "100.0", UIMin = "-100.0", UIMax = "100.0",
		      EditCondition = "bEnableToneAdjustments"))
	float Whites = 0.0f;

	/** Adjust black-point clipping. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Tone",
		meta=(ClampMin = "-100.0", ClampMax = "100.0", UIMin = "-100.0", UIMax = "100.0",
		      EditCondition = "bEnableToneAdjustments"))
	float Blacks = 0.0f;

	/** Controls how smoothly Highlights/Shadows/Whites/Blacks masks blend.
	    Lower = sharper, more isolated tonal bands.
	    Higher = wider, smoother overlap between regions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Tone",
		meta=(ClampMin = "0.0", ClampMax = "100.0", UIMin = "0.0", UIMax = "100.0"))
	float ToneSmoothing = 100.0f;

	/** Contrast pivot point (linear luminance). Default 0.18 = photographic mid-grey.
	    Lower values push the pivot into shadows, higher into highlights. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Tone",
		meta=(ClampMin = "0.01", ClampMax = "0.5", UIMin = "0.01", UIMax = "0.5",
		      EditCondition = "Contrast != 0"))
	float ContrastMidpoint = 0.18f;

	// =========================================================================
	// Presence
	// =========================================================================

	/** Mid-tone local contrast (local sharpening / softening). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Presence",
		meta=(ClampMin = "-100.0", ClampMax = "100.0", UIMin = "-100.0", UIMax = "100.0"))
	float Clarity = 0.0f;

	/** Blur radius used for Clarity detection (pixels). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Presence",
		meta=(ClampMin = "1.0", ClampMax = "50.0", UIMin = "1.0", UIMax = "50.0",
			  EditCondition = "Clarity != 0"))
	float ClarityRadius = 8.0f;

	/** Selective saturation — boosts under-saturated colours more than saturated ones. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Presence",
		meta=(ClampMin = "-100.0", ClampMax = "100.0", UIMin = "-100.0", UIMax = "100.0"))
	float Vibrance = 0.0f;

	/** Global saturation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Presence",
		meta=(ClampMin = "-100.0", ClampMax = "100.0", UIMin = "-100.0", UIMax = "100.0"))
	float Saturation = 0.0f;

	/** Enable sharpening (unsharp mask). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Presence",
		meta=(DisplayName = "Enable Sharpening"))
	bool bEnableSharpening = false;

	/** Sharpening algorithm. Legacy preserves the old look; AMD CAS adapts to local contrast. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Presence",
		meta=(EditCondition = "bEnableSharpening"))
	EToneMapSharpenMethod SharpenMethod = EToneMapSharpenMethod::LegacyUnsharpMask;

	/** Sharpening strength. For AMD CAS this maps to its 0-1 sharpness control. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Presence",
		meta=(ClampMin = "0.0", ClampMax = "100.0", UIMin = "0.0", UIMax = "100.0",
			  EditCondition = "bEnableSharpening"))
	float SharpenAmount = 25.0f;

	/** Pixel radius for sharpening detection. Smaller = finer detail, larger = coarser edges. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Presence",
		meta=(ClampMin = "0.5", ClampMax = "5.0", UIMin = "0.5", UIMax = "5.0",
			  EditCondition = "bEnableSharpening && SharpenMethod == EToneMapSharpenMethod::LegacyUnsharpMask"))
	float SharpenRadius = 1.0f;

	// =========================================================================
	// Dynamic Contrast (multi-scale local contrast & color correction)
	// =========================================================================

	/** Multi-scale local contrast enhancement. Adjusts contrast within each area of
	    the image based on tonality — enhances definition and reveals shadow/highlight
	    detail without crushing darks or blowing whites. Works on midtones. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Dynamic Contrast",
		meta=(ClampMin = "0.0", ClampMax = "100.0", UIMin = "0.0", UIMax = "100.0"))
	float DynamicContrast = 0.0f;

	/** Smart contrast that adapts to the distribution of light and tone in the image.
	    Increases contrast while preserving detail better than a global S-curve. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Dynamic Contrast",
		meta=(ClampMin = "0.0", ClampMax = "100.0", UIMin = "0.0", UIMax = "100.0"))
	float CorrectContrast = 0.0f;

	/** Analyzes the image for color casts and neutralizes them. Uses a Gray World
	    assumption: the average scene color should be neutral grey. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Dynamic Contrast",
		meta=(ClampMin = "0.0", ClampMax = "100.0", UIMin = "0.0", UIMax = "100.0"))
	float CorrectColorCast = 0.0f;

	// =========================================================================
	// Tone Curve (Parametric)
	// =========================================================================

	/** Push / pull the upper highlights region of the tone curve. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Tone Curve",
		meta=(ClampMin = "-100.0", ClampMax = "100.0", UIMin = "-100.0", UIMax = "100.0"))
	float CurveHighlights = 0.0f;

	/** Push / pull the upper-mid (lights) region of the tone curve. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Tone Curve",
		meta=(ClampMin = "-100.0", ClampMax = "100.0", UIMin = "-100.0", UIMax = "100.0"))
	float CurveLights = 0.0f;

	/** Push / pull the lower-mid (darks) region of the tone curve. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Tone Curve",
		meta=(ClampMin = "-100.0", ClampMax = "100.0", UIMin = "-100.0", UIMax = "100.0"))
	float CurveDarks = 0.0f;

	/** Push / pull the deep-shadow region of the tone curve. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Tone Curve",
		meta=(ClampMin = "-100.0", ClampMax = "100.0", UIMin = "-100.0", UIMax = "100.0"))
	float CurveShadows = 0.0f;

	// =========================================================================
	// HSL — Range Controls
	// =========================================================================

	/** HSL algorithm. Smooth is the current softer hue blending; Legacy restores the older sharper range/lightness behavior. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|HSL")
	EToneMapHSLMode HSLMode = EToneMapHSLMode::Smooth;

	/** Controls how smoothly HSL adjustments blend between adjacent color ranges.
	    Lower = sharper cutoff (more isolated color selection).
	    Higher = smoother feathering (wider overlap between ranges). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|HSL",
		meta=(ClampMin = "0.0", ClampMax = "100.0", UIMin = "0.0", UIMax = "100.0"))
	float HSLSmoothing = 100.0f;

	// =========================================================================
	// HSL — Hue (per-color hue rotation, -100 to 100)
	// =========================================================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|HSL|Hue",
		meta=(ClampMin = "-100.0", ClampMax = "100.0", UIMin = "-100.0", UIMax = "100.0"))
	float HueReds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|HSL|Hue",
		meta=(ClampMin = "-100.0", ClampMax = "100.0", UIMin = "-100.0", UIMax = "100.0"))
	float HueOranges = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|HSL|Hue",
		meta=(ClampMin = "-100.0", ClampMax = "100.0", UIMin = "-100.0", UIMax = "100.0"))
	float HueYellows = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|HSL|Hue",
		meta=(ClampMin = "-100.0", ClampMax = "100.0", UIMin = "-100.0", UIMax = "100.0"))
	float HueGreens = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|HSL|Hue",
		meta=(ClampMin = "-100.0", ClampMax = "100.0", UIMin = "-100.0", UIMax = "100.0"))
	float HueAquas = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|HSL|Hue",
		meta=(ClampMin = "-100.0", ClampMax = "100.0", UIMin = "-100.0", UIMax = "100.0"))
	float HueBlues = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|HSL|Hue",
		meta=(ClampMin = "-100.0", ClampMax = "100.0", UIMin = "-100.0", UIMax = "100.0"))
	float HuePurples = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|HSL|Hue",
		meta=(ClampMin = "-100.0", ClampMax = "100.0", UIMin = "-100.0", UIMax = "100.0"))
	float HueMagentas = 0.0f;

	// =========================================================================
	// HSL — Saturation (per-color saturation, -100 to 100)
	// =========================================================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|HSL|Saturation",
		meta=(ClampMin = "-100.0", ClampMax = "100.0", UIMin = "-100.0", UIMax = "100.0"))
	float SatReds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|HSL|Saturation",
		meta=(ClampMin = "-100.0", ClampMax = "100.0", UIMin = "-100.0", UIMax = "100.0"))
	float SatOranges = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|HSL|Saturation",
		meta=(ClampMin = "-100.0", ClampMax = "100.0", UIMin = "-100.0", UIMax = "100.0"))
	float SatYellows = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|HSL|Saturation",
		meta=(ClampMin = "-100.0", ClampMax = "100.0", UIMin = "-100.0", UIMax = "100.0"))
	float SatGreens = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|HSL|Saturation",
		meta=(ClampMin = "-100.0", ClampMax = "100.0", UIMin = "-100.0", UIMax = "100.0"))
	float SatAquas = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|HSL|Saturation",
		meta=(ClampMin = "-100.0", ClampMax = "100.0", UIMin = "-100.0", UIMax = "100.0"))
	float SatBlues = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|HSL|Saturation",
		meta=(ClampMin = "-100.0", ClampMax = "100.0", UIMin = "-100.0", UIMax = "100.0"))
	float SatPurples = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|HSL|Saturation",
		meta=(ClampMin = "-100.0", ClampMax = "100.0", UIMin = "-100.0", UIMax = "100.0"))
	float SatMagentas = 0.0f;

	// =========================================================================
	// HSL — Luminance (per-color brightness, -100 to 100)
	// =========================================================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|HSL|Luminance",
		meta=(ClampMin = "-100.0", ClampMax = "100.0", UIMin = "-100.0", UIMax = "100.0"))
	float LumReds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|HSL|Luminance",
		meta=(ClampMin = "-100.0", ClampMax = "100.0", UIMin = "-100.0", UIMax = "100.0"))
	float LumOranges = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|HSL|Luminance",
		meta=(ClampMin = "-100.0", ClampMax = "100.0", UIMin = "-100.0", UIMax = "100.0"))
	float LumYellows = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|HSL|Luminance",
		meta=(ClampMin = "-100.0", ClampMax = "100.0", UIMin = "-100.0", UIMax = "100.0"))
	float LumGreens = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|HSL|Luminance",
		meta=(ClampMin = "-100.0", ClampMax = "100.0", UIMin = "-100.0", UIMax = "100.0"))
	float LumAquas = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|HSL|Luminance",
		meta=(ClampMin = "-100.0", ClampMax = "100.0", UIMin = "-100.0", UIMax = "100.0"))
	float LumBlues = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|HSL|Luminance",
		meta=(ClampMin = "-100.0", ClampMax = "100.0", UIMin = "-100.0", UIMax = "100.0"))
	float LumPurples = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|HSL|Luminance",
		meta=(ClampMin = "-100.0", ClampMax = "100.0", UIMin = "-100.0", UIMax = "100.0"))
	float LumMagentas = 0.0f;

	// =========================================================================
	// Camera Settings (Low Priority — physical camera exposure model)
	// =========================================================================

	/** Use physical camera parameters (ISO / Shutter / Aperture) to derive additional exposure. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Camera Settings")
	bool bUseCameraExposure = false;

	/** Camera sensor sensitivity (ISO). Standard stops: 100, 200, 400, 800, 1600, 3200, 6400. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Camera Settings",
		meta=(ClampMin = "50.0", ClampMax = "25600.0", UIMin = "50.0", UIMax = "25600.0",
			  EditCondition = "bUseCameraExposure"))
	float CameraISO = 100.0f;

	/** Shutter speed denominator (e.g. 125 = 1/125s, 250 = 1/250s).
	    Standard stops: 1 (1s), 2 (1/2s), 4, 8, 15, 30, 60, 125, 250, 500, 1000, 2000, 4000, 8000. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Camera Settings",
		meta=(DisplayName = "Shutter Speed (1/X)",
			  ClampMin = "1.0", ClampMax = "8000.0", UIMin = "1.0", UIMax = "8000.0",
			  EditCondition = "bUseCameraExposure"))
	float ShutterSpeedDenominator = 125.0f;

	/** Aperture f-stop number. Lower = wider aperture, more light.
	    Standard stops: f/1.4, 2, 2.8, 4, 5.6, 8, 11, 16, 22. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Camera Settings",
		meta=(ClampMin = "1.0", ClampMax = "32.0", UIMin = "1.0", UIMax = "22.0",
			  EditCondition = "bUseCameraExposure"))
	float Aperture = 5.6f;

	// =========================================================================
	// Advanced
	// =========================================================================

	/** Operating mode: PostProcess runs after UE tonemapper (safe, LDR);
	    ReplaceTonemap replaces UE's entire tonemapper with the selected film curve. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Advanced")
	EToneMapMode Mode = EToneMapMode::PostProcess;

	/** Processing path: Per-Pixel evaluates all color math analytically per pixel;
	    LUT bakes non-spatial operations into a 32^3 3D lookup table for lower GPU cost
	    with virtually identical visual output. Both paths produce the same result. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Advanced")
	EToneMapProcessingPath ProcessingPath = EToneMapProcessingPath::PerPixel;

	/** Where in the post-process pipeline to inject (PostProcess mode only). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Advanced",
		meta=(EditCondition = "Mode == EToneMapMode::PostProcess"))
	EToneMapPostProcessPass PostProcessPass = EToneMapPostProcessPass::Tonemap;

	/** Apply triangular-distribution dithering after processing to reduce 8-bit color banding.
	    Active in both modes — prevents visible gradient steps on smooth areas (sky, fog). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Advanced")
	bool bEnableDithering = true;

	/** Dither noise quantum — controls the amplitude of the dithering noise.
	    1/255 ≈ 0.00392 (8-bit, default) — safe for 8-bit and 10-bit displays.
	    1/1023 ≈ 0.00098 (10-bit) — minimal noise, use on 10-bit panels.
	    Higher values = stronger noise, more aggressive banding removal.
	    0 = no dithering even when enabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Advanced",
		meta=(EditCondition = "bEnableDithering",
		      ClampMin = "0.0", ClampMax = "0.02", UIMin = "0.0", UIMax = "0.02"))
	float DitherQuantization = 1.0f / 255.0f;

	/** Force the entire UE post-processing pipeline to use FP16 (64bpp) precision.
	    Prevents banding from 10-bit/11-bit quantization in TAA/TSR and tonemapper
	    output.  Doubles bandwidth of all post-process passes — small GPU cost on
	    modern hardware.  Sets r.PostProcessing.PropagateAlpha at runtime. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Advanced")
	bool bForceFP16Pipeline = true;

	// =========================================================================
	// Auto-Exposure (ReplaceTonemap mode only)
	// =========================================================================

	/** Automatic exposure mode. Krawczyk computes a scene key from log-average
	    luminance and adapts exposure over time (Krawczyk et al. 2005). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Auto-Exposure",
		meta=(EditCondition = "Mode == EToneMapMode::ReplaceTonemap"))
	EToneMapAutoExposure AutoExposureMode = EToneMapAutoExposure::EngineDefault;

	/** Speed of adaptation when the scene gets brighter (eye closing).
	    Higher = faster response. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Auto-Exposure",
		meta=(ClampMin = "0.1", ClampMax = "20.0", UIMin = "0.1", UIMax = "10.0",
		      EditCondition = "Mode == EToneMapMode::ReplaceTonemap && AutoExposureMode == EToneMapAutoExposure::Krawczyk"))
	float AdaptationSpeedUp = 3.0f;

	/** Speed of adaptation when the scene gets darker (eye opening).
	    Higher = faster response. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Auto-Exposure",
		meta=(ClampMin = "0.1", ClampMax = "20.0", UIMin = "0.1", UIMax = "10.0",
		      EditCondition = "Mode == EToneMapMode::ReplaceTonemap && AutoExposureMode == EToneMapAutoExposure::Krawczyk"))
	float AdaptationSpeedDown = 1.0f;

	/** Minimum auto-exposure multiplier. Prevents over-darkening in very bright scenes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Auto-Exposure",
		meta=(ClampMin = "0.001", ClampMax = "10.0", UIMin = "0.01", UIMax = "5.0",
		      EditCondition = "Mode == EToneMapMode::ReplaceTonemap && AutoExposureMode == EToneMapAutoExposure::Krawczyk"))
	float MinAutoExposure = 0.05f;

	/** Maximum auto-exposure multiplier. Prevents over-brightening in very dark scenes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Auto-Exposure",
		meta=(ClampMin = "1.0", ClampMax = "100.0", UIMin = "1.0", UIMax = "50.0",
		      EditCondition = "Mode == EToneMapMode::ReplaceTonemap && AutoExposureMode == EToneMapAutoExposure::Krawczyk"))
	float MaxAutoExposure = 20.0f;

	// =========================================================================
	// Film Curve (ReplaceTonemap mode only)
	// =========================================================================

	/** Tonemapping operator to use.
	 * 	https://64.github.io/tonemapping/
	    Reinhard variants are simpler with different saturation behaviour. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Film Curve",
		meta=(EditCondition = "Mode == EToneMapMode::ReplaceTonemap"))
	EToneMapFilmCurve FilmCurve = EToneMapFilmCurve::Hable;

	/** Shoulder Strength (A). Controls how highlights compress toward white.
	    Higher = more aggressive highlight compression. default: 0.15. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Film Curve|Hable",
		meta=(ClampMin = "0.01", ClampMax = "1.0", UIMin = "0.01", UIMax = "1.0",
		      EditCondition = "Mode == EToneMapMode::ReplaceTonemap && FilmCurve == EToneMapFilmCurve::Hable"))
	float HableShoulderStrength = 0.15f;

	/** Linear Strength (B). Length of the linear (faithful) middle region.
	    Higher = brighter midtones / wider linear zone. default: 0.50. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Film Curve|Hable",
		meta=(ClampMin = "0.01", ClampMax = "1.0", UIMin = "0.01", UIMax = "1.0",
		      EditCondition = "Mode == EToneMapMode::ReplaceTonemap && FilmCurve == EToneMapFilmCurve::Hable"))
	float HableLinearStrength = 0.50f;

	/** Linear Angle (C). Angle of the linear region. default: 0.10. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Film Curve|Hable",
		meta=(ClampMin = "0.01", ClampMax = "1.0", UIMin = "0.01", UIMax = "0.5",
		      EditCondition = "Mode == EToneMapMode::ReplaceTonemap && FilmCurve == EToneMapFilmCurve::Hable"))
	float HableLinearAngle = 0.10f;

	/** Toe Strength (D). Depth of the toe curve (shadow region).
	    Higher = deeper toe. default: 0.20. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Film Curve|Hable",
		meta=(ClampMin = "0.01", ClampMax = "1.0", UIMin = "0.01", UIMax = "1.0",
		      EditCondition = "Mode == EToneMapMode::ReplaceTonemap && FilmCurve == EToneMapFilmCurve::Hable"))
	float HableToeStrength = 0.20f;

	/** Toe Numerator (E). Controls black level lift.
	    Higher = lifted shadows / less contrast in darks. default: 0.02. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Film Curve|Hable",
		meta=(ClampMin = "0.001", ClampMax = "0.5", UIMin = "0.001", UIMax = "0.2",
		      EditCondition = "Mode == EToneMapMode::ReplaceTonemap && FilmCurve == EToneMapFilmCurve::Hable"))
	float HableToeNumerator = 0.02f;

	/** Toe Denominator (F). Toe response sharpness.
	    Higher = sharper toe transition. default: 0.30. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Film Curve|Hable",
		meta=(ClampMin = "0.01", ClampMax = "1.0", UIMin = "0.01", UIMax = "1.0",
		      EditCondition = "Mode == EToneMapMode::ReplaceTonemap && FilmCurve == EToneMapFilmCurve::Hable"))
	float HableToeDenominator = 0.30f;

	/** White Point (W). Input value that maps to output 1.0.
	    Lower = brighter overall, higher = more HDR headroom. default: 11.2. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Film Curve|Hable",
		meta=(ClampMin = "1.0", ClampMax = "30.0", UIMin = "1.0", UIMax = "20.0",
		      EditCondition = "Mode == EToneMapMode::ReplaceTonemap && FilmCurve == EToneMapFilmCurve::Hable"))
	float HableWhitePoint = 11.2f;

	/** Reinhard white point (L_white). Maximum scene luminance that maps to white.
	    Uses the extended Reinhard formula: L*(1 + L/Lw^2) / (1 + L).
	    Lower = brighter, more burn-to-white; very high = basic Reinhard (no clipping). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Film Curve|Reinhard",
		meta=(ClampMin = "0.5", ClampMax = "100.0", UIMin = "1.0", UIMax = "20.0",
		      EditCondition = "Mode == EToneMapMode::ReplaceTonemap && (FilmCurve == EToneMapFilmCurve::ReinhardLuminance || FilmCurve == EToneMapFilmCurve::ReinhardJodie || FilmCurve == EToneMapFilmCurve::ReinhardStandard)"))
	float ReinhardWhitePoint = 100.0f;

	/** Custom tone curve asset. X is normalized 0..1 across CustomCurveMinEV..CustomCurveMaxEV; Y is mapped display-linear luminance. In True HDR output, Y can exceed 1.0 for HDR headroom. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Film Curve|Custom",
		meta=(EditCondition = "Mode == EToneMapMode::ReplaceTonemap && FilmCurve == EToneMapFilmCurve::CustomLuminanceCurve"))
	TObjectPtr<UCurveFloat> CustomToneCurve = nullptr;

	/** Minimum scene EV represented by the baked texture. Usually leave this at the default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Film Curve|Custom",
		meta=(ClampMin = "-24.0", ClampMax = "0.0", UIMin = "-16.0", UIMax = "-4.0",
		      EditCondition = "Mode == EToneMapMode::ReplaceTonemap && FilmCurve == EToneMapFilmCurve::CustomLuminanceCurve",
		      AdvancedDisplay))
	float CustomCurveMinEV = -10.0f;

	/** Maximum scene EV represented by the baked texture. Usually leave this at the default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Film Curve|Custom",
		meta=(ClampMin = "0.0", ClampMax = "24.0", UIMin = "2.0", UIMax = "12.0",
		      EditCondition = "Mode == EToneMapMode::ReplaceTonemap && FilmCurve == EToneMapFilmCurve::CustomLuminanceCurve",
		      AdvancedDisplay))
	float CustomCurveMaxEV = 6.5f;

	/** Number of samples baked from the CurveFloat into the GPU texture. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Film Curve|Custom",
		meta=(ClampMin = "64", ClampMax = "4096", UIMin = "256", UIMax = "2048",
		      EditCondition = "Mode == EToneMapMode::ReplaceTonemap && FilmCurve == EToneMapFilmCurve::CustomLuminanceCurve",
		      AdvancedDisplay))
	int32 CustomCurveSampleCount = 1024;

	/** Runtime texture baked from CustomToneCurve. Stored transiently and rebuilt after preset load. */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> CustomToneCurveTexture = nullptr;

	/** Maximum output brightness relative to paper white. Values above 1.0 require True HDR and are capped to available display headroom. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Film Curve|GT Uchimura Classic",
		meta=(ClampMin = "0.1", ClampMax = "16.0", UIMin = "0.5", UIMax = "5.0",
		      EditCondition = "Mode == EToneMapMode::ReplaceTonemap && FilmCurve == EToneMapFilmCurve::GTUchimuraClassic"))
	float UchimuraMaxBrightness = 1.0f;

	/** Slope of the straight middle section. 1.0 is the original neutral-contrast default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Film Curve|GT Uchimura Classic",
		meta=(ClampMin = "0.05", ClampMax = "4.0", UIMin = "0.25", UIMax = "2.0",
		      EditCondition = "Mode == EToneMapMode::ReplaceTonemap && FilmCurve == EToneMapFilmCurve::GTUchimuraClassic"))
	float UchimuraContrast = 1.0f;

	/** Input value where the straight middle section begins. Runtime validation keeps it below Max Brightness. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Film Curve|GT Uchimura Classic",
		meta=(ClampMin = "0.001", ClampMax = "16.0", UIMin = "0.01", UIMax = "1.0",
		      EditCondition = "Mode == EToneMapMode::ReplaceTonemap && FilmCurve == EToneMapFilmCurve::GTUchimuraClassic"))
	float UchimuraLinearStart = 0.22f;

	/** Fraction of the output range occupied by the straight section before the shoulder. Must remain below 1.0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Film Curve|GT Uchimura Classic",
		meta=(ClampMin = "0.0", ClampMax = "0.99", UIMin = "0.0", UIMax = "0.99",
		      EditCondition = "Mode == EToneMapMode::ReplaceTonemap && FilmCurve == EToneMapFilmCurve::GTUchimuraClassic"))
	float UchimuraLinearLength = 0.4f;

	/** Toe power controlling black tightness. Higher values make the deepest shadows tighter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Film Curve|GT Uchimura Classic",
		meta=(ClampMin = "0.1", ClampMax = "4.0", UIMin = "0.5", UIMax = "3.0",
		      EditCondition = "Mode == EToneMapMode::ReplaceTonemap && FilmCurve == EToneMapFilmCurve::GTUchimuraClassic"))
	float UchimuraBlackTightness = 1.33f;

	/** Raises the black floor. Runtime validation keeps it below Linear Start to avoid an inverted toe. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Film Curve|GT Uchimura Classic",
		meta=(ClampMin = "0.0", ClampMax = "0.5", UIMin = "0.0", UIMax = "0.1",
		      EditCondition = "Mode == EToneMapMode::ReplaceTonemap && FilmCurve == EToneMapFilmCurve::GTUchimuraClassic",
		      AdvancedDisplay))
	float UchimuraPedestal = 0.0f;

	/** HDR color grading saturation (applied pre-curve in linear HDR).
	    1.0 = neutral (default). 0 = monochrome. 2 = boosted. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Film Curve",
		meta=(ClampMin = "0.0", ClampMax = "2.0", UIMin = "0.0", UIMax = "2.0",
		      EditCondition = "Mode == EToneMapMode::ReplaceTonemap"))
	float HDRSaturation = 1.0f;

	/** HDR color balance tint (applied pre-curve in linear HDR).
	    Default white = no change. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Film Curve",
		meta=(EditCondition = "Mode == EToneMapMode::ReplaceTonemap"))
	FLinearColor HDRColorBalance = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

	// =========================================================================
	// Durand-Dorsey 2002 Bilateral Tone Mapping
	// https://people.csail.mit.edu/fredo/PUBLI/Siggraph2002/DurandBilateral.pdf
	// https://cs.brown.edu/courses/cs129/2012/lectures/18.pdf
	// =========================================================================

	/** Spatial sigma for the bilateral filter (pixels at half-res).
	    Controls how far the filter reaches; larger = wider base-layer smoothing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Film Curve|Durand",
		meta=(ClampMin = "2.0", ClampMax = "64.0", UIMin = "2.0", UIMax = "64.0",
		      EditCondition = "Mode == EToneMapMode::ReplaceTonemap && FilmCurve == EToneMapFilmCurve::Durand"))
	float DurandSpatialSigma = 16.0f;

	/** Range sigma for the bilateral filter (log-luminance units).
	    Smaller = stronger edge-preservation; larger = more smoothing across edges. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Film Curve|Durand",
		meta=(ClampMin = "0.05", ClampMax = "2.0", UIMin = "0.05", UIMax = "2.0",
		      EditCondition = "Mode == EToneMapMode::ReplaceTonemap && FilmCurve == EToneMapFilmCurve::Durand"))
	float DurandRangeSigma = 0.35f;

	/** Base layer compression factor.  Lower = more compression (wider dynamic range reduction). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Film Curve|Durand",
		meta=(ClampMin = "0.1", ClampMax = "0.95", UIMin = "0.1", UIMax = "0.95",
		      EditCondition = "Mode == EToneMapMode::ReplaceTonemap && FilmCurve == EToneMapFilmCurve::Durand"))
	float DurandBaseCompression = 0.5f;

	/** Detail layer boost.  1.0 = no change. >1 = enhanced local contrast. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Film Curve|Durand",
		meta=(ClampMin = "0.5", ClampMax = "2.0", UIMin = "0.5", UIMax = "2.0",
		      EditCondition = "Mode == EToneMapMode::ReplaceTonemap && FilmCurve == EToneMapFilmCurve::Durand"))
	float DurandDetailBoost = 1.0f;

	// =========================================================================
	// Fattal et al. 2002 Gradient-Domain Tone Mapping - experimental, could cause visible artifacts - thresholds/quantization, viewport issue to fix later on.
	// https://dl.acm.org/doi/10.1145/566654.566573
	// =========================================================================

	/** Alpha — controls how aggressively large gradients are attenuated.
	    Typical range 0.01–0.5.  Lower = more compression of bright transitions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Film Curve|Fattal",
		meta=(ClampMin = "0.01", ClampMax = "0.5", UIMin = "0.01", UIMax = "0.5",
		      EditCondition = "Mode == EToneMapMode::ReplaceTonemap && FilmCurve == EToneMapFilmCurve::Fattal"))
	float FattalAlpha = 0.1f;

	/** Beta — the exponent for gradient attenuation (φ_s power).
	    0.9 is a good starting point; lower = stronger attenuation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Film Curve|Fattal",
		meta=(ClampMin = "0.5", ClampMax = "1.0", UIMin = "0.5", UIMax = "1.0",
		      EditCondition = "Mode == EToneMapMode::ReplaceTonemap && FilmCurve == EToneMapFilmCurve::Fattal"))
	float FattalBeta = 0.9f;

	/** Output saturation scale (applied to chrominance after tone mapping).
	    1.0 = neutral, 0 = monochrome, >1 = boosted. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Film Curve|Fattal",
		meta=(ClampMin = "0.0", ClampMax = "1.5", UIMin = "0.0", UIMax = "1.5",
		      EditCondition = "Mode == EToneMapMode::ReplaceTonemap && FilmCurve == EToneMapFilmCurve::Fattal"))
	float FattalSaturation = 0.8f;

	/** Small noise floor added to Hy/Hx gradients to prevent divide-by-zero.
	    Rarely needs changing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Film Curve|Fattal",
		meta=(ClampMin = "0.0", ClampMax = "0.01", UIMin = "0.0", UIMax = "0.01",
		      EditCondition = "Mode == EToneMapMode::ReplaceTonemap && FilmCurve == EToneMapFilmCurve::Fattal"))
	float FattalNoise = 0.0001f;

	/** Number of Jacobi solver iterations.  Seeding from log(lum) makes partial convergence
	    useful — 30 iterations gives good results; 60+ is high quality.  Each iteration is
	    one fullscreen pass so keep this reasonable for real-time use. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Film Curve|Fattal",
		meta=(ClampMin = "4", ClampMax = "200", UIMin = "4", UIMax = "100",
		      EditCondition = "Mode == EToneMapMode::ReplaceTonemap && FilmCurve == EToneMapFilmCurve::Fattal"))
	int32 FattalJacobiIterations = 30;

	// =========================================================================
	// AgX (Sobotka) Display Rendering Transform
	// https://github.com/sobotka/AgX
	// =========================================================================

	/** Creative look applied after the AgX base curve. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Film Curve|AgX",
		meta=(EditCondition = "Mode == EToneMapMode::ReplaceTonemap && FilmCurve == EToneMapFilmCurve::AgX"))
	EAgXLook AgXLook = EAgXLook::None;

	/** Minimum exposure (EV) for the log2 encoding range.
	    Lower = captures more shadow detail. Default: -10.0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Film Curve|AgX",
		meta=(ClampMin = "-20.0", ClampMax = "0.0", UIMin = "-15.0", UIMax = "-5.0",
		      EditCondition = "Mode == EToneMapMode::ReplaceTonemap && FilmCurve == EToneMapFilmCurve::AgX"))
	float AgXMinEV = -10.0f;

	/** Maximum exposure (EV) for the log2 encoding range.
	    Higher = more highlight headroom before clipping. Default: +6.5. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Film Curve|AgX",
		meta=(ClampMin = "0.0", ClampMax = "20.0", UIMin = "2.0", UIMax = "12.0",
		      EditCondition = "Mode == EToneMapMode::ReplaceTonemap && FilmCurve == EToneMapFilmCurve::AgX"))
	float AgXMaxEV = 6.5f;

	// =========================================================================
	// Additional Lens Effects — Ciliary Corona & Lenticular Halo
	// =========================================================================

	/** Enable ciliary corona: bright-light spike streaks produced by light diffraction over
	    the iris/lens boundary, forming a star-burst pattern around very bright sources. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Additional Lens Effects")
	bool bEnableCiliaryCorona = false;

	/** Overall brightness of the corona streaks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Additional Lens Effects",
		meta=(ClampMin = "0.0", ClampMax = "4.0", UIMin = "0.0", UIMax = "2.0",
		      EditCondition = "bEnableCiliaryCorona"))
	float CoronaIntensity = 0.5f;

	/** Number of spike arms (must be even — rotational symmetry).  6 = hexagonal, 8 = octagonal. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Additional Lens Effects",
		meta=(ClampMin = "2", ClampMax = "16", UIMin = "2", UIMax = "12",
		      EditCondition = "bEnableCiliaryCorona"))
	int32 CoronaSpikeCount = 6;

	/** Pixel length of each spike arm (at full resolution). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Additional Lens Effects",
		meta=(ClampMin = "10", ClampMax = "400", UIMin = "10", UIMax = "200",
		      EditCondition = "bEnableCiliaryCorona"))
	int32 CoronaSpikeLength = 80;

	/** Minimum luminance for a pixel to emit corona streaks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Additional Lens Effects",
		meta=(ClampMin = "0.0", ClampMax = "10.0", UIMin = "0.0", UIMax = "5.0",
		      EditCondition = "bEnableCiliaryCorona"))
	float CoronaThreshold = 0.8f;

	/** Enable lenticular halo: a faint tinted ring appearing around very bright sources due
	    to scattering in the lens glass (Mie scattering / internal reflections). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Additional Lens Effects")
	bool bEnableLenticularHalo = false;

	/** Halo shape. Stretched Lines creates line-like lens scatter instead of a closed ring. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Additional Lens Effects",
		meta=(EditCondition = "bEnableLenticularHalo"))
	EToneMapHaloPattern HaloPattern = EToneMapHaloPattern::StretchedLines;

	/** Overall brightness of the halo ring. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Additional Lens Effects",
		meta=(ClampMin = "0.0", ClampMax = "4.0", UIMin = "0.0", UIMax = "2.0",
		      EditCondition = "bEnableLenticularHalo"))
	float HaloIntensity = 0.3f;

	/** Radius of the halo ring center, in [0..1] UV-space units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Additional Lens Effects",
		meta=(ClampMin = "0.01", ClampMax = "0.5", UIMin = "0.01", UIMax = "0.5",
		      EditCondition = "bEnableLenticularHalo"))
	float HaloRadius = 0.15f;

	/** Thickness (radial width) of the halo ring in UV-space units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Additional Lens Effects",
		meta=(ClampMin = "0.002", ClampMax = "0.1", UIMin = "0.002", UIMax = "0.1",
		      EditCondition = "bEnableLenticularHalo"))
	float HaloThickness = 0.03f;

	/** Breaks up the perfect circular halo with stable lens-like radial and angular variation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Additional Lens Effects",
		meta=(ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0",
		      EditCondition = "bEnableLenticularHalo"))
	float HaloIrregularity = 0.45f;

	/** Smooths the halo into longer curved arcs instead of a uniform ring. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Additional Lens Effects",
		meta=(ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0",
		      EditCondition = "bEnableLenticularHalo"))
	float HaloArcStretch = 0.35f;

	/** Number of bilateral halo line directions. Only used by Stretched Lines. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Additional Lens Effects",
		meta=(ClampMin = "1", ClampMax = "12", UIMin = "1", UIMax = "8",
		      EditCondition = "bEnableLenticularHalo && HaloPattern == EToneMapHaloPattern::StretchedLines"))
	int32 HaloLineCount = 5;

	/** Width of each stretched halo line in screen pixels. Only used by Stretched Lines. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Additional Lens Effects",
		meta=(ClampMin = "0.5", ClampMax = "40.0", UIMin = "0.5", UIMax = "20.0",
		      EditCondition = "bEnableLenticularHalo && HaloPattern == EToneMapHaloPattern::StretchedLines"))
	float HaloLineThickness = 5.0f;

	/** Minimum source luminance that contributes to the halo. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Additional Lens Effects",
		meta=(ClampMin = "0.0", ClampMax = "10.0", UIMin = "0.0", UIMax = "5.0",
		      EditCondition = "bEnableLenticularHalo"))
	float HaloThreshold = 0.9f;

	/** Tint color for the halo ring.  Pale blue-white mimics real lens coatings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Additional Lens Effects",
		meta=(EditCondition = "bEnableLenticularHalo"))
	FLinearColor HaloTint = FLinearColor(0.85f, 0.90f, 1.0f, 1.0f);

	// =========================================================================
	// Bloom
	// =========================================================================

	/** Enable bloom effect */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Bloom")
	bool bEnableBloom = false;

	/** Bloom effect mode - Standard Gaussian, Directional Glare, Kawase, or Soft Focus */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Bloom",
		meta=(EditCondition = "bEnableBloom"))
	EBloomMode BloomMode = EBloomMode::SoftFocus;

	/** Overall intensity of the bloom effect */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Bloom",
		meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "32.0",
		        SupportDynamicSliderMaxValue = "true", EditCondition = "bEnableBloom"))
	float BloomIntensity = 1.0f;

	/** Threshold for bloom - only pixels brighter than this will bloom */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Bloom",
		meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "10.0",
		        EditCondition = "bEnableBloom && BloomMode != EBloomMode::SoftFocus && BloomMode != EBloomMode::SoftFocusGlow"))
	float BloomThreshold = 0.8f;

	/** Softness of the bloom threshold edge (0 = hard cutoff, 1 = very wide soft knee).
	 *  Higher values eliminate the visible 'cutoff circle' around very bright sources. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Bloom",
		meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0",
		        EditCondition = "bEnableBloom && BloomMode != EBloomMode::SoftFocus && BloomMode != EBloomMode::SoftFocusGlow && BloomMode != EBloomMode::Kawase"))
	float BloomThresholdSoftness = 0.5f;

	/** Optional HDR source peak clamp before the bloom blur (0 = unlimited).
	 *  This is a firefly/ring prevention control for ReplaceTonemap HDR input,
	 *  not a final bloom intensity limiter. Use Protect Highlights for final
	 *  soft limiting after Bloom Intensity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Bloom|Advanced",
		meta = (DisplayName = "HDR Source Peak Clamp", ClampMin = "0.0", UIMin = "0.0", UIMax = "100.0",
		        EditCondition = "bEnableBloom"))
	float BloomMaxBrightness = 0.0f;

	/** Size of the bloom effect */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Bloom",
		meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "64.0",
		        EditCondition = "bEnableBloom && (BloomMode == EBloomMode::Standard || BloomMode == EBloomMode::DirectionalGlare || BloomMode == EBloomMode::SoftFocus || BloomMode == EBloomMode::SoftFocusGlow)"))
	float BloomSize = 16.0f;

	/** Source color treatment for bloom. Legacy Luminance restores the older monochrome bloom look. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Bloom",
		meta=(EditCondition = "bEnableBloom"))
	EBloomColorMode BloomColorMode = EBloomColorMode::SceneColor;

	/** Deprecated serialized compatibility setting. Use BloomColorMode instead. */
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use BloomColorMode instead."))
	bool bUseSceneColor = true;

	/** Tint color for the bloom */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Bloom",
		meta = (EditCondition = "bEnableBloom && BloomColorMode == EBloomColorMode::Tint"))
	FLinearColor BloomTint = FLinearColor::White;

	/** Blend mode for compositing bloom onto the scene.
	 *  Soft Focus Glow automatically uses Soft Light Glow to avoid darkening. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Bloom",
		meta=(EditCondition = "bEnableBloom"))
	EBloomBlendMode BloomBlendMode = EBloomBlendMode::SoftLight;

	/** Strength of the selected bloom blend mode. 0 = original scene, 1 = full blend result.
	 *  Does not change bloom extraction or blur intensity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Bloom",
		meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0",
		        EditCondition = "bEnableBloom"))
	float BloomBlendStrength = 1.0f;

	/** Saturation boost for bloom colors (1.0 = normal) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Bloom",
		meta = (ClampMin = "0.0", ClampMax = "3.0", UIMin = "0.0", UIMax = "2.0",
		        EditCondition = "bEnableBloom"))
	float BloomSaturation = 1.0f;

	/** Protect highlights from over-brightening */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Bloom",
		meta=(EditCondition = "bEnableBloom"))
	bool bProtectHighlights = false;

	/** Highlight protection strength */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Bloom",
		meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0",
		        EditCondition = "bEnableBloom && bProtectHighlights"))
	float HighlightProtection = 0.5f;

	// ---- Bloom Quality (Standard & Soft Focus) ----

	/** Downsample scale (higher = better quality but slower). 1.0 = half res, 2.0 = full res */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Bloom|Quality",
		meta = (ClampMin = "0.25", ClampMax = "2.0", UIMin = "0.5", UIMax = "2.0",
		        EditCondition = "bEnableBloom"))
	float DownsampleScale = 1.0f;

	/** Number of blur passes (more passes = smoother bloom but slower) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Bloom|Quality",
		meta = (ClampMin = "1", ClampMax = "4", UIMin = "1", UIMax = "4",
		        EditCondition = "bEnableBloom && (BloomMode == EBloomMode::Standard || BloomMode == EBloomMode::SoftFocus || BloomMode == EBloomMode::SoftFocusGlow)"))
	int32 BlurPasses = 1;

	/** Reserved for future blur quality selection; current blur shader uses a fixed kernel. */
	UPROPERTY()
	int32 BlurSamples = 5;

	/** Reserved for future upsampling quality selection. */
	UPROPERTY()
	bool bHighQualityUpsampling = false;

	// ---- Directional Glare ----

	/** Number of directional streaks (4-6 recommended for star patterns) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Bloom|Directional Glare",
		meta = (ClampMin = "2", ClampMax = "16", UIMin = "2", UIMax = "12",
		        EditCondition = "bEnableBloom && BloomMode == EBloomMode::DirectionalGlare", EditConditionHides))
	int32 GlareStreakCount = 6;

	/** Length of each streak in pixels (at full resolution) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Bloom|Directional Glare",
		meta = (ClampMin = "5", ClampMax = "200", UIMin = "10", UIMax = "120",
		        EditCondition = "bEnableBloom && BloomMode == EBloomMode::DirectionalGlare", EditConditionHides))
	int32 GlareStreakLength = 40;

	/** Rotation offset for streak directions in degrees */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Bloom|Directional Glare",
		meta = (ClampMin = "0.0", ClampMax = "180.0", UIMin = "0.0", UIMax = "90.0",
		        EditCondition = "bEnableBloom && BloomMode == EBloomMode::DirectionalGlare", EditConditionHides))
	float GlareRotationOffset = 0.0f;

	/** Exponential falloff rate for streak intensity */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Bloom|Directional Glare",
		meta = (ClampMin = "0.5", ClampMax = "10.0", UIMin = "1.0", UIMax = "5.0",
		        EditCondition = "bEnableBloom && BloomMode == EBloomMode::DirectionalGlare", EditConditionHides))
	float GlareFalloff = 3.0f;

	/** Number of samples per streak direction (higher = smoother streaks, slower).
	 *  8=fast, 16=default, 32=high, 48/64=ultra quality. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Bloom|Directional Glare",
		meta = (ClampMin = "8", ClampMax = "64", UIMin = "8", UIMax = "64",
		        EditCondition = "bEnableBloom && BloomMode == EBloomMode::DirectionalGlare", EditConditionHides))
	int32 GlareSamples = 16;

	// ---- Kawase Bloom ----

	/** Number of mip levels in the bloom pyramid (more = larger blur radius) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Bloom|Kawase",
		meta = (ClampMin = "3", ClampMax = "8", UIMin = "4", UIMax = "7",
		        EditCondition = "bEnableBloom && BloomMode == EBloomMode::Kawase", EditConditionHides))
	int32 KawaseMipCount = 5;

	/** Upsample filter radius (higher = softer bloom) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Bloom|Kawase",
		meta = (ClampMin = "0.0001", ClampMax = "0.01", UIMin = "0.001", UIMax = "0.005",
		        EditCondition = "bEnableBloom && BloomMode == EBloomMode::Kawase", EditConditionHides))
	float KawaseFilterRadius = 0.002f;

	/** Apply soft color threshold instead of hard brightness cutoff */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Bloom|Kawase",
		meta = (EditCondition = "bEnableBloom && BloomMode == EBloomMode::Kawase", EditConditionHides))
	bool bKawaseSoftThreshold = true;

	/** Threshold knee - controls the smoothness of the threshold transition */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Bloom|Kawase",
		meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0",
		        EditCondition = "bEnableBloom && BloomMode == EBloomMode::Kawase && bKawaseSoftThreshold", EditConditionHides))
	float KawaseThresholdKnee = 0.5f;

	// ---- Soft Focus (deprecated tuning — hidden from UI) ----

	UPROPERTY() float SoftFocusOverlayMultiplier = 0.5f;
	UPROPERTY() float SoftFocusBlendStrength = 0.33f;
	UPROPERTY() float SoftFocusSoftLightMultiplier = 0.4f;
	UPROPERTY() float SoftFocusFinalBlend = 0.25f;

	// =========================================================================
	// Vignette
	// =========================================================================

	/** Enable vignette effect (screen-space darkening / lightening from edges). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Vignette")
	bool bEnableVignette = false;

	/** Vignette shape: Circular (radial) or Square (per-edge). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Vignette",
		meta=(EditCondition = "bEnableVignette"))
	EVignetteMode VignetteMode = EVignetteMode::Circular;

	/** Size of the clear zone from center.  0 = all vignette, 100 = no vignette. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Vignette",
		meta=(ClampMin = "0.0", ClampMax = "100.0", UIMin = "0.0", UIMax = "100.0",
		      EditCondition = "bEnableVignette"))
	float VignetteSize = 30.0f;

	/** Vignette intensity.  Positive = darken edges.  Negative = lighten edges. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Vignette",
		meta=(ClampMin = "-100.0", ClampMax = "100.0", UIMin = "-100.0", UIMax = "100.0",
		      EditCondition = "bEnableVignette"))
	float VignetteIntensity = 50.0f;

	/** Falloff curve shape.  Controls how the gradient transitions from clear center to darkened edge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Vignette",
		meta=(EditCondition = "bEnableVignette"))
	EVignetteFalloff VignetteFalloff = EVignetteFalloff::Smooth;

	/** Power exponent for the Custom falloff curve.  <1 = hard edge, 1 = linear, >1 = soft/gradual. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Vignette",
		meta=(ClampMin = "0.1", ClampMax = "8.0", UIMin = "0.1", UIMax = "5.0",
		      EditCondition = "bEnableVignette && VignetteFalloff == EVignetteFalloff::Custom"))
	float VignetteFalloffExponent = 2.0f;

	/** Use an alpha texture mask for custom vignette shapes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Vignette",
		meta=(EditCondition = "bEnableVignette"))
	bool bVignetteUseAlphaTexture = false;

	/** Texture mask.  The selected channel controls per-pixel vignette strength.
	    White (1) = no vignette, Black (0) = full vignette. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Vignette",
		meta=(EditCondition = "bEnableVignette && bVignetteUseAlphaTexture"))
	TObjectPtr<UTexture> VignetteAlphaTexture;

	/** Which channel of the texture to read as the mask value.
	    Use Red for single-channel / grayscale textures, Alpha for packed RGBA. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Vignette",
		meta=(EditCondition = "bEnableVignette && bVignetteUseAlphaTexture"))
	EVignetteTextureChannel VignetteTextureChannel = EVignetteTextureChannel::Alpha;

	/** When enabled, only the texture drives the effect — no procedural vignette
	    geometry.  The scene is multiplied by the texture mask, with Intensity as strength. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Vignette",
		meta=(EditCondition = "bEnableVignette && bVignetteUseAlphaTexture"))
	bool bVignetteAlphaTextureOnly = false;

	// =========================================================================
	// LUT (Color Grading Look-Up Table)
	// =========================================================================

	/** Enable LUT-based color grading.  Applies a standard UE LUT texture as a
	    final color-grade lookup after all ToneMapFX processing (post-tonemap,
	    post-sRGB).  Supported resolutions: 256x16, 1024x32, 4096x64. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|LUT")
	bool bEnableLUT = false;

	/** Color Grading LUT texture.
	    Standard UE format: horizontal strip of N slices, each N×N pixels.
	    Common sizes: 256×16 (16³), 1024×32 (32³), 4096×64 (64³). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|LUT",
		meta=(EditCondition = "bEnableLUT"))
	TObjectPtr<UTexture> LUTTexture;

	/** LUT blend intensity.  0 = no effect (bypass), 1 = full LUT. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|LUT",
		meta=(ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0",
		      EditCondition = "bEnableLUT"))
	float LUTIntensity = 1.0f;

	// =========================================================================
	// Engine Overrides
	// =========================================================================

	/** Disable Unreal Engine's built-in bloom (zeros BloomIntensity).
	 *  Recommended when using ToneMapFX bloom to avoid double-bloom. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Engine Overrides")
	bool bDisableUnrealBloom = true;

	/** Disable Unreal Engine's built-in motion blur (zeros MotionBlurAmount and MotionBlurMax).
	 *  Useful for clean ToneMapFX evaluation without temporal smearing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Engine Overrides")
	bool bDisableUnrealMotionBlur = false;

	// =========================================================================
	// Local Exposure
	// =========================================================================

	/** Choose no local exposure, the standard Unreal volume settings, or the custom Durand prototype.
	 *  Custom exposure requires Replace Tonemapper with a non-spatial film curve. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Local Exposure",
		meta=(DisplayName = "Local Exposure Method"))
	EToneMapLocalExposureMode LocalExposureMode = EToneMapLocalExposureMode::Off;

	/** Blend the bounded local adjustment in stops. Zero bypasses the custom passes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Local Exposure|Durand",
		meta=(DisplayName = "Strength", ClampMin = "0.0", ClampMax = "1.0",
		      UIMin = "0.0", UIMax = "1.0", EditCondition = "Mode == EToneMapMode::ReplaceTonemap && LocalExposureMode == EToneMapLocalExposureMode::Durand && FilmCurve != EToneMapFilmCurve::Durand && FilmCurve != EToneMapFilmCurve::Fattal"))
	float LocalExposureStrength = 1.0f;

	/** Spatial sigma at full viewport resolution. Sampling support is capped at 32 pixels per axis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Local Exposure|Durand",
		meta=(DisplayName = "Spatial Sigma (Pixels)", ClampMin = "1.0", ClampMax = "32.0",
		      UIMin = "1.0", UIMax = "32.0", EditCondition = "Mode == EToneMapMode::ReplaceTonemap && LocalExposureMode == EToneMapLocalExposureMode::Durand && FilmCurve != EToneMapFilmCurve::Durand && FilmCurve != EToneMapFilmCurve::Fattal"))
	float LocalExposureSpatialSigma = 16.0f;

	/** Range sigma in log2 stops. Smaller values stop smoothing more strongly at luminance edges. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Local Exposure|Durand",
		meta=(DisplayName = "Edge Range (Stops)", ClampMin = "0.05", ClampMax = "8.0",
		      UIMin = "0.05", UIMax = "4.0", EditCondition = "Mode == EToneMapMode::ReplaceTonemap && LocalExposureMode == EToneMapLocalExposureMode::Durand && FilmCurve != EToneMapFilmCurve::Durand && FilmCurve != EToneMapFilmCurve::Fattal"))
	float LocalExposureRangeSigmaEV = 1.16f;

	/** Base-layer contrast above middle gray. One is neutral; lower values darken bright regions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Local Exposure|Durand",
		meta=(DisplayName = "Highlight Contrast", ClampMin = "0.0", ClampMax = "1.0",
		      UIMin = "0.0", UIMax = "1.0", EditCondition = "Mode == EToneMapMode::ReplaceTonemap && LocalExposureMode == EToneMapLocalExposureMode::Durand && FilmCurve != EToneMapFilmCurve::Durand && FilmCurve != EToneMapFilmCurve::Fattal"))
	float LocalExposureHighlightContrast = 0.85f;

	/** Base-layer contrast below middle gray. One is neutral; lower values lift dark regions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Local Exposure|Durand",
		meta=(DisplayName = "Shadow Contrast", ClampMin = "0.0", ClampMax = "1.0",
		      UIMin = "0.0", UIMax = "1.0", EditCondition = "Mode == EToneMapMode::ReplaceTonemap && LocalExposureMode == EToneMapLocalExposureMode::Durand && FilmCurve != EToneMapFilmCurve::Durand && FilmCurve != EToneMapFilmCurve::Fattal"))
	float LocalExposureShadowContrast = 0.85f;

	/** Maximum positive local exposure adjustment before Strength. Zero disables lifting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Local Exposure|Durand",
		meta=(DisplayName = "Maximum Shadow Lift (Stops)", ClampMin = "0.0", ClampMax = "8.0",
		      UIMin = "0.0", UIMax = "4.0", EditCondition = "Mode == EToneMapMode::ReplaceTonemap && LocalExposureMode == EToneMapLocalExposureMode::Durand && FilmCurve != EToneMapFilmCurve::Durand && FilmCurve != EToneMapFilmCurve::Fattal"))
	float LocalExposureMaxLiftEV = 1.0f;

	/** Maximum negative local exposure adjustment before Strength. Zero disables darkening. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Local Exposure|Durand",
		meta=(DisplayName = "Maximum Highlight Darkening (Stops)", ClampMin = "0.0", ClampMax = "8.0",
		      UIMin = "0.0", UIMax = "4.0", EditCondition = "Mode == EToneMapMode::ReplaceTonemap && LocalExposureMode == EToneMapLocalExposureMode::Durand && FilmCurve != EToneMapFilmCurve::Durand && FilmCurve != EToneMapFilmCurve::Fattal"))
	float LocalExposureMaxDarkenEV = 1.0f;

	/** Protect this interval on either side of middle gray, with a smooth one-stop onset outside it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Local Exposure|Durand",
		meta=(DisplayName = "Midtone Protection (Stops)", ClampMin = "0.0", ClampMax = "4.0",
		      UIMin = "0.0", UIMax = "4.0", EditCondition = "Mode == EToneMapMode::ReplaceTonemap && LocalExposureMode == EToneMapLocalExposureMode::Durand && FilmCurve != EToneMapFilmCurve::Durand && FilmCurve != EToneMapFilmCurve::Fattal"))
	float LocalExposureMidtoneProtectionEV = 1.0f;

	/** Shift the 0.18 middle-gray pivot in stops. Manual plugin exposure is applied afterward. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Local Exposure|Durand",
		meta=(DisplayName = "Middle Grey Bias (Stops)", ClampMin = "-4.0", ClampMax = "4.0",
		      UIMin = "-4.0", UIMax = "4.0", EditCondition = "Mode == EToneMapMode::ReplaceTonemap && LocalExposureMode == EToneMapLocalExposureMode::Durand && FilmCurve != EToneMapFilmCurve::Durand && FilmCurve != EToneMapFilmCurve::Fattal"))
	float LocalExposureMiddleGreyBias = 0.0f;

	/** Fade shadow lifting between eight and twelve stops below the middle-gray pivot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tone Map|Local Exposure|Durand",
		meta=(DisplayName = "Protect Deep Shadows", EditCondition = "Mode == EToneMapMode::ReplaceTonemap && LocalExposureMode == EToneMapLocalExposureMode::Durand && FilmCurve != EToneMapFilmCurve::Durand && FilmCurve != EToneMapFilmCurve::Fattal"))
	bool bLocalExposureProtectDeepShadows = true;

	/** Serialized legacy checkbox retained for older components and text presets. */
	UPROPERTY()
	bool bEnableUnrealLocalExposure = false;

	// =========================================================================
	// Presets (Save / Load to .txt files via OS file dialog)
	// =========================================================================

#if WITH_EDITOR
	/** Open a Save File dialog to choose where to save the preset. */
	UFUNCTION(CallInEditor, Category = "Tone Map|Presets")
	void SavePresetAs();

	/** Open a file browser to load a preset from any location. */
	UFUNCTION(CallInEditor, Category = "Tone Map|Presets")
	void LoadPresetBrowse();
#endif

	/** Save all settings to an absolute file path. */
	UFUNCTION(BlueprintCallable, Category = "Tone Map|Presets")
	bool SavePresetToPath(const FString& FilePath) const;

	/** Load settings from an absolute file path. */
	UFUNCTION(BlueprintCallable, Category = "Tone Map|Presets")
	bool LoadPresetFromPath(const FString& FilePath);

	/** Returns the default directory where preset files are stored. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Tone Map|Presets")
	static FString GetPresetDirectory();

	/** Returns this component's raw blend contribution for a world-space view location. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Tone Map|Blending")
	float GetBlendWeightAtLocation(const FVector& ViewLocation) const;

	/** Rebuilds the transient GPU texture used by Custom Curve (Luminance). */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Tone Map|Film Curve|Custom")
	void RebuildCustomToneCurveTexture();

	UTexture2D* GetCustomToneCurveTexture() const { return CustomToneCurveTexture.Get(); }

	// =========================================================================
	// Helpers
	// =========================================================================

	/** Returns true when HSL adjustments are non-zero. */
	bool IsAnyHSLActive() const;

	/** Returns true when any tone-curve slider is non-zero. */
	bool IsAnyCurveActive() const;

protected:
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	void RegisterWithSubsystem();
	void UnregisterFromSubsystem();
#if WITH_EDITOR
	void BindCustomToneCurveUpdate();
	void UnbindCustomToneCurveUpdate();
	void HandleCustomToneCurveUpdated(UCurveBase* Curve, EPropertyChangeType::Type ChangeType);

	TWeakObjectPtr<UCurveFloat> BoundCustomToneCurve;
	FDelegateHandle CustomToneCurveUpdateHandle;
#endif
};
