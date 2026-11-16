# Scaled Scope

Keeps the sniper scope overlay in its authored 4:3 proportions on widescreen
resolutions instead of stretching it with the display aspect ratio.

Addresses issue #32 (`Correct scope overlay for widescreen`).

## What it changes

- The scope tube, reticle, arrows, bullet gauge, and vignette are sized from a
  centred 4:3 frame. On wider displays the scene and scope stay at the normal
  vertical scale, with black bars filling the unused left/right strip. On
  displays narrower than 4:3 the bars move to the top and bottom.
- The magnification readout (`x 06`) keeps its position inside the scope and
  scales with the vertical scope size, so it stays legible at 1080p and above
  instead of staying at stock 640×480 pixel size.
- At 640×480 / 4:3 the stock layout is unchanged apart from the readout
  following the resolution scale.

## Hooks

- **cScope_Draw** (`SoF.exe`, `0x00006EA0`)
  - `hkcScope_Draw()` - patches the scope's layout fields for a centred 4:3
    frame, lets the engine draw, then paints the side bars.

## Integration

- The shared `Draw_StretchPic` hook asks this feature to redirect the four
  `pics/scope/vinette` quads into the 4:3 frame.
- The shared `R_DrawFont` / vertex scaler identifies `cScope::Draw` text as
  `FontCaller::ScopeDraw` and applies the vertical scope scale to the
  magnification readout.

## Dependencies

- `scaled_ui_base` (added automatically because the feature name starts with
  `scaled_`) provides the shared draw hooks and screen-size state.
