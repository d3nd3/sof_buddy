# Lighting Blend / Overbright

## Purpose

Extends SoF's lightmap pass with a user-selectable lighting style:

- **Vanilla** - stock SoF lighting.
- **Curve presets** - 2x Linear, 4x Linear, Soft Glow, High Contrast, Deep
  Shadows, Film Slope, Cliff Edge, Bright Punch, and Midnight. Every preset is
  just a set of 11 slider values, so selecting one writes those values into the
  public curve cvars. They use **Preserve brightness** and work with
  `gl_ext_multitexture` on or off.
- **Overbright Lighting** - an independent On/Off toggle for the original SoF
  Buddy product blend. It doubles the lightmap * texture product, so dark
  textures can still reach white. It works with `gl_ext_multitexture` on or
  off: with multitexture on the x2 is applied in texture unit 1
  (`GL_ARB_texture_env_combine` + `GL_RGB_SCALE_ARB`), and with multitexture off
  it uses the original `GL_DST_COLOR`, `GL_SRC_COLOR` framebuffer blend. While
  it is on, the Lighting Style preset list and manual blend section are hidden;
  a separate **Overbright Pre-Curve** with its own profiles (default
  **Late highlights**) and Colour Mode appear in their place. The stored
  Lighting Style curve is untouched and returns when the toggle is turned off.
- **Custom** - the 11-point lightmap tone curve plus the manual blend factors.
  Selecting Custom after a preset starts from that preset's slider values;
  editing any slider while a preset is active switches to Custom automatically.
  Leaving the blend at `GL_ZERO` / `GL_SRC_COLOR` gives curve-only overbright.
  The legacy `GL_DST_COLOR` / `GL_SRC_COLOR` pair stacks the old product blend
  on top of the curve; that is much harsher and clips colour channels harder.

The manual blend list also exposes the legacy variations:

- `Src ZERO` / `Dst SRC_C`: `L * T`, stock multiply.
- `Src DST_C` / `Dst SRC_C`: `2 * L * T`, the original product. Pure multiply;
  dark textures stay dark. Selecting this pair in **Custom** still requires
  `gl_ext_multitexture 0`; the **Overbright Lighting** toggle works with either
  setting.
- `Src SRC_C` / `Dst SRC_C`: `L^2 + L * T`. Full light range on black textures,
  but brighter textures clip sooner, so they look more blown out. **SRC^2 Blend**
  on the Custom page selects it directly.

The curve is a real brightness mapping, not a flat post-process multiplier. It
has eleven control points at 0, 10, 20 ... 100% of the lightmap input. Each
point is an output percentage in the same 0..100 range: `100` is white, `50` is
half brightness. Values are clamped at 100, so overbright is expressed by
mapping lower inputs to 100: `0,20,40,60,80,100,100,100,100,100,100` is 2x
until white, and `0,40,80,100,...` is 4x. This is equivalent to the old 0..400%
control points but matches the intuitive “input light -> output light” model.

Three output modes are available:

- **Preserve brightness** (`_sofbuddy_lighting_preserve_color 1`, default and
  what all built-in curve presets use) applies the curve to Rec.709 luminance
  and scales R/G/B by the same factor. This preserves hue and saturation while
  no channel clips, but a saturated pixel whose luma target needs more than 1.0
  in its brightest channel still clips at the top, producing a narrower
  whitening zone than per-channel mode.
- **Per-channel** (`_sofbuddy_lighting_preserve_color 0`) applies the curve to
  R, G, and B independently. Aggressive gains make coloured lights clip at
  different levels and look more saturated/neon. This is the classic overbright
  look.
- **Preserve colour** (`_sofbuddy_lighting_preserve_color 2`) applies the curve
  to the brightest channel (HSV value) and scales R/G/B by the same factor.
  Since no channel is larger than the value used for the lookup, the result can
  never exceed 255: hue and saturation ratios are preserved exactly and there is
  no clipping/whitening. The trade-off is that a colour already near white has
  no headroom and cannot be brightened further by the curve.

Overbright Lighting uses its own pre-curve, so the stored Lighting Style curve
and the Overbright curve never overwrite each other. The colour mode still
governs how the pre-curve is applied: preserve modes scale RGB together,
per-channel mode curves each channel independently.

## The whole pipeline as math

This is the entire feature in one place. The equations are the continuous
model; the actual implementation uses 256-entry LUTs and integer rounding, as
described below.

Definitions:

```text
clamp01(x) = min(1, max(0, x))
Q(x)       = round(255 * x) / 255          // 8-bit framebuffer quantisation
```

### One-line form

```text
O_c = Q( clamp01( base_c(T_c, L_c) + (h / 100) * L_c² ) )
```

### Fully expanded form

```text
Lightmap build:
  Lsum_c     = Σ_k  a_k * S_k,c
  L_raw,c    = Lsum_c * min(1, 1 / max(1, max_c Lsum_c))
  x_c        = G(L_raw,c)

Normalisation:
  M          = max over static map samples of S_k,c
  R          = G( clamp(M * 255, 64, 255) / 255 )      when normalisation is on
  N_R(x)     = R > 0 ? min(1, x / R) : x

Curve / preserve:
  per-channel:
    L_c = C( N_R(x_c) )

  preserve brightness:
    Y   = (54*x_r + 183*x_g + 19*x_b) / 256
    L_c = clamp01( x_c * C(N_R(Y)) / Y )               (Y = 0 -> neutral L_c)

  preserve colour:
    V   = max(x_r, x_g, x_b)
    L_c = clamp01( x_c * C(N_R(V)) / V )               (V = 0 -> neutral L_c)

Texture / base blend:
  curve-only:     base_c = T_c * L_c
  Overbright 2x:  base_c = 2 * T_c * L_c
  SRC²:           base_c = L_c² + T_c * L_c

Output:
  O_c = Q(F_c)
```

### Symbols

- `S_k,c` - raw static lightmap sample for style `k`, channel `c` (0..1).
- `a_k` - current light-style intensity (0..1; style 0 is 1).
- `Lsum_c` - lightmap after summing the active style samples, before the
  max-channel shrink.
- `L_raw,c` - engine lightmap after style summation and the max-channel
  shrink, before gamma.
- `x_c` - gamma-corrected lightmap value before normalisation.
- `G` - the game gamma table (`vid_gamma`, brightness, contrast).
- `M` - brightest raw static lightmap sample on the map.
- `R` - gamma-corrected normalisation reference; gain is `1/R` when `R > 0`.
- `N_R` - map normalisation. Off when `R = 0`.
- `C` - the 11-point tone curve, piecewise-linear through
  `(i/10, p_i/100)`, clamped to 0..1.
- `C_ob` - the separate Overbright Pre-Curve. Defaults to the **Late
  highlights** profile; it shapes the lightmap before the Overbright x2, giving
  `2*T*C_ob(L)`. The **2x everything** profile is the old linear identity.
- `T_c` - filtered texture sample.
- `L_c` - final lightmap texel after gamma, normalisation and curve/preserve.
- `base_c` - the world pass result before the texture blend.
- `F_c` - framebuffer value before 8-bit quantisation.
- `O_c` - final 8-bit framebuffer value.

