# Scaled Scope

Keeps the sniper scope overlay in its authored 4:3 proportions on widescreen
resolutions instead of stretching it with the display aspect ratio.

Addresses issue #32 (`Correct scope overlay for widescreen`).

## What it changes

- Every `cScope::Draw` overlay element — tube, reticle, crosshair bars, arrows,
  ammo gauge, vignette and the magnification readout — is remapped from the
  full-screen layout into a centred 4:3 frame. On wider displays the scope keeps
  its normal vertical scale, with black bars filling the unused left/right strip.
  On displays narrower than 4:3 the bars move to the top and bottom.
- The magnification readout (`x 06`) is remapped with the rest of the overlay
  and scales with the vertical scope size, so it stays legible at 1080p and above
  instead of staying at stock 640×480 pixel size.
- At 640×480 / 4:3 the stock layout is unchanged apart from the readout
  following the resolution scale.

## Hooks

- **cScope_Draw** (`SoF.exe`, `0x00006EA0`)
  - `hkcScope_Draw()` - marks the 4:3 virtual frame and lets the engine draw,
    then paints the side/top bars.
- The scope's function start (`0x6EA0`) is listed in
  `funcmaps/SoF.exe.json`. Without it the callsite classifier cannot resolve
  `cScope::Draw`, so the shared draw hooks cannot identify or scale its output.

## Integration

- While `scaled_scope_active()` is true, the shared `Draw_StretchPic` hook
  remaps each quad about the screen centre by the 4:3 frame ratio.
- The shared `R_DrawFont` hook remaps the readout's screen position and forces
  the `FontCaller::ScopeDraw` vertex scale (`virtualHeight / 480`).

## Dependencies

- `scaled_ui_base` (added automatically because the feature name starts with
  `scaled_`) provides the shared draw hooks and screen-size state.
