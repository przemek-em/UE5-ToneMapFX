[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.6.1+-blue.svg)](https://www.unrealengine.com/)
[![License](https://img.shields.io/badge/License-Zlib-lightgrey.svg)](LICENSE)
[![Experimental](https://img.shields.io/badge/Status-Experimental-orange.svg)]()

# Tone Map FX

A post-process plugin for Unreal Engine 5.6 that brings **Photo RAW / Lightroom-style color grading** directly into the engine viewport. Run it on top of UE's built-in tonemapper, or **fully replace** ACES with classic film curves (Hable, Reinhard).
![Image](Screens/HighresScreenshot00041.jpg)

---

## Getting Started

1. **Enable the plugin** — *Edit → Plugins → Rendering → Tone Map FX*
2. **Add the component** — Add or select any Actor → Add Component → *"Tone Map FX"*
3. **Tweak sliders** — All changes are visible in real time in the viewport
4. **Blueprint / C++ ready** — All properties are exposed for runtime control

---

## Two Operating Modes

### Post-Process Mode *(default)*
Runs **after** UE's built-in tonemapper on the already-processed LDR image. Safe, compatible, zero setup. Choose where to inject: after Tonemap, Motion Blur, FXAA, or at the very end.

### Replace Tonemapper Mode
**Replaces UE's entire ACES pipeline.** Takes raw HDR scene color and maps it through Hable or Reinhard film curves. Eliminates ACES desaturation, glow artifacts, and red-shift. Full control over the HDR-to-LDR conversion with custom bloom and auto-exposure.

---

## Features

### White Balance
- **Temperature** — Cool (blue) ↔ Warm (amber)
- **Tint** — Green ↔ Magenta

### Tone
- **Exposure** — Photographic stops (±5 EV)
- **Contrast** — With adjustable midpoint pivot
- **Highlights / Shadows / Whites / Blacks** — Independent tonal band controls
- **Tone Smoothing** — How smoothly bands overlap

### Presence (experimental)
- **Clarity** — Local mid-tone contrast (with halo prevention)
- **Vibrance** — Smart saturation boost (protects skin tones)
- **Saturation** — Global saturation

### Dynamic Contrast (experimental)
- **Dynamic Contrast** — Multi-scale local contrast (Laplacian pyramid) with shadow/highlight protection
- **Correct Contrast** — Adaptive smart contrast that analyzes local brightness
- **Correct Color Cast** — Automatic color cast removal (Gray World method)

### Parametric Tone Curve
- **Highlights / Lights / Darks / Shadows** — Four-region curve for fine-tuning the tonal response

### HSL (Per-Color Adjustments)
Selective **Hue**, **Saturation**, and **Luminance** control for 8 color ranges:

🔴 Reds · 🟠 Oranges · 🟡 Yellows · 🟢 Greens · 🩵 Aquas · 🔵 Blues · 🟣 Purples · 🩷 Magentas

Smooth feathering between adjacent ranges prevents hard color boundaries.

### Film Curves *(Replace Tonemapper Mode)*
- **Hable** — Filmic curve with 7 adjustable parameters (shoulder, linear, toe, white point)
- **Reinhard (Luminance)** — Preserves hue and saturation
- **Reinhard-Jodie** — Hybrid with subtle highlight desaturation
- **Reinhard (Standard)** — Classic per-channel with extended white point
- **HDR Saturation & Color Balance** — Pre-curve adjustments in linear HDR

### Auto-Exposure *(Replace Tonemapper Mode)*
- **Manual** — No automatic adjustment
- **Engine Default** — UE's built-in eye adaptation
- **Krawczyk** — Scene key estimation with temporal adaptation that mimics human eye behavior (fast bright-adapt, slow dark-adapt)

### Bloom
Four bloom styles:

| Mode | What it does |
|------|--------------|
| **Standard** | Classic Gaussian blur glow |
| **Directional Glare** | Star/cross streaks from bright areas |
| **Kawase** | Progressive pyramid bloom (smooth, efficient) |
| **Soft Focus** | Dreamy full-scene glow |

**Compositing:** 7 blend modes (Screen, Overlay, Soft Light, Hard Light, Lighten, Multiply, Additive) · color tinting · saturation · highlight protection · quality controls

### Camera Settings
Physical camera model — **ISO**, **Shutter Speed**, **Aperture** — for exposure derived from real-world camera parameters.

### Debug
- **Blend Amount** — Crossfade original ↔ processed
- **Split Screen** — Side-by-side comparison (left = original, right = processed)
- **Debug Logging** — Print shader parameters to log

### Compiling

```bat
.\Engine\Build\BatchFiles\Build.bat UnrealEditor Win64 Development -Plugin="YourDriveLetter\UnrealEngine-5.6.1-release\Engine\Plugins\Experimental\ToneMapFX\ToneMapFX.uplugin"
```

## References & Further Reading

### Tonemapping Operators
- **[Tonemapping — 64](https://64.github.io/tonemapping/)** — Excellent overview of tonemapping operators including Reinhard, Reinhard-Jodie, and Hable (Uncharted 2). This plugin's film curve implementations are based on the formulas described here.

### Hable (Uncharted 2) Filmic Curve
John Hable's filmic tonemapping curve, originally developed for *Uncharted 2*, uses a parametric function with 6 constants (Shoulder Strength, Linear Strength, Linear Angle, Toe Strength, Toe Numerator, Toe Denominator) plus a White Point. It produces a natural-looking film response with controllable shoulder rolloff, a linear middle region, and a lifted toe — closely matching how real film stock responds to light.

- **[John Hable — Filmic Tonemapping Operators (GDC 2010)](http://filmicworlds.com/blog/filmic-tonemapping-operators/)** — Original blog post describing the curve.

### Reinhard Tonemapping
Based on Reinhard et al., *"Photographic Tone Reproduction for Digital Images"* (SIGGRAPH 2002). The plugin implements three variants:
- **Standard** — Per-channel extended Reinhard: `L × (1 + L/Lw²) / (1 + L)` where Lw is the white point.
- **Luminance** — Applied to luminance only, then RGB is rescaled by the luminance ratio. Preserves hue and saturation better than per-channel.
- **Jodie** — A hybrid by shadertoy user Jodie that interpolates between luminance-preserved and per-channel Reinhard, using the per-channel result as the blend weight. Produces subtle desaturation in bright areas.

### Krawczyk Auto-Exposure
Based on Krawczyk, Myszkowski & Seidel, *"Lightness Perception in Tone Reproduction for Interactive Walkthroughs"* (Computer Graphics Forum, 2005). The algorithm:
1. Measures the **geometric mean luminance** of the scene (log-average across a 16×16 sampling grid).
2. Estimates a **scene key** automatically: `key = 1.03 − 2 / (2 + log₂(Lavg + 1))` — bright scenes get a lower key (darker exposure), dark scenes get a higher key (brighter exposure).
3. Applies **temporal adaptation** with asymmetric speed — faster when brightening (eye closing), slower when darkening (eye opening) — to simulate human visual adaptation.
**[Krawczyk Auto-Exposure doc](https://resources.mpi-inf.mpg.de/hdr/lightness/krawczyk05eg.pdf)**