### Common cases

```text
Vanilla:
  C(x) = x, no normalisation, no preserve
  L_c = G(L_raw,c)
  O_c = clamp01(T_c * L_c)

Curve preset:
  L_c = preserve(C(N_R(G(L_raw,c))))
  O_c = clamp01(T_c * L_c)

Overbright 2x:
  C_ob(x) = Overbright Pre-Curve (Late highlights by default)
  L_c = preserve(C_ob(G(L_raw,c)))        // or per-channel C_ob
  O_c = clamp01(2 * T_c * L_c)

SRC²:
  O_c = clamp01(L_c² + T_c * L_c)
```

These simplified cases assume normalisation is off. With normalisation on, the
`N_R` step above still applies even when the curve is the identity (Overbright
Lighting and Vanilla can still be normalised through the per-channel LUT or the
preserve post-hook).

### Worked example

With `T = 0.2`, `L = 0.8`, a 2x curve where `C(0.8) = 1`, and no normalisation:

| Mode | Equation | Result |
|---|---|---:|
| Curve only | `0.2 * 1` | `0.20` |
| Overbright 2x | `2 * 0.2 * 0.8` | `0.32` |
| SRC² | `0.8² + 0.2 * 0.8` | `0.80` |

The curve-only row demonstrates the texture cap: `T * curve(L) <= T`. The
Overbright row doubles the product after the texture multiply. SRC² adds an
`L²` term that does not contain `T`, so it can push a dark texture toward white,
unlike the curve-only row.

The integer implementation rounds after each LUT lookup. The identity curve is
exact for all 256 bytes, and the preserve modes early-out when the target equals
the input luma/value, so Vanilla and Overbright Lighting remain byte-for-byte
stock when normalisation is off.

## Why overbright is hard: clamping and the texture cap

SoF's renderer is a fixed-function OpenGL 1.x pipeline with 8-bit lightmaps and
an 8-bit framebuffer. The normal lightmap pass computes:

```text
framebuffer = clamp(texture * lightmap, 0..1)
```

The curve changes the lightmap before that multiply. No matter how aggressive
the curve is, the result is still:

```text
T * curve(L) <= T
```

because `curve(L)` can never exceed 1. A dark texture (`T = 0.2`) stays at 0.2
under white light; only the lightmap contrast changes. This is the fundamental
difference between a curve preset and the original overbright:

```text
2x Linear / any curve:  T * min(2L, 1) = min(2TL, T)
Overbright 2x:          min(2 * T * L, 1)
```

The legacy overbright moves the x2 to the framebuffer blend, after the texture
multiply. The clamp happens once, on the final product, so a 0.2 texture can
reach 0.4 (or white with enough light) instead of being capped at 0.2.

### The engine's own max-channel shrink

`R_BuildLightMap` also has a max-channel shrink: when one channel exceeds 255,
it scales all three channels down to fit. That preserves hue but lowers the
lightmap's peak. Disabling it only changes per-channel clipping and hue; it does
**not** remove the `<= T` cap, because the curve output is still in 0..1. The
shrink is left in place.

### What each blend can do

| Mode | Math | Dark texture | Bright texture |
| --- | --- | --- | --- |
| Curve only | `T * curve(L)` | capped at `T` | caps at `T` |
| Overbright 2x | `2 * T * L` | can exceed `T`; clamps at white | clips sooner |
| SRC² | `L² + L*T` | black textures get the full `L²` range | clips sooner |

SRC² comes from `glBlendFunc(GL_SRC_COLOR, GL_SRC_COLOR)`: the incoming
lightmap is both source and source factor, while the destination factor also
scales the textured framebuffer:

```text
out = src*src + dst*src = L² + T*L
```

Overbright 2x comes from `GL_DST_COLOR, GL_SRC_COLOR`:

```text
out = src*dst + dst*src = 2*T*L
```

### The multitexture problem

With `gl_ext_multitexture 1`, the renderer no longer does a second framebuffer
pass; it multiplies texture and lightmap inside texture unit 1 and writes the
product in one pass. There is no "final product" framebuffer blend left to
double, so the original overbright has to be recreated earlier. The feature uses
`GL_ARB_texture_env_combine`:

```text
unit 0 = texture T
unit 1 = lightmap L
GL_COMBINE: PREVIOUS * TEXTURE
GL_RGB_SCALE_ARB = 2
result = min(2 * T * L, 1)
```

The x2 happens before the framebuffer, so it still multiplies the texture and
lightmap product rather than only the lightmap. The catch is that the combiner
only implements a product; arbitrary manual blend factors still need the
single-texture framebuffer path. That is why non-default Custom blends force
`gl_ext_multitexture 0`, while Overbright Lighting works with either setting.

### Quantization and gamma

The lightmap is 8-bit per channel and the curve is a 256-entry LUT. `apply_byte`
evaluates the piecewise-linear curve and rounds to the nearest byte, so the
identity curve returns every possible input unchanged. The LUT is composed with
the game's gamma table (`vid_gamma`, brightness, contrast), and normalization is
applied to the gamma-corrected value. The framebuffer remains 8-bit, so the
final clamp to 0..255 is unavoidable.

## Preserve modes under the hood

The three output modes are not three different curves. They are three different
ways of deciding the scalar that multiplies R/G/B.

### Per-channel

`_sofbuddy_lighting_preserve_color 0`:

- The curve is baked into `lightmap_curve_lut[256]`.
- `R_BuildLightMap` reads that LUT instead of the game gamma table.
- Each channel is looked up independently:
  `out_r = LUT[r]`, `out_g = LUT[g]`, `out_b = LUT[b]`.
- Normalization is applied to each channel index before the curve lookup.
- Channels clip independently, so bright coloured lights can shift hue and look
  more saturated/neon. This is the classic overbright look.

### Preserve brightness

`1`, the default for built-in presets:

- The inline LUT stays at the game gamma table; the lightmap bytes are the
  stock gamma-corrected values.
- `R_BuildLightMap`'s post-hook then runs on the finished RGBA buffer.
- For each pixel:

```text
luma   = (54*r + 183*g + 19*b + 128) >> 8   // Rec.709, integer weights
input  = normalize(luma)                    // only when normalization is on
target = preserve_lut[input]                // identity when no curve
if (luma == 0) r = g = b = target           // lifted black stays neutral
else if (target == luma) leave the pixel
else r = (r*target + luma/2)/luma           // and the same for g and b
```

- All three channels use the same `target/luma` factor, so hue and saturation
  ratios are preserved until a channel hits 255. A saturated colour whose
  brightest channel would exceed 255 clips only at that top end; the whitening
  zone is narrower than per-channel mode.
- Normalization is folded into the `target/luma` factor, so it also stays
  hue-preserving.

### Preserve colour

`2`:

- Same post-hook, but the curve is indexed by the brightest channel:

```text
value  = max(r, g, b)                       // HSV value
input  = normalize(value)
target = preserve_lut[input]
if (value == 0) r = g = b = target
else if (target == value) leave the pixel
else r = (r*target + value/2)/value         // and g, b
```

