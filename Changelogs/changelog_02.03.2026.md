# ToneMapFX Changelog

## HDR Output Support

### Motivation

ToneMapFX's tone-mapping operators — Hable, Reinhard, AgX, Durand, Fattal — are **SDR tone curves by design**. They compress a high-dynamic-range scene into the standard [0, 1] sRGB range, which is exactly what an SDR display expects. On an SDR monitor, no further work is needed.

However, when UE's ReplaceTonemap mode is active, the engine's built-in tonemapper (and its HDR output encoding) is completely bypassed. This means an HDR monitor receives sRGB-encoded values and has no way to interpret them as HDR — the image appears washed-out or clipped at the display's SDR compatibility level.

The HDR Output feature solves this by adding a **dedicated final encoding pass** that takes the plugin's sRGB output and re-encodes it into the display's native HDR format (ST2084/PQ for HDR10, scRGB for Windows HDR). The tone curves themselves remain unchanged — they still produce SDR tone-mapped results — but those results are then correctly presented on an HDR display at the user's chosen Paper White brightness level, utilizing the monitor's wider luminance range for accurate rendering.

### New Features

- **HDR Mode checkbox** (`bHDROutput`) on `UToneMapComponent` — enables HDR output encoding when running in ReplaceTonemap mode on an HDR monitor. Outputs ST2084 (PQ) or scRGB instead of sRGB, matching the display's expected HDR format. Has no effect in PostProcess mode (UE's built-in tonemapper already handles HDR encoding).

- **Paper White Nits** (`PaperWhiteNits`) property — controls how bright the tone-mapped white point appears on the HDR display. Range 80–500 cd/m², default 200. Only visible when HDR Output is enabled.

- **Automatic `r.HDR.EnableHDROutput` CVar toggle** — the plugin now automatically sets the engine's HDR output CVar to match the UI checkbox state each frame, so users don't need to manually toggle the console variable.

### New Files

| File | Description |
|------|-------------|
| `Shaders/Private/ToneMapHDREncode.usf` | Final-pass pixel shader that converts sRGB output to ST2084/PQ (HDR10) or scRGB (Windows HDR). Includes full sRGB-to-linear decode, PaperWhiteNits scaling, BT.709→BT.2020 color space conversion, and ST2084 PQ OETF. |
| `Source/ToneMapFX/Public/ToneMapHDREncode.h` | `FToneMapHDREncodePS` global shader class declaration with parameters: `OutputDeviceType`, `PaperWhiteNits`, `MaxDisplayNits`. |
| `Source/ToneMapFX/Private/ToneMapHDREncode.cpp` | `IMPLEMENT_GLOBAL_SHADER` registration for the HDR encode pixel shader. |

### Modified Files

| File | Changes |
|------|---------|
| `Source/ToneMapFX/Public/ToneMapComponent.h` | Added `bHDROutput` (bool) and `PaperWhiteNits` (float) properties with appropriate `EditCondition` metadata. |
| `Source/ToneMapFX/Public/ToneMapSubsystem.h` | Added `bCachedHDROutput` member to `FToneMapSceneViewExtension`. |
| `Source/ToneMapFX/Private/ToneMapSubsystem.cpp` | Multiple changes — see details below. |

### ToneMapSubsystem.cpp Details

**New includes:**
- `PostProcess/PostProcessTonemap.h` — for `GetTonemapperOutputDeviceParameters()` and `FTonemapperOutputDeviceParameters`.
- `ToneMapHDREncode.h` — for `FToneMapHDREncodePS`.

**SetupView (game thread):**
- Caches `bHDROutput` from the active component into `bCachedHDROutput`.
- Auto-toggles `r.HDR.EnableHDROutput` via `IConsoleManager::Get().FindConsoleVariable()` (static cached). Sets to 1 when ReplaceTonemap + HDR Output are both enabled, 0 otherwise.

**PostProcessPass_RenderThread:**
- Queries `GetTonemapperOutputDeviceParameters()` to determine if the display is actually in HDR mode (`OutputDevice >= 3`). Sets `bNeedHDREncode` accordingly.
- **Output target chain restructured:**
  - ToneMapProcess intermediate is now also created when `bNeedHDREncode` is true (not just for LUT/Vignette).
  - LUT output target creates an intermediate when HDR encode follows (even without Vignette).
  - **Vignette pass:** When `bNeedHDREncode`, Vignette writes to a new intermediate texture (`"ToneMap.PreHDREncode"`) instead of `FinalOutputTarget`. UV transform variables renamed from `FinalOut*` to `VigOut*` to reflect the actual target. When HDR is off, behavior is identical to before (writes directly to `FinalOutputTarget`).
  - After Vignette, `OutputTarget` is updated so the HDR encode pass reads from Vignette's output.
- **New HDR Encode pass** added as the absolute last pass, after Vignette. Reads from `OutputTarget`, writes to `FinalOutputTarget` (backbuffer). Passes `OutputDeviceType`, `PaperWhiteNits`, and `MaxDisplayNits` to the shader.

### Architecture Notes

The HDR encode pass is intentionally the **final** pass in the pipeline:

```
ToneMapProcess (sRGB) → LUT (sRGB) → Vignette (sRGB) → HDR Encode (ST2084/scRGB)
```

This ensures that LUT application and Vignette — which both expect sRGB [0,1] input — are not corrupted by HDR-encoded values (which can exceed 1.0 and use a non-linear PQ curve). The HDR encode pass only converts the final sRGB result to the display's native HDR format.
