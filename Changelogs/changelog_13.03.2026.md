# ToneMapFX Changelog

## Sharpening, Dithering, Banding Elimination & Bug Fixes

---

## Sharpening Feature

### Motivation

Post-tonemapping sharpening is a common request for cinematic and game workflows — the tone curve and LUT application can soften the image slightly, and a final sharpening pass restores perceived detail without touching the lighting pipeline. A dedicated 9-tap unsharp mask placed after the tone-mapping process (but before vignette and HDR encode) gives artists direct control over output crispness.

### New Features

- **Enable Sharpening** (`bEnableSharpening`) checkbox on `UToneMapComponent` — toggles the sharpening pass on/off.
- **Sharpen Amount** (`SharpenAmount`) — controls the intensity of the unsharp mask. Range 0–100, default 50. Only visible when sharpening is enabled.
- **Sharpen Radius** (`SharpenRadius`) — controls the sampling radius in pixels. Range 0.5–5.0, default 1.0. Only visible when sharpening is enabled.

### New Files

| File | Description |
|------|-------------|
| `Shaders/Private/ToneMapSharpen.usf` | 9-tap unsharp mask pixel shader. Samples center + 8 neighbors (cross + diagonals) at configurable radius, computes blur, then blends `Center + (Center - Blur) * Amount`. Includes DitherQuantization support for last-pass dithering. |
| `Source/ToneMapFX/Public/ToneMapSharpenShaders.h` | `FToneMapSharpenPS` global shader class declaration with parameters: `InputTexture`, `InputSampler`, `SharpenAmount`, `SharpenRadius`, `InputSize`, `DitherQuantization`. |
| `Source/ToneMapFX/Private/ToneMapSharpen.cpp` | `IMPLEMENT_GLOBAL_SHADER` registration for the sharpening pixel shader. |

### Pipeline Position

Sharpening is inserted after ToneMapProcess and before LUT:

```
ToneMapProcess → Sharpen → LUT → Vignette → HDR Encode
```

This placement ensures sharpening operates on the tone-mapped linear result before color grading (LUT) and vignette are applied.

---

## Banding Elimination

### Motivation

Visible color banding (posterization) in smooth gradients — especially skies and dark scenes — had three root causes:

1. **8-bit intermediate textures** — the plugin inherited SceneColor's format for intermediate render targets, which on many configurations was `PF_B8G8R8A8` (8 bits per channel). Passing tone-mapped results through 8-bit intermediates quantized gradients before they reached the final output.
2. **No PostProcess-mode dithering** — dithering was only applied in the ReplaceTonemap path; the PostProcess code path had no dithering at all.
3. **Weak vignette dithering** — the vignette pass used uniform random noise, which still produces visible banding patterns. Triangular-PDF dithering (two uniform samples subtracted) distributes quantization error more evenly.

Additionally, the initial per-pass dithering approach (every pass dithered independently) introduced cumulative noise. The correct approach is to dither only in the **last active pass** in the chain, just before the final write to the backbuffer.

### Changes

#### Float Intermediates

All intermediate render targets created by the subsystem now use `PF_FloatRGBA` (16-bit float per channel) instead of inheriting the scene color format. This eliminates any precision loss between passes:

- ToneMapProcess intermediate → `PF_FloatRGBA`
- Sharpen intermediate → `PF_FloatRGBA`
- LUT intermediate → `PF_FloatRGBA`
- Vignette intermediate (PreHDREncode) → `PF_FloatRGBA`

#### DitherQuantization Parameter

The previous `bEnableDithering` (bool) uniform has been replaced with `DitherQuantization` (float) across all shader passes:

- **0.0** — dithering disabled (not the last pass, or user toggled off)
- **1/255 ≈ 0.00392** — 8-bit quantization step (default for all SDR outputs)
- **1/1023 ≈ 0.000978** — 10-bit quantization step (used for HDR PQ output)

The subsystem determines which pass is the **last active pass** in the current frame's pipeline and sets `DitherQuantization` to the appropriate quantum only for that pass. All earlier passes receive 0.0.