- Since every channel is `<= value` and `target <= 255`, the scaled result can
  never exceed 255. Hue and saturation ratios are exact and nothing clips.
- The trade-off is headroom: a colour already near white has `value ~= 255`,
  so the curve can only return 255 or lower. It cannot be brightened further.
  Shadows and midtones still move normally.

### Why two LUTs and a post-hook?

A single per-channel 8-bit LUT cannot implement "look up luminance, then scale
all three channels by the result" because the lookup index and the output are
per-channel. The feature keeps a second `preserve_color_curve_lut` for the
scalar LUT and applies it in the post-hook, where it can read all three channels
at once. The inline `lightmap_curve_lut` is still used for per-channel mode and
for the stock gamma path.

### Identity is exact

The identity curve (`0,10,20,...,100`) evaluated through `apply_byte` returns
every input byte unchanged. With normalization off, Vanilla and Overbright
Lighting therefore leave the lightmap byte-for-byte stock even in the preserve
modes; the post-hook's `target == luma` / `target == value` early-out makes it a
no-op. This is why Colour Mode appears to do nothing on those styles unless Map
Lighting Normalization is enabled. Normalization is the one part of the preserve
pipeline that is active on every style.

## Curve presets and console templates

Every built-in curve preset writes its 11 values into the public curve cvars.
The F12 **Tone Curve** editor stays available under every curve preset, seeded
with that preset's values, so a preset can be used as a starting point. Editing
any slider in the live editor keeps the value and switches Lighting Style to
Custom. Selecting a preset from F12 updates the editor and applies immediately;
choosing **Custom** keeps the live panel open. The same templates can be applied
from the console:

| Alias | Preset | 0 | 10 | 20 | 30 | 40 | 50 | 60 | 70 | 80 | 90 | 100 |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `linear2x` | 2x Linear | 0 | 20 | 40 | 60 | 80 | 100 | 100 | 100 | 100 | 100 | 100 |
| `linear4x` | 4x Linear | 0 | 40 | 80 | 100 | 100 | 100 | 100 | 100 | 100 | 100 | 100 |
| `softglow` | Soft Glow | 5 | 17 | 29 | 42 | 56 | 70 | 82 | 94 | 100 | 100 | 100 |
| `contrast` | High Contrast | 0 | 1 | 4 | 10 | 25 | 50 | 74 | 89 | 96 | 99 | 100 |
| `deep` | Deep Shadows | 0 | 0 | 2 | 6 | 18 | 44 | 70 | 86 | 94 | 98 | 100 |
| `film` | Film Slope | 3 | 8 | 16 | 28 | 44 | 60 | 74 | 85 | 92 | 97 | 100 |
| `cliff` | Cliff Edge | 0 | 2 | 5 | 8 | 72 | 82 | 89 | 94 | 97 | 99 | 100 |
| `punch` | Bright Punch | 0 | 10 | 22 | 38 | 58 | 75 | 87 | 94 | 98 | 99 | 100 |
| `midnight` | Midnight | 0 | 0 | 1 | 3 | 8 | 22 | 50 | 78 | 92 | 98 | 100 |

```text
sofbuddy_lighting_curve_templates
sofbuddy_lighting_curve_template cliff
```

The template command sets the 11 slider cvars, selects **Preserve brightness**,
and switches Lighting Style to **Custom** so the values can be tweaked. After it
runs, `sp_sc_cvar_list *lighting_curve*` shows the current values and `condump`
can save them. For a manual copy-paste, Cliff Edge is:

```text
set _sofbuddy_lighting_preset 5
set _sofbuddy_lighting_curve_0 0
set _sofbuddy_lighting_curve_10 2
set _sofbuddy_lighting_curve_20 5
set _sofbuddy_lighting_curve_30 8
set _sofbuddy_lighting_curve_40 72
set _sofbuddy_lighting_curve_50 82
set _sofbuddy_lighting_curve_60 89
set _sofbuddy_lighting_curve_70 94
set _sofbuddy_lighting_curve_80 97
set _sofbuddy_lighting_curve_90 99
set _sofbuddy_lighting_curve_100 100
```

## Map lighting normalization

Many maps never use the full 8-bit lightmap range; the retail SoF lightmaps are
often capped around `196/255`. **Map lighting normalization**
(`_sofbuddy_lighting_normalize`, default `0`/off) scans the loaded world model's
static lightmap samples and remaps their gamma-corrected range before the tone
curve is applied.

- **`1` Up:** an **Up amount** from 0–100% chooses how far the brightest
  sample moves toward white: `target = max + amount * (255 - max)`. 0% leaves
  the maximum, 100% maps it to 255. The range is scaled, so black stays black
  and values above the map maximum keep their relative headroom.
- **`2` Down:** a **Down amount** from 0–100% chooses how far the absolute
  minimum moves toward black: `target = min - amount * min`. 0% leaves the
  minimum, 100% maps it to 0. The maximum stays fixed, and values above the map
  maximum (dynamic lights) are left untouched so they still stand out. When the
  absolute minimum is already `0`, Down has no black point to move; this is
  intentional, because it is a black-point stretch rather than a brightness
  boost.
- **`3` Both:** applies Down and then Up. With both amounts at 100% it is the
  full absolute-min..max → 0..255 stretch. The live hub shows only the amount
  sliders that the active mode uses.
- The percentages are map-relative: the same 50% setting recalculates the
  target when the next map has a different min/max.
- The scan runs from the `R_BeginRegistration` post-hook, after the BSP and its
  samples are loaded. It records the absolute minimum, maximum, mean, median and
  percentiles.
- Normalization always applies on every map; there is no bright-map skip.
- Toggling the mode, or loading a different map, refreshes the shared
  normalization LUT and queues a lightmap rebuild. In the preserve modes the
  inline LUT stays at the gamma table, so the shared normalization LUT is
  included in the change detection; otherwise existing static lightmaps would
  keep their pre-normalization values.
- It stacks with the selected lighting style and colour mode.
- Normalization is applied **before** the tone curve. A steep preset that
  already clips (for example 2x Linear maps input 50 to white) will therefore
  show the gain mostly in the remaining midtones and shadows; a gentler curve
  shows more of the reclaimed range. The page still reports the exact gain.
- In the preserve modes, normalization is folded into the uniform RGB scale,
  so it does not break the preserved channel ratios. In per-channel mode it
  scales each channel index independently, like the rest of the curve.
- **Entities, particles, and Ghoul models follow the same LUT.** The renderer's
  `R_LightPoint` path samples raw lightmap bytes and normally bypasses the
  built lightmap texture. A post-hook applies the same gamma, normalization
  reference, and per-channel/preserve curve to its returned colour whenever the
  LUT differs from stock. This is what keeps Ghoul models from staying at the
  old unnormalized brightness in shadowed areas. Vanilla with no curve or
  normalization leaves the hook inactive.
- **Ghoul minimum light: Vanilla by default, optional bypass.** `DrawPlayer`
  clamps entities with `RF_MINLIGHT` to a hard `0.1` floor after `R_LightPoint`
  and `ghl_light_multiply`. F12 → Lighting → Ghoul has **Ghoul Minimum Light:
  Vanilla/Bypass**. Vanilla keeps the stock floor; Bypass rewrites the
  instruction to `0`, so the model follows the actual light and the curve and
  can go fully dark. Entities explicitly flagged fullbright remain fullbright by
  design.
