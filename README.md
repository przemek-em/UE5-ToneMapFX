[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.6.1+-blue.svg)](https://www.unrealengine.com/)
[![License](https://img.shields.io/badge/License-Zlib-lightgrey.svg)](LICENSE)
[![Experimental](https://img.shields.io/badge/Status-Experimental-orange.svg)]()

# Tone Map FX

A post-process plugin for Unreal Engine 5.6 that brings **Photo RAW style color grading** directly into the engine viewport. Run it on top of UE's built-in tonemapper, or **fully replace** ACES with classic film curves (Hable, Reinhard, Durand, Fattal, AgX).
![Image](Screens/HighresScreenshot00041.jpg)

---

## Getting Started

1. **Enable the plugin** - *Edit -> Plugins -> Rendering -> Tone Map FX*
2. **Add the actor or component**
   - **Place Actors panel** - Search *"Tone Map FX"* and drag `AToneMapActor` into the scene
   - **Or** select any Actor -> Add Component -> *"Tone Map FX"*
3. **Tweak sliders** - All changes are visible in real time in the viewport
4. **Save / Load presets** - Use the *Presets* buttons in the Details panel to save and share `.txt` preset files
5. **Blueprint / C++ ready** - All properties are exposed for runtime control

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
- **AgX (Sobotka)** - Display rendering transform by Troy Sobotka. Inset matrix → log2 encoding → polynomial sigmoid tone curve → outset matrix. Preserves hue and saturation through highlight compression with minimal color clipping. Three creative looks: *None* (base), *Punchy* (vivid contrast), *Golden* (warm golden-hour). Configurable min/max EV encoding range.
- **HDR Saturation & Color Balance** - Pre-curve adjustments in linear HDR

### Auto-Exposure *(Replace Tonemapper Mode)*
- **Manual (None)** - No automatic exposure. UE's built-in exposure is disabled automatically; only the manual Exposure slider applies.
- **Engine Default** - UE's built-in eye adaptation remains active and passes exposure through.
- **Krawczyk** (experimental) - Scene key estimation with temporal adaptation that mimics human eye behavior (fast bright-adapt, slow dark-adapt). UE's built-in exposure is disabled automatically.

> **Exposure Independence:** When set to *Krawczyk* or *None*, ToneMapFX automatically neutralizes UE's entire exposure pipeline (forces `AEM_Manual`, zeros `AutoExposureBias`, disables physical camera exposure, and neutralizes local exposure contrast) so that only ToneMapFX controls the scene brightness.

### Bloom
Four bloom styles:

| Mode | What it does |
|------|--------------|
| **Standard** | Classic Gaussian blur glow |
| **Directional Glare** | Star/cross streaks from bright areas |
| **Kawase** | Progressive pyramid bloom (smooth, efficient) |
| **Soft Focus** | Dreamy full-scene glow |

**Compositing:** 7 blend modes (Screen, Overlay, Soft Light, Hard Light, Lighten, Multiply, Additive) - color tinting - saturation - highlight protection - quality controls

**Threshold:** Soft-knee quadratic threshold with configurable softness (eliminates hard cutoff circles around bright sources). MaxBrightness HDR clamp prevents banding/quantization rings from extreme values like the sun.

**Directional Glare** now supports configurable sample count (8–64) for quality/performance trade-off.

### Additional Lens Effects *(available in both modes)*

| Effect | Description |
|--------|-------------|
| **Ciliary Corona** | Diffraction spike streaks radiating from very bright light sources. Configurable spike count (2-16), length, threshold, and intensity. Check also very similar kawase bloom setting. |
| **Lenticular Halo** | Physically-based scattering ring around bright sources. Uses chromatic dispersion - red channel sampled at a slightly larger radius, blue at a slightly smaller radius - producing a violet-inside/red-outside iridescent ring matching real lens coating behavior. Configurable radius, thickness, threshold, intensity, and tint. Can be used in some specific cases. |

### Vignette
Screen-space darkening or lightening from edges with full creative control.

- **Shape** - Circular (radial) or Square (per-edge Chebyshev distance)
- **Falloff** - 5 curves: Linear, Smooth (smoothstep), Soft (smootherstep), Hard (sqrt), Custom (power exponent)
- **Signed intensity** - Positive darkens edges, negative lightens edges
- **Alpha texture mask** - Use any texture channel (R/G/B/A) as a custom vignette shape
- **Texture-only mode** - Bypass procedural vignette entirely; scene is multiplied by texture mask
- Blue-noise dithering eliminates banding in smooth gradients

### LUT (Color Grading Look-Up Table)
Apply standard UE LUT textures as a final color-grade lookup after all ToneMapFX processing.

- Supported resolutions: **256×16** (16³), **1024×32** (32³), **4096×64** (64³)
- 2D-unwrapped 3D texture sampling with bilinear interpolation between slices
- Intensity slider to blend between original and graded color

### Presets (Save / Load)
Save and load all ToneMapFX settings to `.txt` files using OS native file dialogs.

- **Save Preset As** / **Load Preset** - Buttons in the actor and component Details panel
- `SavePresetToPath()` / `LoadPresetFromPath()` - Blueprint-callable API
- Reflection-based serialization — automatically handles all property types including enums, colors, textures, and vectors
- Presets are forward/backward compatible — unknown properties are skipped gracefully
- Default directory: `Saved/ToneMapFX/`

### Engine Overrides
- **Disable Unreal Bloom** - Zeros UE's `BloomIntensity` to prevent double-bloom when using ToneMapFX bloom (enabled by default)
- **Automatic Exposure Neutralization** - When exposure mode is *Krawczyk* or *None*, UE's entire exposure system is disabled (see Auto-Exposure section)

### Camera Settings
Physical camera model - **ISO**, **Shutter Speed**, **Aperture** - for exposure derived from real-world camera parameters.


---

## Compiling

```bat
.\Engine\Build\BatchFiles\Build.bat UnrealEditor Win64 Development -Plugin="YourDriveLetter\UnrealEngine-5.6.1-release\Engine\Plugins\Experimental\ToneMapFX\ToneMapFX.uplugin"
```
---

## Roadmap

- [x] ~~AgX tonemapper~~ *(done — AgX with Punchy/Golden looks)*
- [x] ~~Vignette~~ *(done — Circular/Square, 5 falloffs, texture mask)*
- [x] ~~LUT import~~ *(done — 16³/32³/64³ LUT color grading)*
- [x] ~~Bloom bugfixes~~ *(done — soft-knee threshold, MaxBrightness clamp, PF_FloatRGBA)*
- [ ] Bounding box - multiple postprocess actors, blending across them
- [ ] Additional RGB curves
- [ ] Sharpen, additional mode for pixel radius
- [ ] Texture overlay
- [ ] Custom light shafts

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

### AgX Display Rendering Transform
Based on Troy Sobotka's open-source AgX display rendering pipeline. Scene-linear RGB is transformed through an inset matrix (working space rotation), log2-encoded across a configurable EV range, shaped by a polynomial sigmoid tone curve, then transformed back through an outset matrix. The sigmoid preserves hue and saturation through highlight compression with minimal color clipping — a key advantage over ACES. Creative looks (Punchy, Golden) are applied as post-curve ASC-CDL-style transforms.

- **[Troy Sobotka — AgX (GitHub)](https://github.com/sobotka/AgX)**
- **[AgX / Troy Sobotka — Blender Documentation](https://docs.blender.org/manual/en/latest/render/color_management/color_spaces.html)**
