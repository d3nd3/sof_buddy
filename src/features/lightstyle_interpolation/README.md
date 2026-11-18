# Light Style Interpolation

## Purpose

Smoothly interpolates animated light styles (flickering lights, pulsing lights,
etc.) between their 10 Hz animation frames instead of snapping from one value to
the next.

The stock engine `CL_RunLightStyles` picks `map[frame % length]` and copies that
value into the style's RGB value. This feature runs after that function and blends
the current map entry with the next one using the fractional part of the current
animation time, following the Quake `CL_RunLightStyles` interpolation approach.

Disabled by default, so stock behaviour is unchanged unless the option is enabled.

## Callbacks

- **PostCvarInit** (Post, Priority: 50)
  - `lightstyle_interpolation_PostCvarInit()` - Registers the feature CVar

## Hooks

- **CL_RunLightStyles** (Post, Priority: 100)
  - `lightstyle_interpolation_run_post_callback()` - Replaces the style values
    with interpolated values when the option is enabled

## Configuration

### CVars

- `_sofbuddy_lightstyle_interpolation` - Enable smooth light styles
  - `0` - Off (default, stock 10 Hz stepping)
  - `1` - On (interpolate between animation frames)

Saved to `base/sofbuddy.cfg` with `CVAR_SOFBUDDY_ARCHIVE`. Also exposed as
**F12 → Lighting → Smooth Lightstyles**.

## Technical Details

- Engine function: `CL_RunLightStyles` at `SoF.exe` RVA `0x0002F940`
- `cl_lightstyle` array at RVA `0x0026B420` (256 entries, 0x110 bytes each)
- `cl.time` at RVA `0x001E8FC8`
- Light style index `13` is the sky style and is handled separately by the engine,
  so it is skipped.
- `length == 0` (fullbright default) and `length == 1` (single value) are left
  untouched.
