# ToneMapFX Changelog

## Major Feature Update and UE 5.7.3 Cleanup

---

## Overview

This update is a large rewrite of the newer ToneMapFX branch compared with the older plugin version. It expands Replace Tonemapper mode into a much more complete rendering path, adds HDR output support, custom curve assets, improved bloom controls, AMD CAS sharpening, better lens effects, actor blending, and several engine override fixes for Unreal's built-in post-process stack.

The changelog was generated from `changes.diff` in the plugin root.

---

## Highlights

- Added **True HDR Output** and **Paper White Nits** output modes for Replace Tonemapper.
- Added **custom CurveFloat luminance curves** baked to a GPU texture.
- Added **ACES Hill Fit** and **ACES Narkowicz Fit** film curves.
- Added **Per-Pixel / LUT processing paths** for color operations.
- Added **AMD CAS sharpening** alongside the legacy unsharp mask.
- Added **Bloom Blend Strength** for artist control over blend modes.
- Added **Bloom Color Mode**: Scene Color, Legacy Luminance, and Tint.
- Added **Soft Light Glow** bloom blend mode to preserve soft-light character without darkening.
- Added improved **lenticular halo** patterns: Ring / Arcs and Stretched Lines.
- Added **multi-actor blending** with global and bounding-box falloff modes.
- Added **Disable Unreal Motion Blur** engine override.
- Fixed UE local exposure neutralization so curves and detail strength cannot leak through.
- Hardened Shipping builds by guarding editor-only `DesktopPlatform`, `Slate`, and `SlateCore` dependencies.

---

## Replace Tonemapper

### HDR Output

Replace Tonemapper can now drive HDR displays without relying on UE's built-in tonemapper output encoding.

New controls:

- **HDR Output** toggle
- **HDR Output Mode**
  - Paper White Nits
  - True HDR Output
- **Auto Detect HDR Display**
- **Debug HDR Logging**
- **Paper White Nits**

Technical changes:

- Added `ToneMapHDREncode.usf` support for linear HDR input.
- Added `bLinearHDROutput` / `bInputIsLinearHDR` shader parameters across the tonemap, LUT, sharpen, vignette, and HDR encode paths.
- Added ST2084/PQ and scRGB-aware output behavior.
- Auto-toggles `r.HDR.EnableHDROutput` to match the effective HDR state.
- Debug logs now report requested/effective HDR state, display output format, display max nits, mode, and paper white.

### Film Curves

New film curve options:

- **ACES Hill Fit**
- **ACES Narkowicz Fit**
- **Custom Luminance Curve**

Custom luminance curves:

- Use a `CurveFloat` asset.
- Are sampled into a transient `PF_R32_FLOAT` texture.
- Use configurable EV range:
  - `CustomCurveMinEV`
  - `CustomCurveMaxEV`
  - `CustomCurveSampleCount`
- Rebuild after loading presets.
- Update automatically in editor when the curve asset changes.

### Tone and HSL Improvements

- Added configurable **HSL Mode**:
  - Smooth
  - Legacy
- Reworked hue range smoothing to reduce hard boundaries and color discontinuities.
- Added soft-light style lightness modification for smoother luminance edits.
- Improved tone curve behavior for linear HDR output.

---

## Bloom

### Bloom Color Mode

Bloom source color is now explicit:

- **Scene Color** - uses source scene color.
- **Legacy Luminance** - restores the older monochrome/luminance bloom behavior.
- **Tint** - multiplies bloom by the selected tint color.

The old `bUseSceneColor` boolean is kept as a deprecated compatibility field and migrates to the new enum.

### Blend Modes and Artist Controls

Bloom compositing now supports:

- Screen
- Overlay
- Soft Light
- Hard Light
- Lighten
- Multiply
- Additive
- Soft Light Glow

New control:

- **Bloom Blend Strength** - blends between the original scene and the selected blend-mode result.

This separates bloom generation from bloom compositing:

- `BloomIntensity` controls how much bloom energy exists.
- `BloomBlendStrength` controls how strongly the blend mode reshapes the scene.

### Highlight Protection

Highlight protection was reworked:

- The old `tanh` soft-clip could become unstable near certain protection values.
- Protection is now applied after bloom intensity, so high bloom intensity cannot bypass it.
- New soft limiter preserves color ratios and avoids hard white clipping.

### Bloom Quality and Stability

- Added `BloomMaxBrightness` as an HDR source peak clamp.
- Added soft-threshold behavior to reduce visible cutoff rings.
- Added max-brightness clamping to standard and Kawase bloom paths.
- Added configurable Directional Glare sample count.
- Added Soft Focus Glow behavior for bright, dreamy bloom without unwanted darkening.

---

## Lens Effects

### Lenticular Halo

The halo system was expanded beyond a simple circular ring.

New controls:

- `HaloPattern`
  - Ring / Arcs
  - Stretched Lines
- `HaloIrregularity`
- `HaloArcStretch`
- `HaloLineCount`
- `HaloLineThickness`

Shader changes:

- Added angular weighting and wave modulation.
- Added edge fading to reduce screen-edge artifacts.
- Added bidirectional line sampling for stretched halo lines.
- Added subtle veil accumulation for a smoother optical feel.
- Improved lens effect composition so strong effects compress more gracefully over bright scenes.

---

## Sharpening

