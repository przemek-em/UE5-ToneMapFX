# ToneMapFX Changelog

## GT / Uchimura and Selectable Local Exposure — 09.10.2026

---

## Overview

This update ports the classic Uchimura tone mapper from the testing plugin into the release plugin and adds a local exposure selector. Users can leave local exposure off, use Unreal's standard Post Process Volume settings, or use the custom Durand prototype to recover shadows and compress highlights before their chosen global film curve.

Scope was checked against `ToneMapFX-prechanges` in the UE 5.8 workspace. Existing HDR output, bloom, sharpening, actor blending, and other film operators remain part of the earlier release; this entry describes the additions since that baseline.

---

## Highlights

- Added **GT / Uchimura (Classic)** with six adjustable curve parameters.
- Integrated Uchimura into **Per-Pixel** and **LUT** processing, presets, and actor blending.
- Added **Local Exposure Method**: **Off**, **Unreal (Post Process Volume)**, and **Durand (Prototype)**. Off is the default.
- Enabled Unreal **Bilateral** and **Exposure Fusion** local exposure in Replace Tonemapper mode.
- Added a custom Durand-style HDR exposure prepass with separate shadow/highlight controls and bounded adjustments in stops.
- Separated local exposure selection from global auto-exposure neutralization, allowing local exposure with **None**, **Engine Default**, or **Krawczyk**.
- Preserved earlier checkbox settings through component and preset migration.

---

## GT / Uchimura (Classic)

The release plugin now includes the same classic six-parameter Uchimura curve and defaults as `ToneMapFX-modified`:

| Control | Default |
|---------|---------|
| Max Brightness | 1.0 |
| Contrast | 1.0 |
| Linear Start | 0.22 |
| Linear Length | 0.4 |
| Black Tightness | 1.33 |
| Pedestal | 0.0 |

- Select it under **Film Curve** in **Replace Tonemapper** mode.
- Applies the curve per RGB channel after conversion from working color space to Rec.709.
- Uses the same validated parameters in the analytical shader and baked color LUT.
- Supports output above paper white with **True HDR Output**; Max Brightness is capped to available display headroom.
- Copies curve parameters into render settings and blends continuous values across actors.
- Adds `ThirdPartyNotices/GLSL-Tone-Map.txt` for the reference implementation used to cross-check the formula.

---

## Local Exposure Method

The final UI is **Tone Map > Local Exposure > Local Exposure Method**, replacing the earlier visible **Enable Unreal Local Exposure** checkbox.

| Method | Result |
|--------|--------|
| **Off** *(default)* | Neutralizes Unreal local exposure settings; global/manual exposure remains available. |
| **Unreal (Post Process Volume)** | Retains the volume's local exposure settings. The release plugin applies Bilateral or Exposure Fusion before its film curve in Replace Tonemapper mode. In regular Post-Process mode, Unreal handles local exposure normally. |
| **Durand (Prototype)** | Applies the plugin's custom local correction before the selected global film curve. Unreal local exposure is neutralized. |

For None/Krawczyk, the plugin still neutralizes Unreal's global auto exposure, bias, and physical camera exposure. That no longer forces the chosen local exposure method off. The Unreal bridge and custom method share exposure setup corresponding to the plugin's selected auto-exposure mode.

No engine source patch is required: integration lives in the plugin's C++ and shaders, with the Unreal bridge using existing engine shader functions and pass logic.

---

## Durand Local Exposure Prototype

The custom method reuses the existing Durand base/detail concept to apply a local exposure gain to HDR scene color. The selected global film curve remains active afterward; the existing Durand-Dorsey film operator is unchanged.

- Extracts working-space log2 luminance, filters horizontally and vertically, then applies a bounded RGB gain.
- Uses the original unfiltered luminance as the guide in both filter directions, including the center sample.
- Works at full viewport resolution, with no Gaussian base blend or low-resolution reconstruction.
- Preserves luminance detail, RGB ratios, alpha, pre-exposure, and HDR values above 1.
- Keeps plugin manual exposure downstream of the correction.
- Shares the HDR prepass between Per-Pixel and LUT processing.

New controls:

| Control | Default |
|---------|---------|
| Strength | 1.0 |
| Spatial Sigma (Pixels) | 16 |
| Edge Range (Stops) | 1.16 |
| Highlight Contrast | 0.85 |
| Shadow Contrast | 0.85 |
| Maximum Shadow Lift (Stops) | 1.0 |
| Maximum Highlight Darkening (Stops) | 1.0 |
| Midtone Protection (Stops) | 1.0 |
| Middle Grey Bias (Stops) | 0.0 |
| Protect Deep Shadows | On |

Strength 0 bypasses the custom passes. Contrast 1 is neutral; lower shadow contrast lifts dark regions, and lower highlight contrast darkens bright regions. Independent correction caps and midtone/deep-shadow protection keep the adjustment bounded.

The prototype requires Replace Tonemapper and is inactive when Durand or Fattal is selected as the final film operator. It uses four fullscreen passes plus exposure setup, with sampling support capped at 32 pixels on either side per axis. GPU profiling, motion-aware history, guided filtering, and custom exposure fusion remain future work.


---

## Presets and Actor Blending

- Saves the new selector, Durand controls, and Uchimura parameters through the existing reflection-based preset system.
- Migrates saved components using the earlier enabled Unreal checkbox to the Unreal selector value.
- Loads old checkbox-only text presets as Unreal/Off; the new selector takes precedence when both fields are present.
- Keeps the legacy checkbox serialized but hidden; newly saved presets use the selector.
- Blends continuous local exposure controls and Uchimura parameters across actor contributions. Local exposure method and the deep-shadow protection toggle follow the dominant component; custom Strength fades with eligible custom contributions.

---

## Validation

- UE 5.8 Win64 Development editor C++ module compilation and linking succeeded; generated UI metadata was checked.
- Uchimura formula, defaults, and parameter routing were checked against the testing plugin. Manual comparison confirmed no visual difference between the two versions.
- Initial manual testing of the Durand prototype confirmed useful shadow lifting without visible halos in the tested scene. GPU timings and temporal behavior have not yet been evaluated.

---

## Main Files

| Area | Files |
|------|-------|
| Uchimura curve | `ToneMapUchimura.h`, `ToneMapUchimura.cpp`, `ToneMapUchimura.ush` |
| Unreal local exposure bridge | `ToneMapLocalExposure.h`, `ToneMapLocalExposure.cpp`, `ToneMapLocalExposure.usf` |
| Custom Durand local exposure | `ToneMapCustomLocalExposure.h`, `ToneMapCustomLocalExposure.cpp`, `ToneMapCustomLocalExposure.usf` |
| Settings, presets, migration, and render routing | `ToneMapComponent.h`, `ToneMapComponent.cpp`, `ToneMapSubsystem.cpp` |
| Per-Pixel and LUT integration | `ToneMapShaders.h`, `ToneMapCombineLUTShaders.h`, `ToneMapProcess.usf`, `ToneMapCombineLUT.usf` |
| Documentation and attribution | `README.md`, `Docs/LocalExposurePrototype.md`, `Docs/CustomLocalExposureResearch.md`, `ThirdPartyNotices/GLSL-Tone-Map.txt` |
