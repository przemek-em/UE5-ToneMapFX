[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.6.1+-blue.svg)](https://www.unrealengine.com/)
[![License](https://img.shields.io/badge/License-Zlib-lightgrey.svg)](LICENSE)
[![Experimental](https://img.shields.io/badge/Status-Experimental-orange.svg)]()

# Tone Map FX

A post-process plugin for Unreal Engine 5.6 that brings **Photo RAW style color grading** directly into the engine viewport. Run it on top of UE's built-in tonemapper, or **fully replace** ACES with classic film curves (Hable, Reinhard, Durand, Fattal).
![Image](Screens/HighresScreenshot00041.jpg)

---

## Getting Started

1. **Enable the plugin** - *Edit -> Plugins -> Rendering -> Tone Map FX*
2. **Add the component** - Add or select any Actor -> Add Component -> *"Tone Map FX"*
3. **Tweak sliders** - All changes are visible in real time in the viewport
4. **Blueprint / C++ ready** - All properties are exposed for runtime control

---

## Two Operating Modes

### Post-Process Mode *(default)*
Runs **after** UE's built-in tonemapper on the already-processed LDR image. Safe, compatible, zero setup. Choose where to inject: after Tonemap, Motion Blur, FXAA, or at the very end.

### Replace Tonemapper Mode
**Replaces UE's entire ACES pipeline.** Takes raw HDR scene color and maps it through a film curve. Eliminates ACES desaturation, glow artifacts, and red-shift. Full control over the HDR-to-LDR conversion with custom bloom and auto-exposure.

---

## Features

### White Balance
- **Temperature** - Cool (blue) <-> Warm (amber)
- **Tint** - Green <-> Magenta

### Tone
- **Exposure** - Photographic stops (+/-5 EV)
- **Contrast** - With adjustable midpoint pivot
- **Highlights / Shadows / Whites / Blacks** - Independent tonal band controls
- **Tone Smoothing** - How smoothly bands overlap

### Presence (experimental)
- **Clarity** - Local mid-tone contrast (with halo prevention)
- **Vibrance** - Smart saturation boost (protects skin tones)
- **Saturation** - Global saturation

### Dynamic Contrast (experimental)
- **Dynamic Contrast** - Multi-scale local contrast (Laplacian pyramid) with shadow/highlight protection
- **Correct Contrast** - Adaptive smart contrast that analyzes local brightness
- **Correct Color Cast** - Automatic color cast removal (Gray World method)

### Parametric Tone Curve
- **Highlights / Lights / Darks / Shadows** - Four-region curve for fine-tuning the tonal response

### HSL (Per-Color Adjustments)
Selective **Hue**, **Saturation**, and **Luminance** control for 8 color ranges:

Reds / Oranges / Yellows / Greens / Aquas / Blues / Purples / Magentas

Smooth feathering between adjacent ranges prevents hard color boundaries.

### Film Curves *(Replace Tonemapper Mode)*
- **Hable** - Filmic curve with 7 adjustable parameters (shoulder, linear, toe, white point)
- **Reinhard (Luminance)** - Preserves hue and saturation
- **Reinhard-Jodie** - Hybrid with subtle highlight desaturation
- **Reinhard (Standard)** - Classic per-channel with extended white point
- **Durand-Dorsey 2002 (Bilateral)** - Bilateral-filter tone mapping. Decomposes the scene log-luminance into a *base layer* (large-scale illumination) and a *detail layer* (fine structures). Compresses only the base layer, then recombines - preserving micro-contrast while reducing overall dynamic range. Configurable spatial sigma, range sigma, base compression factor, and detail boost.
- **Fattal et al. 2002 (Gradient Domain)** (experimental) - Gradient-domain tone mapping. Attenuates large luminance gradients while leaving small ones intact. Implemented as a 4-pass RDG pipeline: attenuated gradient field -> divergence field -> iterative Jacobi Poisson solver -> tone-mapped reconstruction. Seeded from log-luminance for correct partial-convergence behavior. Configurable alpha/beta attenuation, noise floor, output saturation, and solver iteration count.
- **HDR Saturation & Color Balance** - Pre-curve adjustments in linear HDR

### Auto-Exposure *(Replace Tonemapper Mode)*
- **Manual** - No automatic adjustment
- **Engine Default** - UE's built-in eye adaptation
- **Krawczyk** - Scene key estimation with temporal adaptation that mimics human eye behavior (fast bright-adapt, slow dark-adapt)

### Bloom
Four bloom styles:

| Mode | What it does |
|------|--------------|
| **Standard** | Classic Gaussian blur glow |
| **Directional Glare** | Star/cross streaks from bright areas |
| **Kawase** | Progressive pyramid bloom (smooth, efficient) |
| **Soft Focus** | Dreamy full-scene glow |

**Compositing:** 7 blend modes (Screen, Overlay, Soft Light, Hard Light, Lighten, Multiply, Additive) - color tinting - saturation - highlight protection - quality controls

### Lens Effects *(available in both modes)*

| Effect | Description |
|--------|-------------|
| **Ciliary Corona** | Diffraction spike streaks radiating from very bright light sources. Configurable spike count (2-16), length, threshold, and intensity. Check also very similar kawase bloom setting. |
| **Lenticular Halo** | Physically-based scattering ring around bright sources. Uses chromatic dispersion - red channel sampled at a slightly larger radius, blue at a slightly smaller radius - producing a violet-inside/red-outside iridescent ring matching real lens coating behavior. Configurable radius, thickness, threshold, intensity, and tint. Can be used in some specific cases. |

### Camera Settings
Physical camera model - **ISO**, **Shutter Speed**, **Aperture** - for exposure derived from real-world camera parameters.

### Debug
- **Blend Amount** - Crossfade original <-> processed
- **Split Screen** - Side-by-side comparison (left = original, right = processed)
- **Debug Logging** - Print shader parameters to log

---

## Changelog

### 18.02.2026

#### New Features
- **Durand-Dorsey 2002 bilateral tone mapping** - Full 3-pass RDG implementation (log-lum -> bilateral base layer -> reconstruct). Correct mid-grey anchoring formula ensures the base layer is properly compressed and smooth areas are anchored to mid-grey.
- **Fattal et al. 2002 gradient-domain tone mapping** - Full 4-pass RDG implementation (gradient attenuation -> divergence -> Jacobi Poisson solve -> reconstruct). Seeded from log(lum) for correct partial-convergence. Reconstruction uses ratio = exp(I - logLumIn) clamped to [0.02, 8] - smooth areas get ratio ~1, contrast edges get ratio <1 (attenuation).
- **Ciliary Corona lens effect** - New lens effect producing diffraction spike streaks from very bright light sources. Configurable spike count (2-16), length, threshold, and intensity.
- **Lenticular Halo lens effect** - New physically-based scattering ring around bright sources with chromatic dispersion: each RGB channel is sampled at a distinct radius (R at radius + 0.6*thickness, B at radius - 0.6*thickness) producing a violet-inside/red-outside iridescent ring instead of a flat scene copy. 32 angular x 5 radial Gaussian samples eliminate polygon banding artifacts.

#### Bug Fixes
- **Gaussian blur echo / ghost artifact** - Both ToneMapBlur.usf (Clarity) and ClassicBloomBlur.usf were using BlurRadius as the *step distance* between sparse taps rather than as Gaussian sigma, causing aliased skipped-texel sampling. Both shaders now use a dense per-texel Gaussian kernel (sigma = BlurRadius, samples every texel out to 3*sigma, properly normalized).

---

## Compiling

```bat
.\Engine\Build\BatchFiles\Build.bat UnrealEditor Win64 Development -Plugin="YourDriveLetter\UnrealEngine-5.6.1-release\Engine\Plugins\Experimental\ToneMapFX\ToneMapFX.uplugin"
```
---

## Roadmap

- [ ] Bounding box - multiple postprocess actors, blending across them
- [ ] Additional AgX tonemapper
- [ ] Additional RGB curves
- [ ] Vignette
- [ ] Sharpen, additional mode for pixel radius
- [ ] Texture overlay
- [ ] Custom light shafts
- [ ] LUT import

---

## References & Further Reading

### Tonemapping Operators
- **[Tonemapping - 64.github.io](https://64.github.io/tonemapping/)** - Excellent overview of tonemapping operators including Reinhard, Reinhard-Jodie, and Hable (Uncharted 2). This plugin's film curve implementations are based on the formulas described here.

### Hable (Uncharted 2) Filmic Curve
John Hable's filmic tonemapping curve, originally developed for *Uncharted 2*, uses a parametric function with 6 constants (Shoulder Strength, Linear Strength, Linear Angle, Toe Strength, Toe Numerator, Toe Denominator) plus a White Point. It produces a natural-looking film response with controllable shoulder rolloff, a linear middle region, and a lifted toe - closely matching how real film stock responds to light.

- **[John Hable - Filmic Tonemapping Operators (GDC 2010)](http://filmicworlds.com/blog/filmic-tonemapping-operators/)**

### Reinhard Tonemapping
Based on Reinhard et al., *"Photographic Tone Reproduction for Digital Images"* (SIGGRAPH 2002). The plugin implements three variants:
- **Standard** - Per-channel extended Reinhard: `L * (1 + L/Lw^2) / (1 + L)` where Lw is the white point.
- **Luminance** - Applied to luminance only, then RGB is rescaled by the luminance ratio. Preserves hue and saturation better than per-channel.
- **Jodie** - A hybrid by shadertoy user Jodie that interpolates between luminance-preserved and per-channel Reinhard. Produces subtle desaturation in bright areas.

### Krawczyk Auto-Exposure
Based on Krawczyk, Myszkowski & Seidel, *"Lightness Perception in Tone Reproduction for Interactive Walkthroughs"* (Computer Graphics Forum, 2005). The algorithm:
1. Measures the **geometric mean luminance** of the scene (log-average across a 16x16 sampling grid).
2. Estimates a **scene key** automatically: `key = 1.03 - 2 / (2 + log2(Lavg + 1))` - bright scenes get a lower key (darker exposure), dark scenes get a higher key (brighter exposure).
3. Applies **temporal adaptation** with asymmetric speed - faster when brightening (eye closing), slower when darkening (eye opening) - to simulate human visual adaptation.

- **[Krawczyk et al. 2005 (MPI Informatik)](https://resources.mpi-inf.mpg.de/hdr/lightness/krawczyk05eg.pdf)**

### Durand & Dorsey - Bilateral Tone Mapping
Based on Durand & Dorsey, *"Fast Bilateral Filtering for the Display of High-Dynamic-Range Images"* (SIGGRAPH 2002). Decomposes log-luminance into a bilateral-filtered base layer (large-scale illumination) and a residual detail layer. Only the base layer is compressed by a configurable factor; the detail layer is optionally boosted and recombined. This preserves local micro-contrast while reducing the scene's overall dynamic range - avoiding the flat look of global operators on scenes with wide luminance variation.

- **[Durand & Dorsey 2002 (SIGGRAPH)](https://people.csail.mit.edu/fredo/PUBLI/Siggraph2002/DurandBilateral.pdf)**
- **[Bilateral Filtering lecture notes - Brown CS129](https://cs.brown.edu/courses/cs129/2012/lectures/18.pdf)**

Key parameters: `SpatialSigma` - spatial reach of the bilateral filter; `RangeSigma` - edge sensitivity (smaller = sharper edge preservation); `BaseCompression` - compression applied to the base layer (lower = more dynamic range reduction); `DetailBoost` - scales the detail residual (>1.0 = enhanced local contrast).

### Fattal, Lischinski & Werman - Gradient-Domain Tone Mapping
Based on Fattal, Lischinski & Werman, *"Gradient Domain High Dynamic Range Compression"* (SIGGRAPH 2002). Operates in the gradient domain: attenuates large luminance gradients while preserving small ones, then solves for the output image via a Poisson equation. Large gradients (bright edges, light sources) are compressed; small gradients (fine detail, textures) pass through nearly unchanged.

The Poisson equation `Laplacian(I) = div(H)` is solved iteratively with Jacobi relaxation. The solver is seeded with `log(lum)` rather than the zero field, ensuring that even at low iteration counts the output is a valid tone-mapped luminance. Reconstruction: `ratio = exp(I_solved - log(lum_in))`, clamped to `[0.02, 8]` to prevent inversion or overflow.

- **[Fattal et al. 2002 (ACM DL)](https://dl.acm.org/doi/10.1145/566654.566573)**

Key parameters: `Alpha` - gradient magnitude reference threshold (lower = more uniform attenuation); `Beta` - attenuation exponent (lower = stronger compression); `FattalJacobiIterations` - solver iteration count (30 is a good default with logLum seeding); `FattalSaturation` - output chrominance scale.