Added **AMD CAS** as a sharpening method alongside the existing legacy unsharp mask.

New controls:

- `SharpenMethod`
  - Legacy Unsharp Mask
  - AMD CAS
- CAS input range handling
- Linear HDR input support

Licensing:

- Added `ThirdPartyNotices/FidelityFX-CAS.txt`.

---

## Processing Path and LUT

Added a dual processing-path architecture:

- **Per-Pixel** - full analytical shader path.
- **LUT** - combines color operations into a 3D LUT path where possible.

LUT path now supports:

- Film curves
- HSL controls
- Tone curves
- ACES fits
- Custom luminance curves
- Linear HDR output awareness

This improves performance options while keeping spatial effects such as bloom, clarity, dynamic contrast, lens effects, sharpening, vignette, and final HDR encoding outside the baked LUT path.

---

## Actor Blending

ToneMapFX components now support view-location blending.

New blending controls:

- `BlendMode`
  - Global
  - Bounding Box
- `BlendPriority`
- `BlendWeight`
- `BlendBoxExtent`
- `BlendFalloffDistance`
- `bShowBlendBounds`
- `BlendBoundsLineThickness`
- `ColorBlendSpace`
  - Legacy Linear RGB
  - Oklab

Editor visualization:

- ToneMapFX component now derives from `UPrimitiveComponent`.
- Bounding-box blend regions can be drawn in the viewport.
- Inner bounds and falloff bounds are both visualized.

The subsystem now resolves active components into a weighted settings snapshot before rendering.

---

## Engine Overrides

### Unreal Bloom

ToneMapFX directly zeros UE's `BloomIntensity` when **Disable Unreal Bloom** is enabled. This avoids double-bloom without relying on console commands.

### Unreal Motion Blur

Added **Disable Unreal Motion Blur**.

When enabled, ToneMapFX directly overrides:

```cpp
MotionBlurAmount = 0.0f;
MotionBlurMax = 0.0f;
```

This follows the renderer's own motion blur gate and avoids command/CVar dependency.

### Unreal Tone Curve and Exposure

Replace Tonemapper mode now neutralizes UE's filmic tone curve stack:

```cpp
ToneCurveAmount = 0.0f;
ExpandGamut = 0.0f;
BlueCorrection = 0.0f;
```

For ToneMapFX exposure modes `None` and `Krawczyk`, UE exposure is neutralized:

- `AutoExposureMethod = AEM_Manual`
- `AutoExposureBias = 0`
- physical camera exposure disabled
- local exposure scales set to neutral
- local exposure curves cleared
- local exposure detail strength reset to `1.0`

This fixes an edge case where UE local exposure could remain active through contrast curves or detail strength even if contrast scales were neutral.

### FP16 Pipeline

The FP16 pipeline override remains available and now has clearer behavior:

- Toggles `r.PostProcessing.PropagateAlpha`.
- Keeps post-process intermediates in `PF_FloatRGBA`.
- Helps prevent banding and quantization in smooth gradients.

---

## Presets

Preset save/load remains reflection-driven and now covers the expanded property set automatically.

Improvements:

- Unknown properties are skipped gracefully for forward/backward compatibility.
- Custom curve texture is rebuilt after preset load.
- Deprecated fields are preserved where needed for compatibility.

---

## Compatibility and Build Cleanup

### Shipping Build Hygiene

- `DesktopPlatform`, `Slate`, and `SlateCore` dependencies are now added only when compiling against the editor.
- Editor file dialog code remains behind `WITH_EDITOR`.

### SMAA / FXAA / Output Texture Safety

The newer rendering path includes output texture improvements for anti-aliasing compatibility:

- Uses quantized scene buffer extent where needed.
- Uses `PF_FloatRGBA` for intermediate outputs.
- Clears gap pixels between view rect and texture extent.
- Keeps returned `SceneColor` compatible with downstream SMAA / FXAA passes.

---

## Modified Areas

| Area | Main Files |
|------|------------|
| Bloom composite and bloom shaders | `ClassicBloomComposite.usf`, `ClassicBloomShaders.usf`, `ClassicBloomKawase.usf`, `ClassicBloomShaders.h` |
| Tone mapping and LUT path | `ToneMapProcess.usf`, `ToneMapCombineLUT.usf`, `ToneMapApplyLUT.usf`, `ToneMapShaders.h`, `ToneMapCombineLUTShaders.h` |
| HDR output | `ToneMapHDREncode.usf`, `ToneMapHDREncode.h`, `ToneMapSubsystem.cpp`, `ToneMapSubsystem.h` |
| Lens effects | `ToneMapLensHalo.usf`, `ToneMapLensComposite.usf`, `ToneMapLensEffects.h` |
| Sharpening | `ToneMapSharpen.usf`, `ToneMapSharpenShaders.h`, `ThirdPartyNotices/FidelityFX-CAS.txt` |
| Vignette | `ToneMapVignette.usf`, `ToneMapVignetteShaders.h` |
| Component properties, presets, blending bounds | `ToneMapComponent.h`, `ToneMapComponent.cpp` |
| Render orchestration and engine overrides | `ToneMapSubsystem.cpp`, `ToneMapSubsystem.h` |
| Build and metadata | `ToneMapFX.Build.cs`, `ToneMapFX.uplugin`, `cleanbuild_tonemapfx.ps1` |

