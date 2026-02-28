# ToneMapFX — Changelog 01.03.2026

## New Features

### AgX Tonemapper (Sobotka)
- Added **AgX display rendering transform** as a new Film Curve option (`EToneMapFilmCurve::AgX`, mode index 6).
- Implements Troy Sobotka's AgX pipeline: **inset matrix → log2 encoding → polynomial sigmoid tone curve → outset matrix**.
- Three creative looks: **None** (base AgX), **Punchy** (increased contrast/saturation), **Golden** (warm golden-hour tint).
- Exposed properties: `AgXLook`, `AgXMinEV` (default −10), `AgXMaxEV` (default +6.5).
- New shader uniform `AgXParams` (FVector4f: MinEV, MaxEV, Look, unused).

### LUT Color Grading
- **New shader file**: `ToneMapLUT.usf` — post-tonemap, post-sRGB LUT lookup pass.
- Supports standard UE LUT textures: **256×16** (16³), **1024×32** (32³), **4096×64** (64³) unwrapped strips.
- 2D-unwrapped 3D texture sampling with bilinear interpolation between slices.
- Intensity slider (`LUTIntensity`, 0–1) for blending between original and graded color.
- **New C++ files**: `ToneMapLUT.cpp`, `ToneMapLUTShaders.h` (shader registration and parameter struct).
- Integrated as a **dedicated render pass** in the post-process chain, running after ToneMapProcess.

### Vignette System
- **New shader file**: `ToneMapVignette.usf` — full-featured screen-space vignette.
- Two shape modes: **Circular** (Euclidean distance) and **Square** (Chebyshev distance).
- Five falloff curves: **Linear**, **Smooth** (smoothstep), **Soft** (smootherstep), **Hard** (sqrt), **Custom** (power exponent).
- Signed intensity: positive darkens edges, negative lightens edges.
- **Alpha texture mask** support with configurable channel selection (R/G/B/A).
- Texture-only bypass mode (`bVignetteAlphaTextureOnly`) — no procedural vignette, texture mask only.
- Blue-noise dithering to eliminate banding in gradients.
- Aspect-ratio correction for consistent appearance on widescreen displays.
- **New C++ files**: `ToneMapVignette.cpp`, `ToneMapVignetteShaders.h`.
- Integrated as a **dedicated render pass**, running after LUT (or ToneMapProcess if no LUT).

