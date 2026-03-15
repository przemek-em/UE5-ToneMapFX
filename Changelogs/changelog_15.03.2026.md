# ToneMapFX Changelog

## LUT Processing Path, FP16 Pipeline Override & Dither Quantization Control

---

## LUT Processing Path (Dual-Mode Architecture)

### Motivation

Every frame, the per-pixel shader evaluates ~14 color operations (WhiteBalance, Exposure, Contrast, HSL, Vibrance, Saturation, HDR Saturation, HDR Color Balance, Film Curve, Tone Curve, etc.) analytically for each screen pixel. On GPU-limited scenes this ALU cost can become significant. Baking all non-spatial operations into a compact 3D LUT replaces that per-pixel math with a single trilinear texture fetch — a classic ALU-for-bandwidth trade.

### New Features

- **Processing Path** enum (`EToneMapProcessingPath`) in Advanced section — choose between:
  - **Per-Pixel (Full Quality)** — existing behavior, every operation evaluated analytically per pixel
  - **LUT (Performance)** — non-spatial operations baked into a 32×32×32 3D LUT, sampled with one trilinear fetch per pixel
- Both paths produce virtually identical visual output. The LUT path trades ALU for texture bandwidth.

### New Files

| File | Description |
|------|-------------|
| `Shaders/Private/ToneMapCombineLUT.usf` | Bakes all non-spatial color operations into a 1024×32 (32³ unwrapped) `PF_FloatRGBA` LUT texture. Runs once per frame on a 1024×32 quad. Contains copies of all non-spatial functions from ToneMapProcess.usf. ReplaceTonemap path: log2 [-10, +6.5] EV encoding → full grading chain → Film Curve → LinearToSRGB. PostProcess path: direct [0,1] → grading (no Film Curve, no sRGB). |
| `Shaders/Private/ToneMapApplyLUT.usf` | Samples the baked LUT via manual trilinear interpolation (2 bilinear + lerp across Z slices), then applies spatial operations (Clarity, Dynamic Contrast, Correct Contrast, Correct Color Cast) post-LUT. Spatial blur textures are transformed through the LUT to match the output domain. |
| `Source/ToneMapFX/Public/ToneMapCombineLUTShaders.h` | Declares `FToneMapCombineLUTPS` (~25 uniforms for non-spatial params) and `FToneMapApplyLUTPS` (~35 params for LUT texture + spatial textures + transforms). Both use `DECLARE_GLOBAL_SHADER` + `SHADER_USE_PARAMETER_STRUCT`. |
| `Source/ToneMapFX/Private/ToneMapCombineLUT.cpp` | `IMPLEMENT_GLOBAL_SHADER` registration for both CombineLUT and ApplyLUT pixel shaders. |

### Operations Baked into LUT (14 total)

WhiteBalance, Exposure, ToneAdjustments, Contrast, HSL, Vibrance, Saturation, HDR Saturation, HDR Color Balance, Film Curve (Hable/Reinhard/AgX), Tone Curve, sRGB conversion.

### Operations Remaining Per-Pixel in ApplyLUT

Auto-Exposure, Bloom composite, Clarity, Dynamic Contrast, Correct Contrast, Correct Color Cast, Dithering.

### Pipeline Integration

The dual-path dispatch is controlled by `bUseLUTPath` in `ToneMapSubsystem.cpp`:

```
if (!bUseLUTPath)
    // Per-Pixel: existing single-pass ToneMapProcess
else
    // LUT: Step 1 → CombineLUT (1024×32), Step 2 → ApplyLUT (fullscreen)
```

---

## FP16 Pipeline Override (Force Full Precision)

### Motivation

UE's post-processing pipeline uses reduced-precision formats by default:
- **TAA/TSR output**: `PF_FloatR11G11B10` (11-11-10 float, 32bpp) — only 6/6/5 mantissa bits
- **Tonemapper SDR output**: `PF_A2B10G10R10` (10-bit integer) — 1024 discrete values per channel

This quantization introduces banding before ToneMapFX even receives the texture. Neither Per-Pixel nor LUT processing can recover lost precision.

### New Feature

- **Force FP16 Pipeline** (`bForceFP16Pipeline`) checkbox in Advanced section — **enabled by default**
- Toggles `r.PostProcessing.PropagateAlpha` CVar at runtime via `SetupView`
- Forces `PF_FloatRGBA` (FP16, 64bpp) through the entire post-processing chain:
  - TAA/TSR output: 11-11-10 → FP16 (10 mantissa bits per channel)
  - Tonemapper output: 10-bit integer → FP16
  - All post-process intermediates: 32bpp → 64bpp
- **No engine restart required** — the CVar is `ECVF_RenderThreadSafe`, read live every frame
- Follows the same CVar toggle pattern as the existing HDR Output toggle

### Trade-off

Doubles bandwidth of all post-processing passes (32bpp → 64bpp). On modern GPUs this is a small cost for significantly better gradient quality.

---

## Dither Quantization Control

### Motivation

The dithering system previously used a hardcoded quantum of `1/255` (8-bit). Users with 10-bit displays may want less noise (`1/1023`), while users fighting stubborn banding may want more aggressive values.

### New Feature

- **Dither Quantization** (`DitherQuantization`) float slider in Advanced section
- Range: 0.0 to 0.02, default `1/255 ≈ 0.00392`
- Only visible when **Enable Dithering** is checked (`EditCondition`)
- Typical values:
  - `1/255 ≈ 0.00392` — 8-bit quantum (default, safe for 8-bit and 10-bit displays)
  - `1/1023 ≈ 0.00098` — 10-bit quantum (minimal noise for 10-bit panels)
  - Higher values — stronger noise for aggressive banding removal
  - `0` — no dithering even when enabled
- The subsystem now reads `ActiveComp->DitherQuantization` instead of the hardcoded `1/255`

---

## Modified Files Summary

| File | Changes |
|------|---------|
| `ToneMapComponent.h` | Added `EToneMapProcessingPath` enum, `ProcessingPath` UPROPERTY, `DitherQuantization` float UPROPERTY, `bForceFP16Pipeline` bool UPROPERTY |
| `ToneMapSubsystem.cpp` | Added `#include "ToneMapCombineLUTShaders.h"`, `bUseLUTPath` dispatch logic, full LUT path (CombineLUT + ApplyLUT), `r.PostProcessing.PropagateAlpha` CVar toggle in `SetupView`, user-configurable `DitherQuantization` value |
| `ToneMapCombineLUT.usf` | New file — LUT bake shader |
| `ToneMapApplyLUT.usf` | New file — LUT apply + spatial ops shader |
| `ToneMapCombineLUTShaders.h` | New file — shader parameter struct declarations |
| `ToneMapCombineLUT.cpp` | New file — shader registration |