- **Ghoul imported dlights follow the curve.** The engine passes a copy of each
  dlight to `Ghoul::AddImportLight` for model lighting. That copy's colour is
  scaled by the effective curve before it reaches Ghoul; the original
  `r_dlights` list is left untouched for the world lightmap path, so world
  dlights are not double-curved. This is a single scale per light (based on its
  colour), while `R_LightPoint` remains the exact per-sample entity light.
- The F12 Lighting page shows a compact `Normalization :` status line. The live
  hub has the mode list and the matching Up/Down amount sliders.

Dynamic lights and animated light styles are still applied by `R_BuildLightMap`
after the static sample scan; normalization only establishes the map's static
input range.

### Normalization under the hood

1. **Scan.** `lighting_scan_model_max_light()` walks the world model's surface
   array. It skips surfaces that do not use static lighting
   (`kNonLitSurfaceFlags = 0x80C`: sky, warp, and `SURF_ALPHA_TEXTURE`), reads
   the four light-style bytes until `0xFF`, and scans
   `width * height * 3 * num_styles` bytes from the surface's sample block. The
   result is the brightest raw channel found in any static style block.
2. **References.** `lighting_normalize_min_reference()` and
   `lighting_normalize_max_reference()` map the raw map minimum and maximum
   through the game gamma table, because the curve operates on gamma-corrected
   values. They return `0` when normalization is off or no map range is known.
   The live percentages are applied to these actual references; there is no
   fixed 4x gain cap.
3. **Scale.** `lighting_curve_detail::build_normalization_lut_amount()` fills a
   256-entry LUT indexed by gamma-corrected bytes. Let `u` and `d` be the Up and
   Down amounts in 0..1:
   - Up: `white = max + round((255 - max) * u)`;
     `out = (value * white + max / 2) / max`, clamped to `255`.
   - Down: `black = min - round(min * d)`;
     `0..min` maps to `0..black`, `min..max` maps linearly to `black..max`, and
     values above `max` are returned unchanged.
   - Both: Down first, then Up. At `u = d = 1` this is the full
     min..max → 0..255 stretch; at `u = d = 0` it is identity.
   Flat ranges are left unchanged.
4. **Apply.**
   - Per-channel mode folds the shared normalization LUT into
     `lightmap_curve_lut` before the curve lookup, so each channel is
     normalized independently.
   - Preserve brightness and Preserve colour pass the same LUT to the
     post-hook, which normalizes the luma or brightest channel and scales RGB by
     the same factor.
   - `R_LightPoint` uses the same LUT for entity, particle, and Ghoul light.
5. **Rebuild.** The map's static lightmaps were usually built before the post
   hook had a chance to scan the new map, and in preserve modes the inline LUT
   bytes do not change when normalization changes. The feature therefore
   includes the normalization LUT in its change detection; a mode or range
   change queues a lightmap rebuild. `R_BeginRegistration` resets the old map's
   range in the pre hook, scans the new map in the post hook, and compares the
   LUTs again.
6. **Readout.** The F12 status line shows the mode and percentage targets:
   Up shows `Up 50% -> white N/255`; Down shows `Down 50% -> black N` or
   `min=0: no dark point.`; Both shows the final black..white range. The targets
   are gamma-corrected bytes.

Animated light styles are also refreshed by the rebuild: each surface is built
with the current style values and `R_SetCacheState` records those values, so a
later style change still triggers a normal rebuild. Dynamic lights are applied
after the static scan and are not part of the map maximum. The full-color curve
path is bypassed by `gl_monolightmap` debug modes.

### Static style 0 readout

The F12 Lighting page reads the renderer's active light style 0 and shows the
current map's value, for example:

```text
Static style 0 : 1.00 (m)
```

Quake maps the `CS_LIGHTS+0` letters as
`(letter - 'a') / ('m' - 'a')`:

| Letter | Value |
|---|---:|
| `a` | 0.00 |
| `l` | 0.92 |
| `m` | 1.00 |
| `n` | 1.08 |
| `z` | 2.08 |

`m` is the neutral 1.0 value; it is not below 1. The code leaves headroom
*above* 1 (`n`..`z`) for extra punch in other light styles. A map that sets
style 0 below `m` is deliberately reserving more headroom for those styles.
The readout is updated a few times per second from the renderer's live style
array and is cleared while a new map is loading.

## Overbright Lighting with multitexture

Overbright Lighting is the original SoF Buddy product blend, exposed as an
independent toggle instead of a Lighting Style preset. It works with either
renderer path and never changes the user's `gl_ext_multitexture` value. It
always applies while On; there is no per-map skip.

```text
multitexture on:
  unit 0 = texture T
  unit 1 = lightmap L
  GL_COMBINE_ARB: PREVIOUS * TEXTURE, GL_RGB_SCALE_ARB = 2
  result = min(2 * T * L, 1)

multitexture off:
  GL_DST_COLOR / GL_SRC_COLOR framebuffer blend
  result = min(2 * T * L, 1)
```

The combiner path follows the renderer's active path. `ref_gl` nulls the SGIS
multitexture pointers in `R_Init` when `gl_ext_multitexture` is 0, so the
pointer is the truth for the renderer that is currently running. A console
`gl_ext_multitexture` change without `vid_restart` therefore keeps the existing
path (and the existing blend) until the next renderer restart, when both switch
together. The x2 happens after the texture multiply, so dark textures can still
reach white. The **Overbright Pre-Curve** is applied to the lightmap before the
combiner or framebuffer blend in both paths, so it works with multitexture on or
off. Its default is **Late highlights**; the **2x everything** profile is the
linear identity that keeps the stock `2*T*L` product.

The combiner is installed by patching the two `CheckComplexStates` calls and the
`glEnd` call inside `GL_RenderLightmappedPoly`. The wrappers forward normally
when Overbright Lighting is off, so the curve presets keep their original state.

The live hub shows only one style row: the **Lighting Style** row and
**Tone Curve...** button while Overbright is off, or the **Overbright Pre-Curve**
row and **OB Pre-Curve...** button while it is on. The shared Colour Mode row
and Map Lighting Normalization stay visible because they apply independently of
which curve is active. The main Lighting tab no longer duplicates these rows; it
keeps the Custom manual blend (only when Overbright is off) and the non-preview
controls.

### Overbright Pre-Curve

The pre-curve is a second 11-point curve with its own archived cvars,
`_sofbuddy_lighting_overbright_curve_0` through `_100`. It is separate from the
Lighting Style presets and has its own profile selector,
`_sofbuddy_lighting_overbright_profile`:

| Profile | Values | Character |
|---|---|---|
| **Late highlights** (default) | `0,5,10,15,20,25,30,35,45,65,100` | Vanilla through 70% input, then only the top range pops. |
| **Popping highlights** | `0,5,10,15,20,25,30,45,60,80,100` | Stronger pop starting above 60% input. |
| **Popping mid+highlight** | `0,5,10,15,20,25,40,55,70,85,100` | Brighter midtones and highlights. |
| **Preserve Darks only** | `0,5,10,15,23,33,45,59,75,93,100` | Keeps shadows dark and lifts mostly the upper range. |
| **2x everything** | `0,10,20,30,40,50,60,70,80,90,100` | Linear identity: the original stock `2*T*L` look. |
| **Custom** | edited values | Selected automatically when a slider is edited. |