#### Quantum Auto-Detection

For SDR output, the quantum is always `1/255` regardless of backbuffer format. This is intentional — even when UE uses a 10-bit backbuffer (`PF_A2B10G10R10`), most consumer monitors are 8-bit panels. The GPU/driver performs 10→8 bit conversion without dithering, so dithering at 1/1023 would be too subtle to mask the final 8-bit quantization. Using 1/255 is safe for both 8-bit and 10-bit displays.

For HDR PQ output (`OutputDevice >= 3`), the quantum is `1/1023` to match the 10-bit PQ encoding. For scRGB float output, dithering is disabled (quantum = 0) since float targets have no quantization.

#### Triangular-PDF Dithering in Vignette

The vignette shader's dithering has been upgraded from uniform random noise to **triangular-PDF dithering** (two IGN samples subtracted). This distributes quantization error symmetrically around zero, producing visually smoother gradients than uniform dithering.

#### Enable Dithering Toggle

- **Enable Dithering** (`bEnableDithering`) checkbox on `UToneMapComponent` in the Advanced category — allows users to disable dithering entirely if desired. Enabled by default.

### Modified Files for Dithering

| File | Changes |
|------|---------|
| `Shaders/Private/ToneMapProcess.usf` | Replaced `bEnableDithering` with `DitherQuantization` float. `ApplyDithering()` now takes quantum parameter. Added dithering to PostProcess code path (was missing). |
| `Shaders/Private/ToneMapSharpen.usf` | Added `DitherQuantization` uniform and dithering support. |
| `Shaders/Private/ToneMapLUT.usf` | Added `DitherQuantization` uniform, `IGNoise()` function, and dithering after LUT blend. |
| `Shaders/Private/ToneMapVignette.usf` | Added `DitherQuantization` uniform. Upgraded from uniform to triangular-PDF dithering (two `IGNoise` samples). Dithering conditional on `DitherQuantization > 0`. |
| `Shaders/Private/ToneMapHDREncode.usf` | Added `DitherQuantization` uniform, `IGNoise()` function, and dithering in both PQ and SDR paths. |
| `Source/ToneMapFX/Public/ToneMapShaders.h` | Replaced `SHADER_PARAMETER(uint32, bEnableDithering)` with `SHADER_PARAMETER(float, DitherQuantization)`. |
| `Source/ToneMapFX/Public/ToneMapSharpenShaders.h` | Added `SHADER_PARAMETER(float, DitherQuantization)`. |
| `Source/ToneMapFX/Public/ToneMapLUTShaders.h` | Added `SHADER_PARAMETER(float, DitherQuantization)`. |
| `Source/ToneMapFX/Public/ToneMapVignetteShaders.h` | Added `SHADER_PARAMETER(float, DitherQuantization)`. |
| `Source/ToneMapFX/Public/ToneMapHDREncode.h` | Added `SHADER_PARAMETER(float, DitherQuantization)`. |
| `Source/ToneMapFX/Private/ToneMapSubsystem.cpp` | Last-pass detection logic, quantum computation, `PF_FloatRGBA` intermediates, per-pass `DitherQuantization` wiring. |

---

## Krawczyk Auto-Exposure Flickering Fix

### Problem

The Krawczyk tone-mapping operator uses temporal adaptation (`LastLuminance` blended toward current luminance over time). During hitches or long frame times (e.g., shader compilation stalls, level streaming), `DeltaTime` spikes to large values, causing the luminance adaptation to overshoot dramatically. This produced visible brightness flickering on the next normal frame.

### Fix

DeltaTime is now clamped to a maximum of 66 ms (~15 FPS) before being used in the Krawczyk adaptation calculation:

```cpp
LastDeltaTime = FMath::Min((float)FApp::GetDeltaTime(), 0.066f);
```

This prevents adaptation overshoot during hitches while allowing normal frame-to-frame adaptation at any reasonable frame rate.

---

## Camera Exposure Overhaul

### Changes

#### ShutterSpeed → ShutterSpeedDenominator