### ToneMap Actor
- **New file**: `ToneMapActor.cpp` / `ToneMapActor.h` — `AToneMapActor`, a placeable actor wrapping `UToneMapComponent`.
- Searchable in Place Actors panel as "Tone Map FX".
- Forwards `SavePresetAs()` and `LoadPresetBrowse()` CallInEditor buttons from the component (UE doesn't auto-propagate these from sub-objects).

### Preset System (Save / Load)
- Full **reflection-based preset serialization** to `.txt` files via OS native file dialogs.
- `SavePresetAs()` — opens Save File dialog, writes all UPROPERTY values using `FProperty` iteration.
- `LoadPresetBrowse()` — opens Open File dialog, reads text line-by-line and applies via `ImportText`.
- `SavePresetToPath()` / `LoadPresetFromPath()` — Blueprint-callable path-based API.
- `GetPresetDirectory()` — returns default preset folder (`Saved/ToneMapFX/Presets/`).
- Handles special types: `FLinearColor`, `FVector4f`, enums, textures (asset path), booleans, numeric types.
- SoftFocus mode auto-selects SoftLight blend mode on preset load.
- Added `DesktopPlatform` module dependency for native file dialogs.

### Engine Override: Disable Unreal Bloom
- New property `bDisableUnrealBloom` (default: `true`).
- Zeros `FinalPostProcessSettings.BloomIntensity` in `SetupView` to prevent double-bloom when using ToneMapFX bloom.

### Automatic Exposure Independence
- When `AutoExposureMode` is set to **Krawczyk** or **None**, UE's built-in exposure system is automatically disabled:
  - Forces `AEM_Manual` exposure method.
  - Zeros `AutoExposureBias` (prevents Exposure Compensation leaking into PreExposure as `pow(2, bias)`).
  - Disables `AutoExposureApplyPhysicalCameraExposure`.
  - Neutralizes `LocalExposureHighlightContrastScale` and `LocalExposureShadowContrastScale` to 1.0.
- **Engine Default** leaves UE's eye-adaptation fully active and passes exposure through.
- Enum tooltips updated to clearly document exposure behavior per mode.

---

## Bug Fixes

### Bloom Cutoff Circle Artifacts
- **Root cause**: `PF_FloatR11G11B10` format + hard threshold step caused visible circle/banding around bright sources.
- **Fix**: Replaced all bloom render targets with `PF_FloatRGBA` format for full HDR precision.
- Added **soft-knee quadratic threshold** (`SoftKneeThreshold()` function in `ClassicBloomShaders.usf`):
  - 3-path BrightPass: SoftFocus bypass, soft-knee (quadratic), hard cutoff.
  - New uniforms: `ThresholdSoftness` (0=hard, 1=wide), `MaxBrightness` (HDR clamp).
- New properties: `BloomThresholdSoftness` (default 0.5), `BloomMaxBrightness` (default 1.0).
- `MaxBrightness` clamps extreme HDR spikes before blur, preventing quantization rings around the sun/emissives.

### Glare Streak Quality
- Replaced compile-time `STREAK_SAMPLES` define with **runtime `StreakSamples` uniform** (8–64).
- Changed `UNROLL` → `LOOP` with `MAX_STREAK_SAMPLES 64` cap and early `break` on zero weight.
- New property `GlareSamples` (default 16, range 8–64) with quality tiers: 8=fast, 16=default, 32=high, 48/64=ultra.

---

## Improvements

### Quality Settings Visibility
- Removed `EditConditionHides` from bloom quality properties (`DownsampleScale`, `BlurPasses`, `BlurSamples`, `bHighQualityUpsampling`).
- Properties now remain **visible but greyed out** when their edit condition is false, instead of being hidden entirely.
- `DownsampleScale` edit condition widened to apply to all bloom modes (was limited to Standard/SoftFocus).

### Default Value Changes
- `ToneSmoothing`: 50 → **100** (wider default blending between tonal zones).
- `HSLSmoothing`: 50 → **100** (wider default feathering between HSL ranges).
- `AutoExposureMode` default: **Engine Default** (for maximum compatibility with existing scenes).

### Category Renames
- "Tone Map|Lens Effects" → **"Tone Map|Additional Lens Effects"** (Ciliary Corona & Lenticular Halo section).

### Post-Process Chain Architecture
- LUT and Vignette are now **dedicated render passes** with proper intermediate texture routing:
  - ToneMapProcess → (optional) LUT → (optional) Vignette → Final Output.
  - Each pass creates an intermediate `FRDGTexture` only when needed.
  - Proper `FScreenTransform` UV mapping between passes.
- `FinalOutputTarget` concept introduced — last pass in the chain writes directly to it.

---

## Removed

### Debug Properties
- **Removed** `BlendAmount` (blend slider between original and processed).
- **Removed** `bSplitScreenComparison` (left/right split view toggle).
- **Removed** `bEnableDebugLogging`.
- Corresponding shader uniforms (`BlendAmount`, `bSplitScreen`) removed from `ToneMapShaders.h` and `ToneMapProcess.usf`.

---

## Files Changed

### New Files
| File | Description |
|------|-------------|
| `Shaders/Private/ToneMapLUT.usf` | LUT color grading pixel shader |
| `Shaders/Private/ToneMapVignette.usf` | Vignette pixel shader (procedural + texture mask) |
| `Source/Private/ToneMapActor.cpp` | AToneMapActor implementation |
| `Source/Private/ToneMapLUT.cpp` | LUT shader registration (IMPLEMENT_GLOBAL_SHADER) |
| `Source/Private/ToneMapVignette.cpp` | Vignette shader registration |
| `Source/Public/ToneMapActor.h` | AToneMapActor class declaration |
| `Source/Public/ToneMapLUTShaders.h` | FToneMapLUTPS shader parameter struct |
| `Source/Public/ToneMapVignetteShaders.h` | FToneMapVignettePS shader parameter struct |

### Modified Files
| File | Key Changes |
|------|-------------|
| `Shaders/Private/ClassicBloomGlare.usf` | Dynamic StreakSamples, LOOP instead of UNROLL, early break |
| `Shaders/Private/ClassicBloomShaders.usf` | SoftKneeThreshold function, ThresholdSoftness/MaxBrightness uniforms, 3-path BrightPass |
| `Shaders/Private/ToneMapProcess.usf` | AgX tonemapper (~120 lines), removed BlendAmount/bSplitScreen debug code |
| `Source/Private/ToneMapComponent.cpp` | Full preset system (+214 lines), SoftFocus auto-select |
| `Source/Private/ToneMapSubsystem.cpp` | SetupView overrides (bloom/exposure), PF_FloatRGBA, new params, LUT+Vignette pass chain, AgX uniforms |
| `Source/Public/ClassicBloomShaders.h` | ThresholdSoftness, MaxBrightness, StreakSamples shader parameters |
| `Source/Public/ToneMapComponent.h` | AgX/Vignette/LUT enums & properties, preset API, removed Debug section, updated tooltips |
| `Source/Public/ToneMapShaders.h` | AgXParams uniform, FilmCurveMode comment updated, removed BlendAmount/bSplitScreen |
| `ToneMapFX.Build.cs` | Added `DesktopPlatform` module dependency |

---

## New Enums

| Enum | Values | Purpose |
|------|--------|---------|
| `EAgXLook` | None, Punchy, Golden | Creative look for AgX tonemapper |
| `EVignetteMode` | Circular, Square | Vignette shape mode |
| `EVignetteFalloff` | Linear, Smooth, Soft, Hard, Custom | Vignette gradient curve |
| `EVignetteTextureChannel` | Alpha, Red, Green, Blue | Texture mask channel selection |

---

## New Properties Summary

| Property | Type | Default | Category |
|----------|------|---------|----------|
| `AgXLook` | EAgXLook | None | Film Curve\|AgX |
| `AgXMinEV` | float | −10.0 | Film Curve\|AgX |
| `AgXMaxEV` | float | +6.5 | Film Curve\|AgX |
| `BloomThresholdSoftness` | float | 0.5 | Bloom |
| `BloomMaxBrightness` | float | 1.0 | Bloom |
| `GlareSamples` | int32 | 16 | Bloom\|Directional Glare |
| `bEnableVignette` | bool | false | Vignette |
| `VignetteMode` | EVignetteMode | Circular | Vignette |
| `VignetteSize` | float | 30.0 | Vignette |
| `VignetteIntensity` | float | 50.0 | Vignette |
| `VignetteFalloff` | EVignetteFalloff | Smooth | Vignette |
| `VignetteFalloffExponent` | float | 2.0 | Vignette |
| `bVignetteUseAlphaTexture` | bool | false | Vignette |
| `VignetteAlphaTexture` | UTexture* | nullptr | Vignette |
| `VignetteTextureChannel` | EVignetteTextureChannel | Alpha | Vignette |
| `bVignetteAlphaTextureOnly` | bool | false | Vignette |
| `bEnableLUT` | bool | false | LUT |
| `LUTTexture` | UTexture* | nullptr | LUT |
| `LUTIntensity` | float | 1.0 | LUT |
| `bDisableUnrealBloom` | bool | true | Engine Overrides |