Selecting a profile writes its values into the overbright curve cvars. Editing
any slider in the live editor switches the profile to Custom. The reset button
returns to **Late highlights**. The pipeline is:

```text
out = 2 * T * C_ob(L)
```

The default Late profile is half through 70% input, so `2 * C(0.5) = 0.50`,
`2 * C(0.6) = 0.60`, and `2 * C(0.7) = 0.70` all match vanilla. Above that it
lifts: `2 * C(0.8) = 0.90`, `2 * C(0.9) = 1.30`, and `2 * C(1.0) = 2.0` (white).
Popping highlights is the stronger option: half through 60%, then
`2 * C(0.7) = 0.90` and `2 * C(0.8) = 1.20` (clipped). A Custom curve must stay
at half to keep a tone unchanged; setting a point to identity makes the x2
double that tone instead.
The crossover is in gamma-corrected lightmap space, not linear light. The
stored Lighting Style curve is untouched and returns when Overbright is
switched off. The pre-curve is applied before the x2 in both renderer paths,
so it works with `gl_ext_multitexture` on or off and does not require a
restart. The LUT builder uses nearest-even rounding for the pre-curve, so the
preserved range does not drift brighter by one 8-bit level. The crossover
applies to the curve input: per-channel uses each channel, preserve
brightness uses Rec.709 luminance, and preserve colour uses the brightest
channel. A saturated midtone can therefore cross the crossover earlier in
preserve-colour mode than its luminance would suggest.

Verified against the legacy Raven path on `nyc1` in the same frozen first-person
view: ordinary static world surfaces match within rounding (per-pixel difference
of at most 2/255 on most walls and floors). Entities drawn through the
brush-model path, such as the lamp post near the spawn, stay about a third
brighter under the legacy path because its global lightmap blend also
multiplies that path's pre-blend contribution.

## Other implementation details

### Lightmap rebuild and upload

`lighting_rebuild_world_lightmaps()` is the in-place refresh path used whenever
the LUT or normalization reference changes. It does not repack the atlas; it
rebuilds each surface into a scratch RGBA buffer and uploads it back to the same
atlas rectangle.

- Iterates the world model surfaces (`+0xB4` count, `+0xB8` array, stride
  `0x6C`).
- Skips surfaces without texinfo, non-lit flags (`0x80C`), invalid lightmap
  indices, invalid width/height, and atlas rectangles outside `128x128`.
- Includes surfaces with non-zero/animated light styles. Saves the surface's
  `dlightframe` (`+0x44`), clears it, calls the original `R_BuildLightMap` into
  a scratch buffer, applies the preserve post-process, and restores the frame.
  This prevents per-frame dynamic lights from being baked into the static atlas.
- Calls `R_SetCacheState` after the upload so the cached style values match the
  lightmap that was just built. If an animated style changes later,
  `isCachedLight()` still detects it and rebuilds normally.
- Binds `gl_state_lightmap_textures + lightmap_index`, uploads with
  `glTexSubImage2D` at the surface's `lightmap_x/lightmap_y` (`+0x18/+0x1C`),
  and restores the previous texture binding.
- Logs `rebuilt N world lightmaps (M skipped)`.

`lighting_frame_tick()` watches the gamma table and the rebuild request flag. It
waits `s_rebuild_delay` frames before running the rebuild so it never happens
in the middle of a world draw. The request is queued by preset/curve/colour/
normalization changes, gamma changes, map load, renderer reload, and
`sofbuddy_lighting_rebuild`.

### Lightmap LUT patch

`R_BuildLightMap` reads the shared `gammatable` at `ref_gl+0x4C520` from three
instructions (`+0x99D5`, `+0x99DD`, `+0x99E6`). `lighting_patch_lut_reads()`
verifies the expected opcodes and operands and redirects those reads to
`lightmap_curve_lut`. If the renderer does not match the retail 1.06a layout, it
logs and leaves the code untouched. In that case the per-channel curve is
unavailable, but the preserve modes still work through the `R_BuildLightMap`
post-hook (the post-hook does not depend on the patched reads).

### Overbright combiner patch

The multitexture path cannot use a framebuffer blend, so the feature patches only
`GL_RenderLightmappedPoly`:

- the two `CheckComplexStates` calls at `ref_gl+0x15C53` and `+0x15CC6`
- the `glEnd` call at `+0x15D18`

The wrappers call the originals, then set unit 1 to `GL_COMBINE_ARB` with
`PREVIOUS * TEXTURE` and `GL_RGB_SCALE_ARB = 2`. The `glEnd` wrapper restores
`GL_MODULATE`. When Overbright Lighting is off, the wrappers forward and do
nothing else.

Texture-unit selection uses the engine's `qglActiveTextureARB` wrapper
(`ref_gl+0x6130`), not the raw `glSelectTextureSGIS` pointer. The engine tracks
the current unit in `gl_state_currenttmu` and caches the bound texture per unit
in `dword_3008FFD8[]`; calling the raw pointer without updating that state made
`GL_MBind2` bind the wrong texture and corrupted the world. This was the main
bug found while prototyping the combiner.

The combiner follows the renderer that is actually running. `R_Init` nulls the
SGIS pointers when `gl_ext_multitexture` is 0, so a console cvar change without
`vid_restart` keeps the current path and the current blend until the next
restart.

### Live preview panels

The settings whose effect is easiest to judge in the world open as a right-side
panel instead of a full menu. The main menu is closed first
(`killmenu;sofbuddy_menu ...`), so only the panel remains over the game and the
world stays visible while a value changes. The panels are authored as
`<stm resize 300 600 global nopush>` pages. `hk_frame_resize()` pins that exact
size to the right edge (`vid_w - scaled_w - 8`) and scales it down with
`min(menu scale, (vid_h - 16) / 600)` so it also fits at 640x480. Other resize
pages stay centred.

The main F12 Lighting tab is now the non-preview page: it keeps
`gl_ext_multitexture`, the Lighting status readouts, Lightmap Multiplier, the
Custom manual blend, and the restore/rebuild actions. The preview rows are no
longer duplicated there. The manual blend remains on the main tab (Custom only,
while Overbright is off) rather than in the hub.

The live pages are:

- `lighting_live.rmf` + `lighting_live_content.rmf` - the hub. Overbright
  Lighting, Map Lighting Normalization (the Up/Down amount sliders appear only
  for the modes that use them), Colour Mode, Specular Textures, Dynamic
  Light Threshold, Dynamic Lighting, `gl_dlightintensity`, Ghoul
  Mode/Specular/Shadows/Light/Minimum Light, **Rebuild Lightmaps**,
  **Back to Lighting** (full F12 page), and **Back to Game**. The Lighting
  Style row and **Tone Curve...** button are included only while Overbright is
  off; the Overbright Pre-Curve profile and **OB Pre-Curve...** button only
  while it is on. Toggling Overbright rebuilds the hub (`sofbuddy_close_menu;
  reloadall;sofbuddy_menu sof_buddy/lighting_live`) so the rows swap immediately
  without stacking a second hub page. `lighting_live_style_row.rmf` and
  `lighting_live_precurve_row.rmf` hold the two conditional snippets. The live
  pages and their Back/Close buttons run `sofbuddy_close_menu`, which pops the
  whole menu stack, so the F12 page cannot remain behind a popout.
