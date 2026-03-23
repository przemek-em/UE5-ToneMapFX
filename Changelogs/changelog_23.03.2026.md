# ToneMapFX Changelog

## SMAA Compatibility Fix

---

## Problem

When ToneMapFX was active in **ReplaceTonemap** mode, UE 5.7's SMAA anti-aliasing pipeline broke:
- SMAA was not rendering (no visible anti-aliasing)
- SMAA debug visualization (`r.SMAA.DebugVisualization 1`) showed corrupted screen UVs
- Green/uninitialized pixels appeared at viewport edges in non-fullscreen windows

### Root Cause

Two issues in the output texture creation:

1. **Wrong texture extent** — The intermediate output texture was created with `ViewportSize` (= `ViewRect.Size()`, e.g. 1537×843) as its extent. The engine's scene buffers use `QuantizeSceneBufferSize` which rounds up to multiples of 8 (e.g. 1544×848). SMAA computes `RTMetrics` (reciprocal texel size for UV offset calculations) from `Inputs.SceneColor.Texture->Desc.Extent`. When ToneMapFX returned a texture with un-quantized extent, SMAA's `QuantizeSceneBufferSize` produced different values, causing all three SMAA passes (edge detection, blending weights, neighborhood blending) to use wrong UV offsets.

2. **Wrong pixel format** — The output texture inherited `SceneColor.Texture->Desc.Format`, which without `PropagateAlpha` could be `PF_FloatR11G11B10` (10/11-bit). With `bForceFP16Pipeline` enabled, the format was already `PF_FloatRGBA` — but the code wasn't explicit about this, making it fragile.

---

## Fix

### 1. Save Original Scene Color Extent (line 237)

```cpp
const FIntPoint OriginalSceneColorExtent = SceneColor.Texture->Desc.Extent;
```

Captured at function entry before bloom/lens processing modifies `SceneColor`. This preserves the engine's quantized extent.

### 2. Output Texture Uses Quantized Extent (line 1399)

```cpp
OutputTarget = FScreenPassRenderTarget(
    GraphBuilder.CreateTexture(
        FRDGTextureDesc::Create2D(
            OriginalSceneColorExtent, PF_FloatRGBA,  // was: ViewportSize, SceneColor.Texture->Desc.Format
            FClearValueBinding::Black,                // was: FClearValueBinding::None
            TexCreate_ShaderResource | TexCreate_RenderTargetable),
        TEXT("ToneMap.Output")),
    FIntRect(0, 0, ViewportSize.X, ViewportSize.Y),
    ERenderTargetLoadAction::EClear);                        // was: ERenderTargetLoadAction::ENoAction
```

| Aspect | Before | After | Why |
|--------|--------|-------|-----|
| Extent | `ViewportSize` | `OriginalSceneColorExtent` | Matches engine's quantized buffer size — SMAA's `QuantizeSceneBufferSize` is a no-op, RTMetrics are correct |
| Format | `SceneColor.Texture->Desc.Format` | `PF_FloatRGBA` | Explicit FP16 — consistent with `bForceFP16Pipeline`, prevents banding |
| Clear | `None` / `ENoAction` | `Black` / `EClear` | Clears gap pixels between ViewRect and extent edge to black (prevents green artifacts in non-fullscreen viewports) |

### 3. PropagateAlpha Comment Update (line 67)

Comment-only change documenting SMAA compatibility — `r.PostProcessing.PropagateAlpha` is set once per frame on the game thread, so the `FAlphaChannelDim` shader permutation is stable across all SMAA passes.

---

## How SMAA Works With ToneMapFX Now

1. **OverrideOutput valid** (no SMAA/FXAA active) → ToneMapFX writes directly to the backbuffer via `Inputs.OverrideOutput`. No change from before.
2. **OverrideOutput invalid** (SMAA or FXAA enabled) → ToneMapFX creates an intermediate `PF_FloatRGBA` texture with the engine's quantized extent, renders into it, and returns it as `SceneColor`. SMAA receives this texture with correct geometry, computes correct RTMetrics, and processes it normally.

---

## Modified Files

| File | Changes |
|------|---------|
| `ToneMapSubsystem.cpp` | Save `OriginalSceneColorExtent` at entry; output texture uses quantized extent + `PF_FloatRGBA` + `EClear`; updated PropagateAlpha comment |