The `ShutterSpeed` property has been renamed to `ShutterSpeedDenominator` to accurately represent the **1/X** shutter speed notation used in photography. The value is the denominator — e.g., a value of 100 means 1/100s exposure time.

- Range: 1–8000 (covers 1/1s to 1/8000s)
- Default: 100
- Tooltip updated: *"Shutter speed denominator (1/X seconds). Standard stops: 30, 60, 100, 125, 250, 500, 1000, 2000, 4000, 8000"*

#### EV100 Inversion Fix

The camera exposure EV calculation was inverted — increasing ISO or opening the aperture was making the image darker instead of brighter. The formula has been corrected:

**Before (incorrect):**
```cpp
CameraEV = EV100 - ReferenceEV;
```

**After (correct):**
```cpp
CameraEV = ReferenceEV - EV100;
```

Where `ReferenceEV` is the EV100 of a "standard" exposure (ISO 100, f/16, 1/100s ≈ EV 14.6). A lower EV100 (brighter exposure settings) now correctly produces a positive `CameraEV`, brightening the image.

#### Updated Tooltips

ISO and Aperture tooltips now list standard photographic stops for quick reference:
- **ISO**: *"Standard stops: 100, 200, 400, 800, 1600, 3200, 6400"*
- **Aperture**: *"Standard stops: 1.4, 2, 2.8, 4, 5.6, 8, 11, 16, 22"*

---

## Enable Toggles

### New Properties on UToneMapComponent

| Property | Type | Category | Description |
|----------|------|----------|-------------|
| `bEnableWhiteBalance` | bool | White Balance | Enables/disables white balance adjustment. Controls visibility of Temperature and Tint sliders via `EditCondition`. |
| `bEnableToneAdjustments` | bool | Tone Adjustments | Enables/disables tone adjustment controls. Controls visibility of Highlights, Midtones, Shadows, and Contrast sliders via `EditCondition`. |
| `bEnableDithering` | bool | Advanced | Enables/disables output dithering. Default: enabled. |

### Extended Parameter Ranges

| Property | Old Range | New Range |
|----------|-----------|-----------|
| `Temperature` | ±150 | ±1000 |
| `Tint` | ±150 | ±1000 |
| `Exposure` | ±10 | ±20 |

---

## Summary of All Modified Files

| File | Type | Change |
|------|------|--------|
| `Shaders/Private/ToneMapProcess.usf` | Modified | DitherQuantization parameter, PostProcess dithering |
| `Shaders/Private/ToneMapSharpen.usf` | **New** | 9-tap unsharp mask shader |
| `Shaders/Private/ToneMapLUT.usf` | Modified | DitherQuantization parameter, output dithering |
| `Shaders/Private/ToneMapVignette.usf` | Modified | DitherQuantization parameter, triangular-PDF dithering |
| `Shaders/Private/ToneMapHDREncode.usf` | Modified | DitherQuantization parameter, output dithering |
| `Source/ToneMapFX/Public/ToneMapComponent.h` | Modified | All new properties, range extensions, ShutterSpeedDenominator |
| `Source/ToneMapFX/Public/ToneMapShaders.h` | Modified | DitherQuantization float replaces bEnableDithering uint32 |
| `Source/ToneMapFX/Public/ToneMapSharpenShaders.h` | **New** | Sharpening shader parameter struct |
| `Source/ToneMapFX/Public/ToneMapLUTShaders.h` | Modified | Added DitherQuantization parameter |
| `Source/ToneMapFX/Public/ToneMapVignetteShaders.h` | Modified | Added DitherQuantization parameter |
| `Source/ToneMapFX/Public/ToneMapHDREncode.h` | Modified | Added DitherQuantization parameter |
| `Source/ToneMapFX/Private/ToneMapSubsystem.cpp` | Modified | DeltaTime clamp, sharpening pass, float intermediates, dithering quantum, EV fix, enable toggles |
| `Source/ToneMapFX/Private/ToneMapSharpen.cpp` | **New** | Shader registration |