- `lighting_curve_live.rmf` + `lighting_curve_live_content.rmf` - the Tone
  Curve editor (11 sliders, reset, **Back to Live Lighting**, **Back to
  Game**).
- `lighting_overbright_curve_live.rmf` +
  `lighting_overbright_curve_live_content.rmf` - the Overbright Pre-Curve editor
  (profile and Colour Mode lists, 11 sliders, reset, **Back to Live Lighting**,
  **Back to Game**).

**Back to Game** uses `killmenu` and returns to the game. **Back to Live
Lighting** returns to the hub, and **Back to Lighting** opens the full Lighting
tab. These navigation buttons use an orange border so they read differently
from the normal cyan controls. The main Lighting tab has a **Live Lighting
(preview over game)** button at the top. The hub's **Tone Curve...** and
**OB Pre-Curve...** buttons open the live editors. `gl_ext_multitexture` and
**Lightmap Multiplier** remain on the main tab because they need a renderer
restart or a confirmation dialog.

### UI snippets and migration

The main F12 Lighting tab is now the non-preview page. It keeps
`gl_ext_multitexture`, the Lighting status readouts, the Lightmap Multiplier,
the Custom manual blend, and the restore/rebuild actions. The preview rows live
only in the live hub.

- `_sb_internal_lighting_custom_section_rmf` - include gate for the manual blend.
  Empty while Overbright is on; the fragment itself is gated by
  `_sb_internal_lighting_custom`.
- `_sb_internal_lighting_custom` - `1` only for Custom; gates the manual
  lightmap blend controls.
- `_sb_internal_lighting_normalize_status` /
  `_sb_internal_lighting_style0_status` - the status readouts on the main tab.
- `_sb_internal_lighting_normalize_show_up` /
  `_sb_internal_lighting_normalize_show_down` - `1` when the active
  normalization mode uses the matching amount slider; set by
  `lighting_refresh_ui_rmf()`. The live normalize mode list rebuilds the hub so
  the sliders appear/disappear when the mode changes.

`lighting_refresh_ui_rmf()` rebuilds the manual-blend gate from the Overbright
state. The live hub's Style/Pre-Curve rows are conditional includes driven
directly by `_sofbuddy_lighting_overbright`, and its Overbright toggle uses
`sofbuddy_lighting_overbright_live_commit` to apply the setting and rebuild the
hub. The old centred popup pages and the unused style/pre-curve snippets were
removed.

Migration is handled in `lightblend_PostCvarInit()`:

- `_sofbuddy_lighting_preset 4` (the old Raven 2x entry) becomes
  `_sofbuddy_lighting_overbright 1` with preset `0` selected underneath.
- The legacy `_sofbuddy_lighting_overbright 1` value is now the toggle itself.
- Non-neutral manual blend factors still migrate to Custom.
- The preset list no longer contains value `4`; `lighting_preset_change()` still
  accepts it from old scripts and promotes it to the toggle.

### Lightmap Multiplier warning

The F12 **Lightmap Multiplier** slider is bound to the internal mirror cvar
`_sb_internal_lighting_gl_modulate`, not directly to `gl_modulate`. The mirror
exists so a slider move can be intercepted before the engine cvar changes:

- Moving the slider away from `1` stores the requested value, shows it in
  `_sb_internal_lighting_gl_modulate_pending`, and queues
  `sofbuddy/gl_modulate_warning` once the value has been stable for about
  300 ms. The debounce lets a drag reach the intended value before the popup
  interrupts it.
- The popup shows `Current` (the real cvar) and `Requested` (the mirror), plus
  the warning that keeping `gl_modulate` at `1` avoids flat lighting.
- **Apply Anyway (Y)** calls `sofbuddy_gl_modulate_confirm` and writes the real
  cvar. **Cancel (N)** and Escape call `sofbuddy_gl_modulate_cancel` and restore
  the mirror. Either path closes the popup and reopens the Lighting page.
- Returning the slider to `1` applies immediately and never opens the popup.
- Closing the whole menu, switching to another F12 page, or pressing F12 while
  the popup is open is treated as a cancel by
  `lighting_gl_modulate_menu_closed()` / `lighting_gl_modulate_note_menu()`, so
  the mirror can never be left showing a value that was not applied.
- The real `gl_modulate` remains untouched while the popup is open. Console
  writes to `gl_modulate` are mirrored back to the slider on the next frame.

### Diagnostics

- `sofbuddy_lighting_status` prints the effective preset, blend factors,
  colour mode, normalization state/maximum, Overbright state,
  combiner state, and the 11 curve values. While Overbright is on it also prints
  the separate Overbright Pre-Curve.
- The log records curve/preset changes, normalization scans, rebuild counts,
  the Overbright combiner path, and migration decisions.
- `sofbuddy_lighting_rebuild` forces a rebuild; useful after a manual cvar edit
  that did not go through a change callback.

## Performance

The renderer is fixed-function, so the cost split is simple: anything that
changes lightmap bytes is a load/rebuild cost, while the per-frame costs are
Overbright's extra blend state and a small entity-light LUT lookup when
normalization or a non-identity curve is active.

Measured with a static 71-frame `timedemo` at `nyc1`, 1280x720, 3 runs per mode,
medians, with the game pinned to two cores and SCHED_FIFO. This is software
rendering (Xvfb/llvmpipe), so absolute FPS is low; compare the percentages, not
the raw numbers.

| Mode | Multitexture ON | Multitexture OFF |
|---|---:|---:|
| Vanilla | 11.5 fps | 11.6 fps |
| Curve preset 2x Linear | 11.5 fps (0%) | 11.3 fps (-3%, noise) |
| Overbright Lighting | 10.6 fps (-8%) | 11.0 fps (-5%) |
| Overbright + Pre-Curve | 11.0 fps (-4%, noise) | 11.1 fps (-4%, noise) |
| Curve + Normalization | 11.3 fps (-2%, noise) | 11.1 fps (-4%, noise) |

- **Overbright Lighting** has a small per-surface cost while On (the combiner
  on the multitexture path, or the framebuffer blend on the legacy path). Off
  performs no combiner or blend work; the patched polygons only execute the
  original calls plus a cached boolean check.
- **Tone Curve presets, Overbright Pre-Curve, and Map Lighting Normalization**
  have no per-frame cost on world surfaces. They are baked into the lightmap
  LUT / game gamma path and only rebuild lightmaps when a setting changes or a
  map loads. The small differences in the table are run-to-run noise.
