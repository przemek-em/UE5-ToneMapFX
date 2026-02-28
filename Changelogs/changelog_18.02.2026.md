# ToneMapFX — Changelog 01.03.2026

#### New Features
- **Durand-Dorsey 2002 bilateral tone mapping** - Full 3-pass RDG implementation (log-lum -> bilateral base layer -> reconstruct). Correct mid-grey anchoring formula ensures the base layer is properly compressed and smooth areas are anchored to mid-grey.
- **Fattal et al. 2002 gradient-domain tone mapping** - Full 4-pass RDG implementation (gradient attenuation -> divergence -> Jacobi Poisson solve -> reconstruct). Seeded from log(lum) for correct partial-convergence. Reconstruction uses ratio = exp(I - logLumIn) clamped to [0.02, 8] - smooth areas get ratio ~1, contrast edges get ratio <1 (attenuation).
- **Ciliary Corona lens effect** - New lens effect producing diffraction spike streaks from very bright light sources. Configurable spike count (2-16), length, threshold, and intensity.
- **Lenticular Halo lens effect** - New physically-based scattering ring around bright sources with chromatic dispersion: each RGB channel is sampled at a distinct radius (R at radius + 0.6*thickness, B at radius - 0.6*thickness) producing a violet-inside/red-outside iridescent ring instead of a flat scene copy. 32 angular x 5 radial Gaussian samples eliminate polygon banding artifacts.

#### Bug Fixes
- **Gaussian blur echo / ghost artifact** - Both ToneMapBlur.usf (Clarity) and ClassicBloomBlur.usf were using BlurRadius as the *step distance* between sparse taps rather than as Gaussian sigma, causing aliased skipped-texel sampling. Both shaders now use a dense per-texel Gaussian kernel (sigma = BlurRadius, samples every texel out to 3*sigma, properly normalized).