- The **R_LightPoint post-hook** adds one cached boolean check when the LUT is
  stock. When normalization or a curve is active, entity/particle/Ghoul light
  gets a 256-byte LUT lookup per sample. A focused timedemo check
  (`nyc1`, 3 runs, multitexture on) measured Vanilla at 11.5 fps and
  Normalization + 2x Linear at 11.4 fps, so the cost is within noise here.
- The `gl_modulate` mirror sync and gamma-table check run every 16 frames rather
  than every frame.

## Known limitations and differences

- **Curve-only output cannot exceed the texture's own brightness.** This is a
  property of `T * curve(L)`, not a bug. Use Overbright Lighting when
  dark textures need to reach white.
- **The renderer is fixed-function and 8-bit.** There is no float/HDR
  framebuffer, bloom, or shader path; every blend clamps at 1.0.
- **Normalization uses the static map maximum.** Dynamic lights and animated
  light styles are applied later and are not part of the scan. The percentage
  amounts are relative to the observed min/max, so they carry over to the next
  map.
- **Overbright Lighting on the multitexture path is not pixel-identical to the
  legacy framebuffer path on brush-model entities.** The legacy global blend
  also multiplies that path's pre-blend contributions; the lamp post near the
  `nyc1` spawn is the known example and is about a third brighter under the
  legacy path.
- **Preserve colour cannot brighten a colour that is already near white.** It
  has no channel headroom by definition; Preserve brightness can clip the
  brightest channel instead.
- **Non-default Custom blend factors still require `gl_ext_multitexture 0`.**
  The texture combiner only implements a product; arbitrary manual factors need
  the framebuffer blend. Overbright Lighting and its pre-curve are the exception
  and work either way, because the curve is applied before the x2.
- **The LUT patch and combiner patches target the retail 1.06a `ref_gl`
  layout.** Unknown builds log a skip instead of corrupting code; per-channel
  curve or the multitexture combiner may be unavailable there.
- **`gl_monolightmap` must stay at `0`.** The monochrome debug modes bypass the
  full-color lightmap path.
- **A console `gl_ext_multitexture` change needs `vid_restart`.** The F12
  Lighting and Graphics rows do that automatically; the feature itself never
  changes the user's value.
- **Colour Mode is inert on Vanilla and Overbright Lighting when normalization
  is off.** The identity curve is exact; with normalization on, the mode chooses
  per-channel vs uniform normalization.

## Callbacks

- **PostCvarInit** (Post, Priority: 50)
  - `lightblend_PostCvarInit()` - registers CVars, commands, migrates old
    preset `4` configs to the independent Overbright Lighting toggle and
    manual blend factors to Custom.
- **RefDllLoaded** (Post, Priority: 60)
  - `lightblend_RefDllLoaded()` - resolves ref_gl addresses, installs the
    lightmap LUT patch, initialises the curve, applies initial settings.

## Hooks

- **R_BlendLightmaps** (Pre, Priority: 100)
  - `r_blendlightmaps_pre_callback()` - applies the effective blend factors.
- **R_BlendLightmaps** (Post, Priority: 100)
  - `r_blendlightmaps_post_callback()` - clears the blend-active flag.
- **R_SortAndDrawAlphaSurfaces** (Post, Priority: 100)
  - `r_sortanddrawalphasurfaces_post_callback()` - once per rendered frame,
    detects gamma-table changes and performs a queued lightmap rebuild after
    the world pass.
- **R_BuildLightMap** (Post, Priority: 100)
  - `r_buildlightmap_post_callback()` - in a preserve mode, applies the curve
    to luminance or the brightest channel and scales RGB together after the
    normal lightmap build.
- **R_BeginRegistration** (Pre, Priority: 100)
  - `r_beginregistration_pre_callback()` - clears the previous map's
    normalization range before its static lightmaps are built.
- **R_BeginRegistration** (Post, Priority: 100)
  - `r_beginregistration_post_callback()` - after a map's BSP and lightmap
    samples are loaded, scans the map maximum and refreshes normalization.

## How the curve is applied

`R_BuildLightMap` normally converts the finished lightmap through the shared
`gammatable` at `ref_gl+0x4C520`. The feature patches the three reads of that
table inside `R_BuildLightMap` (`ref_gl+0x99D5`, `+0x99DD`, `+0x99E6`) to point
at `lightmap_curve_lut`. The LUT is composed from the current game gamma table
and the selected 11-point curve, so `vid_gamma`/brightness/contrast still work
normally.

Because the lightmap data is built before it is uploaded, changing the curve
requires re-uploading the current map's static lightmaps. The feature queues a
rebuild request and performs it in the `R_SortAndDrawAlphaSurfaces` post-hook on
the next frame. It iterates the world model's surface array, rebuilds each lit
surface through the original `R_BuildLightMap`, and uploads the result back to
the same lightmap-atlas rectangle. Dynamic-light rebuilds use the new LUT
immediately.

The patch verifies the expected opcodes and operand first; on an unknown or
custom `ref_gl` build it logs and leaves the renderer untouched instead of
corrupting it. In that case the legacy blend presets still work, but the
per-channel lightmap curve is unavailable until the renderer matches the retail
1.06a layout.

In the **preserve brightness** and **preserve colour** modes the inline LUT is
left at the original gamma table, and the `R_BuildLightMap` post-hook applies the
curve to luminance or the brightest channel and scales R/G/B by the same factor.
The post-hook covers static, dynamic, and rebuilt lightmaps; the rebuild path
calls the same helper explicitly because it calls the original
`R_BuildLightMap` pointer directly.

## CustomDetours
- **glBlendFunc** (ref.dll, via memory patch)
  - `glBlendFunc_R_BlendLightmaps()` - Intercepts glBlendFunc calls during lightmap blending to apply custom blend modes
  - Patched at ref.dll addresses: `0x3001B9A4`, `0x3001B690`
- **AdjustTexCoords** (ref.dll, via memory patch)
  - `hkAdjustTexCoords()` - Calls the original `Builds_S_T_Shiny` for the non-spherical path and submits the resulting S/T
  - Patched at ref.dll address: `0x30014995`

## Memory addresses (ref_gl.dll, retail 1.06a)

- **R_BuildLightMap**: `0x30009550`
- **R_BeginRegistration**: `0x3000AB80`
- **R_SortAndDrawAlphaSurfaces**: `0x30011010`
- **Source blend factor**: `0x300A4610` - Default: `GL_ZERO`
- **Destination blend factor**: `0x300A43FC` - Default: `GL_SRC_COLOR`
- **Lighting cutoff**: `0x3002C368` - Float value for lighting cutoff
- **Water size (double)**: `0x3002C390` - Double value for water size
- **Water size (float)**: `0x3002C398` - Float value for water size (1.0 / water_size)
- **Shiny spherical branch**: `0x30014814` - `SURF_SIMPLE_SPHERICAL` branch in `R_DrawDetailChains`
- **Shiny sphere texinfo guard**: `0x30014A66` - Null-texinfo guard in the `GL_SPHERE_MAP` pass
- **Shiny pass gate**: `0x300147DF` - "shiny surfaces collected" test before the shiny pass
- **Shiny flat threshold**: `0x3002C434` - `radiusSquared`/flat-surface cutoff, stock `0.1f`
- **Gamma table**: `0x3004C520`
- **World model pointer**: `0x3008FC6C`
- **Lightmap texture base**: `0x3008FFD0`

Surface layout used by the rebuild: stride `0x6C`; texinfo `+0x34`; dlightframe
`+0x44`; lightmap index `+0x4C`; samples `+0x64`; width/height `+0x68/+0x69`;
atlas x/y `+0x18/+0x1C`; extents `+0x14/+0x16`. The world model has surface
count at `+0xB4` and the surface array at `+0xB8`.

## Configuration

### CVars

- `_sofbuddy_lighting_preset` - `0` Vanilla, `1` 2x Linear, `2` 4x Linear,
  `3` Soft Glow, `5` Custom, `6` High Contrast, `7` Deep Shadows,
  `8` Film Slope, `9` Cliff Edge, `10` Bright Punch, `11` Midnight.
  Default `0`. Value `4` is accepted only for old configs and migrates to the
  Overbright Lighting toggle.
- `_sofbuddy_lighting_curve_0`, `_10`, `_20`, ... `_100` - curve output
  percentages at 10% input intervals. Defaults `0,10,20,...,100` (identity).
  Output is clamped to 0..100; map lower inputs to 100 for overbright. Every
  built-in curve preset writes its values here.
- `_sofbuddy_lighting_preserve_color` - `0` per-channel curve,
  `1` preserve brightness (luminance, default), `2` preserve colour (brightest
  channel). Built-in curve presets select `1`.
- `_sofbuddy_lighting_normalize` - `0` map lighting normalization off
  (default), `1` Up (move the max toward white), `2` Down (move the absolute min
  toward black, max kept), `3` Both (Down then Up). Down has no effect when the
  absolute minimum is `0`.
- `_sofbuddy_lighting_normalize_up` - Up amount percentage, default `100`.
  `0` leaves the map maximum; `100` moves it to `255`. Used by Up and Both.
- `_sofbuddy_lighting_normalize_down` - Down amount percentage, default `100`.
  `0` leaves the map minimum; `100` moves it to `0`. Used by Down and Both.
- `_sofbuddy_lightblend_src` / `_sofbuddy_lightblend_dst` - manual blend factors
  for Custom. Defaults `GL_ZERO` / `GL_SRC_COLOR`.
- `_sofbuddy_lighting_overbright` - independent **Overbright Lighting**
  toggle. `1` uses the original `2 * L * T` product; `0` uses the selected
  Lighting Style. It never changes `gl_ext_multitexture`. Default `0`.
- `_sofbuddy_lighting_overbright_profile` - overbright-only pre-curve profile:
  `0` Popping highlights, `1` Popping mid+highlight, `2` Preserve Darks only,
  `3` 2x everything (linear identity), `4` Custom, `5` Late highlights
  (default). Selecting a profile writes its values into the overbright curve
  cvars; editing a slider switches to Custom.
- `_sofbuddy_lighting_overbright_curve_0`, `_10`, ... `_100` - the separate
  Overbright Pre-Curve output percentages. Defaults to the **Late highlights**
  profile, `0,5,10,15,20,25,30,35,45,65,100`; **2x everything** restores the
  identity `0,10,20,...,100` and the stock product. Archived independently of
  the Lighting Style curve.
- `_sofbuddy_lighting_cutoff` - dynamic light threshold (default `64`).
- `_sofbuddy_water_size` - water warp size (default `64`, minimum `16`).
- `_sofbuddy_shiny_spherical` - shiny detail-texture mode (default `1`):
  - `0` - Off: skip the shiny/env-map pass entirely
  - `1` - Default: stock behaviour (spherical on `SURF_SIMPLE_SPHERICAL`, plus the flat-surface heuristic)
  - `2` - No spherical: draw every shiny surface with the non-spherical/planar projection
  - `3` - All spherical: force every shiny surface through `GL_SPHERE_MAP`
  - `4` - Both (experimental): draw every shiny surface with both projections; forces the flat threshold to `10.1f` so every surface is also queued for the sphere pass

Spherical depends on **camera position and view angle**. The non-spherical/planar
projection is built from the camera position relative to the surface and a
world-space surface basis, so pure view rotation does not change it.
- `_sofbuddy_lightstyle_interpolation` - smoothly interpolate animated light
  styles between the engine's 10 Hz frames. `1` on, `0` off (default). F12 →
  Lighting → **Smooth Lightstyles**.
- `_sb_internal_lighting_gl_modulate` - non-archived mirror bound to the F12
  **Lightmap Multiplier** slider. The real `gl_modulate` changes only after the
  warning is confirmed. `_sb_internal_lighting_gl_modulate_current` and
  `_sb_internal_lighting_gl_modulate_pending` are the popup's display values.

All persisted CVars use `CVAR_SOFBUDDY_ARCHIVE` and are saved to
`base/sofbuddy.cfg`.

### Commands

- `sofbuddy_lighting_rebuild` - queue a rebuild of the current world lightmaps.
- `sofbuddy_lighting_status` - print the active preset, blend factors, colour
  mode, normalization state/maximum, and custom curve values.
- `sofbuddy_lighting_curve_template <alias>` - write a built-in curve template
  into the 11 slider cvars, select Preserve brightness, and switch to Custom.
- `sofbuddy_lighting_curve_templates` - list the curve-template aliases.
- `sofbuddy_lighting_overbright_curve_reset` - reset the Overbright Pre-Curve
  and profile to **Late highlights**.
- `sofbuddy_gl_modulate_confirm` - apply the pending Lightmap Multiplier value
  and close the warning popup.
- `sofbuddy_gl_modulate_cancel` - discard the pending value, restore the existing
  `gl_modulate`, and close the warning popup.
- `sofbuddy_lighting_curve_reset` - reset the custom 11-point curve to
  linear (`0,10,...,100`). Also available as **Reset Curve (Linear)**.
- `sofbuddy_lighting_reset` - restore the lighting feature's defaults.

## Requirements

- **Curve presets, the Overbright Pre-Curve, and Overbright Lighting work with
  `gl_ext_multitexture 1` and `0`.** Overbright Lighting reads the current value
  and picks the combiner or the legacy framebuffer blend; it never writes the
  user's setting. The pre-curve is upstream of the x2, so it does not need a
  renderer restart.
- **Non-default Custom blend factors require `gl_ext_multitexture 0`.**
  Selecting them saves the current value, sets it to `0`, and flags `vid_ref`
  for a renderer restart. Changing back to a multitexture-safe preset restores
  the saved value and flags one restart. If multitexture was already off, it is
  left off. `_sb_internal_lighting_multitexture_restore` tracks the saved value
  across launches. The Overbright toggle and its pre-curve do not force the
  setting.
- `vid_gamma`, `vid_brightness` and `vid_contrast` still compose normally; the
  curve is applied on top of the game's gamma table.
- `gl_ext_multitexture` is exposed on both **F12 → Lighting** and
  **F12 → Graphics**. Both rows run `vid_restart` after a change so the
  renderer picks the new path up immediately.
- Leave `gl_monolightmap` at `0` (the default). The curve hooks the normal
  full-color lightmap path; the monochrome debug modes bypass it.
