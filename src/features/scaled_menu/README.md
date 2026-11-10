# Scaled Menu

Scales SoF menu content (text, areas, borders, inputs, tickers) by the vertical
design ratio and applies the 4:3→wide frame-height correction while preserving the
authored 640×480 design. This document reflects the current implementation,
including the verified engine semantics behind it.

Requires `UI_MENU` and `RUNTIME_FEATURE_SCALED_MENU`.

## Coordinate model

### Virtual space

Stock SoF menus live on a **640-wide virtual page**. The engine's canvas scale is

```
C = menu_c+0xAC = viddef_width / 640        (menu_c ctor; store at 0xC6298)
```

So the visible vertical virtual space is

```
V = vid_h / C = (vid_h / vid_w) * 640
```

- 16:9 → `V = 360` (1080p: `C = 3.0`)
- 16:10 → `V = 400`
- 4:3 → `V = 480`

### Frames use the stock screen canvas

The stock `frame_c` ctor (0xC60A0) multiplies authored frame W/H by `C`
(`vid_w/640`). Normal menu frames keep that stock canvas scale for width.
`ss_content` pages are the 4:3 page fitted into the narrower 16:9 centre, so their
frame widths are scaled by the same vertical ratio as the frame height.
`menuScale` applies to frame content (text, rects, content borders, offsets); it
must not resize frames. Frame borders scale with the content like rect borders
(see Borders).

The normal frame correction is height-only and resolution-driven: on non-4:3
screens, frames have their height converted from the stock horizontal canvas to
the vertical design ratio (`authored_h × vid_h/480`, implemented as
`stock_h × (vid_h/480) / C`). A zero authored height is left alone. `ss_content`
pages use the same ratio for width, which fits the 4:3 page into the narrower
16:9 centre. This restores the original 4:3→wide fit without tying frames to
menuScale.

The old conversion to `authored × menuScale` has been removed: it made frames
shrink and drift at menu scales below `C` (for example 800×600 with a 1.19 menu
scale) and broke `m_join` and other framed menus. At 4:3 the height factor is 1.0,
so frame geometry is identical at 1.0 and 1.19.

SoFPlus's `_ss` menu variants are deprecated: any `_ss` menu name is redirected to
its `_ns` variant at load time (toke hooks plus an FS-path fallback), so the frame
correction applies to them like any other 4:3-authored page.

### Internal F12 and loading menus

The embedded `sof_buddy/*` pages keep the stock frame canvas (`authored × C`)
while their text, rects and layout continue to use `menuScale`; the F12 redesign
authors the tab grid and content spacers in 640×480 units so the two tab rows and
page body stay together at the source resolution. Internal `loading/*` pages are
not excluded from the normal frame height correction: the loading page is a real
scaled menu and must get the same 4:3→widescreen frame-height treatment as stock
loading pages.

### Content is scaled by the vertical design (menuScale)

Everything inside the frames — text, area rects, content borders, offsets — is
scaled by **`menuScale`** (tickers use their own split; see Ticker):

- **Max scale**:
  `auto_scale_resolution_ratio()` = `screen_y_scale` = `vid_h / 480`
  (1.0 at 480p height, 2.25 at 1080p).
- **Percentage** (`_sofbuddy_menu_scale_pct`, default `75`): integer `0`–`100`.
  The effective menu scale is
  `1 + (max − 1) × pct / 100`; `0%` = `1×`, `100%` = the vertical max.
  The F12 UI Scale tab uses a percentage slider plus a **Reset to default (75%)**
  button in Smooth mode: `0%` = `1×`, `100%` = `max`, with linear scales in
  between. Values below `2×` round to the nearest integer (`1×` or `2×`); `2×`
  and above stay fractional. If the vertical max itself is below `2×` (e.g.
  800×600 → `1.25×`), Smooth stays at `1×` instead of producing a fractional
  sub-2× scale. Integer mode uses an integer slider bound to the vertical max
  instead. `_sofbuddy_menu_scale` is the computed scale cvar.
  The menu-scale slider defers its cvar update but previews the knob at the
  pending value during the drag, then commits and re-parses the open UI Scale
  page after the mouse button is released; menu rect/frame geometry is baked at
  RMF parse time, so the displayed scale alone cannot resize an already open page.

Frame width always keeps the stock `C` factor. Frames get only the
resolution-driven height correction on non-4:3 screens; this is independent of
`menuScale`. See "Scaling ratios interact".

Rounding cvars:

- `_sofbuddy_scale_round_auto` (default `0`) — snap the effective scale to the
  round step;
- `_sofbuddy_scale_round_ratio` (default `0.25`) — the step; manual `menuScale` is
  always snapped with it;
- `_sb_internal_menu_scale_rounded` — mirror written by `menuscale_change()` for
  the settings UI.

### Scale synchronisation

The `VID_CheckChanges` post-hook (`scaled_ui_refresh_vid_dimensions_from_engine`)
captures the actual video mode into `current_vid_w` / `current_vid_h` and sets
`screen_x_scale` / `screen_y_scale`. `scaled_menu_sync_scale_from_engine()` runs
from the menu font measurement path and prefers those captured values, falling
back to the live `viddef_width` / `viddef_height` (0x40365C / 0x403660) only
before the first `VID_CheckChanges`. It re-derives
`screen_x_scale = vid_w/640`, `screen_y_scale = vid_h/480`, and re-clamps
`menuScale` to the new resolution max. SoFPlus can expose the 16:9 **virtual**
menu height through `viddef_height` while a menu is parsed; using that value would
make `screen_y_scale` `0.75` instead of `2.25` at 1080p (a frame shrink factor of
`0.25` instead of `0.75`) and double-shrink frames, so `current_vid_*` is
preferred. The sync runs before menu layout is measured, so the first menu opened
after startup uses the same scale as later opens. Menus keep the scale they were
parsed with (rect geometry is baked at RMF parse), so a scale change requires
re-opening the menu. `scaled_menu_on_vid_layout_changed()` intentionally does
nothing for open menus.

### Activation and gates

`scaled_menu_uniform_actual_active()` is true only when:

- `RUNTIME_FEATURE_SCALED_MENU` is enabled;
- the effective `menuScale` differs from `1.0` by more than 0.001;

Per context:

- **intro/disclaimer** — Scaled Menu's rect/border/ticker/text scaling is skipped
  (see Intro / disclaimer);
- **per rect** — `noscale` (`rect+0x66`) disables dim/offset scaling;
- **resize pages** — `menu_actual_pixel_scale_active()` returns false when the page
  canvas actually carries a raised scale (`menu_resize_canvas_is_scaled()`:
  `canvas > 1.001 && canvas <= max(menuScale, screen_x_scale, screen_y_scale) * 2`);
- **load/save loadbox** — `loadbox_c::Parse` multiplies its stored width by the
  menu canvas itself. The rect hook therefore gives the loadbox `menuScale / canvas`
  instead of `menuScale`, so the page scales but the save list is not double-scaled
  offscreen. A `loadbox_c::Draw` hook scales/recenters the preview thumbnails and
  shifts the three text lines down with the taller icons; the 128 px cell width and
  32 px row padding in `GetIndices`/`Setup`/`Draw` are patched with `menuScale`.

## Rect fields

### Dimensions and hit-testing

Rect geometry (`W/H`, `OuterW/OuterH`, `xoff/yoff`) is scaled **once at RMF parse
time** and written back into the rect. The stored fields are the single source of
truth for drawing, layout and hit-testing: `rect_c::GetCoords` (0xCF570),
`rect_c::Onscreen` (0xCF6E0) and the engine's cursor line-picker (0xCF780) all read
the same values, so hotspots (`<blank ... key mouse1>`) line up with what is drawn.
There is no per-frame scale/restore pass for rect dimensions; the tooltip hover pass
temporarily rewrites `ox/oy` only (see Tooltips).

Dimension factor:

```
dimension factor = menuScale / widgetScale
```

where `widgetScale = rect+0x1C` (default `1.0`, set by a `<scale N>` tag; the
engine's scale-token parser multiplies that value by the canvas for widget classes
that carry the virtual-offset flags). The parse hook applies:

```
value  = round(value * factor)
outer  = round(value * widgetScale) + 2 * borderPad      // `value` here is the
                                                          // already-scaled value
```

Widgets whose box comes from text measurement keep engine-driven sizes because the
measurement hooks already scale them:

- `<text>` — W/H/Outer from `R_Strlen` / `R_StrHeight`
- `<input>` — box from measured text + border padding
- `<ticker>` — band height from `R_StrHeight` (width is handled by the ticker init)

Resize pages carry their page scale in the raised canvas, but `<text>` and
`<input>` fixed widths/offsets still get the normal menu-scale factor. Image/pic
hitboxes are scaled inside the hover pass so clicks match the canvas-scaled art
without changing their construction or draw state. This applies to every resize
page, with no menu-name/spm-specific filters.

`<hbar>` is picture-sized: its ctor sets W/H from the cap picture and stores the
repeated tile's native W/H, so `<width>`/`<height>` never reach the rect parse
hook. A post-`Parse` hook scales W/H/Outer and the tile pitch by the normal content
factor, and `hbar_c::Draw` (0x200D5050) is replaced with a draw that stretches the
cap and tile pictures to those scaled fields via `Draw_StretchPic`. That keeps the
layout centring, tile count, progress fraction and art size on the same scale. The
bevel (non-picture) form scales its box the same way. If `Draw_StretchPic` is
unavailable, the hbar is left stock.

`<slider>` is picture-sized, so `slider_c::Setup` (0x200D08A0) recomputes W/H and
the bar/button/cap dimensions from native pictures after rect parsing. A post-
`Setup` hook scales those final fields by the same `menuScale / widgetScale`
factor, so the bar, button, caps and flow-layout width all follow menuScale.

`<vbar>` derives its width from the native `misc/vbar`/`misc/vend` cap pictures,
so the authored 16 px width stayed thin. A post-constructor hook scales `W`,
`OuterW`, `xoff`, and the cap picture W/H by `menuScale` (xoff is recomputed
from the frame's right edge), and the draw's bevel
thumb width is scaled through the `Draw_BeveledBox` IAT hook. The vbar height is
frame-derived, so the bar still fills its frame.

`<hr>` is a `setkey_c` object with its own parser; its `width` and `line` tokens
never reach `rect_c::Parse`. A post-parse hook scales the stored width
(`hr+0x8C`) and line thickness (`hr+0x90`) by the normal content factor, with the
same `noscale`, intro/disclaimer and resize-page gates as other widgets.

Rect flag `rect+0x66` (`noscale`) disables all of this for the rect. Ghouls are
excluded (see Ghouls and bghouls).

### xoff / yoff

Verified engine semantics:

1. **Storage.** `rect_c::Parse` (0x200D006D / 0x200D00B9) stores
   `xoff = atol(token) * widgetScale(+0x1C)` (same for yoff) — the authored value is
   already multiplied by the rect's scale.
2. **Main path.** `GetCoords` adds them as **absolute pixels**.
3. **Hard path.** For left/right-aligned (align 2/3) or bolted rects the engine uses
   `sub_200CF350`, where per-axis flags decide:
   ```
   if (rect[+0x63]) x += (int)(canvasScale(+0xAC) * xoff);  // virtual x
   else             x += xoff;                              // absolute
   if (rect[+0x64]) y += (int)(canvasScale(+0xAC) * yoff);  // virtual y
   else             y += yoff;
   ```
4. **Engine-owned offsets.** Bolted center/right items (`+0x62` set, align 1/3) get
   xoff/yoff rewritten every layout by `stm_c::GetValidPosition` (0x200E64DA):
   `xoff = -OuterW/2 * widgetScale`, `yoff = -OuterH/2 * widgetScale`. These are not
   scaled by us; the engine recomputes them from the already-scaled outer dims.

Per-axis parse factor:

```
offset factor = menuScale / (widgetScale * (hard_path && virtual_flag ? canvasScale : 1))
```

For ordinary elements (`widgetScale = 1`, flags clear) this is exactly the effective
`menuScale` (at 100%, `resY/480`). The canvas is divided out when the engine would
multiply the offset itself. Tickers use a different split (see Ticker): `xoff`
canvas-space, `yoff` vanilla/authored.

## Borders

`border_c::Init` (0xC46D0) sets:

- pad (`border P`, `border+0`) = `atol(dimA) * scale`;
- thickness (`border T`, `border+4`) = `atol(dimB)` (raw).

Frame borders and rect borders are both authored in absolute pixels. While
Scaled Menu is active, `hk_menu_border_dims_init` passes `menuScale` for both call
sites, so **pad = authored × menuScale**, and the raw thickness is scaled by
`menuScale` as well.

`border_c::RectFrameDims` (0xC4740, not hooked) copies the rect's stored outer dims
into the border's draw rect, so it sees the scaled values.

## Text and glyph snapping

- Measurement (`R_Strlen` / `R_StrHeight`) is scaled only for menu-module callers
  (function start in `0xC3EF0..0xEB000`), when the feature is enabled and
  `scaled_menu_uniform_actual_active()`.
- The disclaimer uses the intro page-width ratio (`vid_w/640`); during the intro
  cinematic both measurement and glyph scaling return stock `1.0`.
- The scale for a font is snapped to that font's glyph grid:
  `roundf(scale * glyphPx) / glyphPx`. Glyph sizes: title 12, small 6, medium 8,
  interface 18, unknown 12. Measurement and drawing use the same snapped value.
- Glyph vertices scale about the run's own start (`g_menuFontPivotX/Y`), and each
  glyph's horizontal advance is compensated by
  `characterIndex * fontPx * (scale - 1)`. Runs grow in place — no per-glyph warp
  and no translation of the run position chosen by the engine.
- The version stamp `ctext` scales like every other menu text run. Its ctext
  right-clip is bypassed with a full-screen scissor so the scaled stamp stays
  visible on the panel.
- At `menuScale = 1` every text path is identical to vanilla.

### Text alignment (`talign`)

`<text>` supports `talign center` (and consumes `left`/`right` as no-ops; the stock
engine only implements `talign` for input/filebox/serverbox widgets). Scaled Menu
handles the token in the rect parse hook and records the text widget; the
`text_c::Draw` wrapper then measures the label with `R_Strlen` and shifts only that
draw's `xoff` by `(rect W - measured width) / 2` before restoring it. The border and
the cursor hitbox read the original rect fields, so the label is centred inside its
box without moving the navigation target.

The internal `sof_buddy/tabs/*` files are served with `talign center` injected on
every tab `<text>` tag. This replaces the old leading-space padding: a space is a
full glyph advance, so its integer rounding could leave tabs visibly off-centre at
fractional menu scales (for example 800×600 at 1.25×). Draw-time centring uses the
same snapped text scale as the glyphs, so it stays centred at every scale and
resolution.

## Rect parse hook (`width` / `height` / `xoff` / `yoff`)

`rect_c::Parse` is wrapped. For every rect (when scaling is active and the rect has
no `noscale` flag):

- `width` / `height`: scaled by the dimension factor (formula above). Resize pages
  whose canvas was already raised get factor `1`. A ticker's `width` is preserved
  (factor `1`) because the ticker init applies the canvas scale to the viewport.
- `xoff` / `yoff`: scaled by the per-axis offset factor (formula above);
  tickers use their own split — `xoff` canvas-scaled, `yoff` left authored
  (see Ticker).

Because the results are stored, drawing, centring and cursor hit-testing all see the
same geometry.

## Ticker

Engine behaviour in the ticker init/setup method (vtable `+0x28`, `sub_200D1B30`):

```
ticker[0xB0] = R_StrLen(font, text)          // measured run width (hooked -> ×menuScale)
if (bolt anchor(+0x7C) && !noscale(+0x66)) {
    delay(+0xB8) *= canvasScale
    W(+0x20)     *= canvasScale               // OuterW is NOT touched
}
```

`ticker_c::Draw` scrolls with `cycle = W + ticker[0xB0]` and
`textX = x + W - (pos % cycle)`; text is clipped to the ticker box via
`Draw_RenderRect`.

Tickers are a special space: they are usually bolted (`bbolt`/`bolt`) to a **ghoul
part** (e.g. `b_panel_tickertape`), and ghouls/artwork are drawn in the canvas space
(`vid_w/640`), not the `menuScale` content space. Scaled Menu therefore:

- scales ticker `xoff` by the **canvas scale** (`canvasScale / widgetScale`) so the
  box stays over its panel/artwork;
- keeps ticker `yoff` **authored** (vanilla pixels) and then applies a
  half-glyph-growth compensation in the ticker init, sign-matched to the nudge:
  `yoff += sign(yoff) * round(glyphPx * (menuScale - 1) / 2)`, where `glyphPx`
  comes from the ticker font (ticker+0x8C).
- divides the canvas out of either offset when the engine's hard path will multiply
  it (`align` 2/3 or bolted + `+0x63`/`+0x64`);
- preserves the authored `width` at parse; the ticker init multiplies `W` and
  `OuterW` by the canvas only when the engine will not (i.e. no bolt anchor). For
  bolted tickers the engine scales `W` itself and `OuterW` stays authored. Final `W`
  is `authored * canvasScale`; bolted tickers on any wide screen then scale
  `W`/`OuterW`/`xoff` by `target/canvas` (0.75 at 16:9) to match the shrunk
  bghoul. This is driven by resolution, not by `_ns`/`_ss` or whether the page is
  in `ss_content`; it includes the full-screen main menu.
- expands the ticker's own `Draw_RenderRect` scissor to the full screen whenever
  the ticker is bolted to the background art. The ticker draw hook records that
  state, so the IAT hook does not depend on `_ns`/`_ss` or `ss_content`. Clipping
  a bolted tape to a narrower frame made it first appear at the frame edge, in
  front of the ghoul, instead of at the screen edge. The following clip restore
  still uses the frame's real bounds;
- captures the authored `delay` before the stock init and restores it afterwards.
- multiplies the ticker speed (`ticker+0xAC`) by the height-derived 4:3 width scale
  (`vid_h / 480`, e.g. 1080p gives 1440/640 = 2.25) for every non-`noscale` ticker.
  Vanilla's 480p speed therefore scales up with resolution instead of staying at
  the authored value. Stock multiplies the delay by the canvas for
  bolted tickers, so at 1080p a 6 s delay became 18 s and the tape also spawned 3×
  further right of the band. Restoring the authored value keeps the wait-until-entry
  identical at every resolution and leaves the spawn point nearer the band's right
  edge on wider screens.

The measured text width still uses `menuScale`, and glyphs draw at `menuScale`; the
marquee cycle therefore mixes a canvas-sized viewport with text-scale run width.

### Why the ticker needed one more nudge (in plain terms)

The RMF's `yoff -10` was hand-tuned when the letters were 12 px tall. At 2.25×
they are about 27 px tall — roughly 15 px of extra letter. Those extra pixels are
not added equally above and below the old line: the glyphs grow around the spot
where the run starts, so one side gains more than the other and the "middle" of the
visible text ends up a few pixels away from where the artist put it.

Two obvious fixes both miss:

- scaling `yoff` along with the menu (×2.25) dragged the whole line up as well, so
  the tape floated above the band;
- leaving `yoff` untouched kept the old line, but with the taller letters the
  visible middle of the text still sat lower than the vanilla text's middle.

The fix splits the difference exactly: keep the artist's line, then move it by
**half** of the extra letter height, in the same direction the artist's nudge
points (`glyphPx × (scale − 1) / 2`). Half the growth is what it takes to put the
middle of the scaled text on the middle of the vanilla text. With the title font
(12 px) at 1080p that is `-10 + (-8) = -18`, i.e. the tape sits on the band the way
it does in vanilla, just with larger letters.
During the engine's delay the phase is negative, so the tape waits off the right
edge (clipped) and then enters from the band's right edge, scrolling left. Stock
scales that delay by the canvas (6 s becomes 18 s at 1080p); restoring the authored
delay makes the wait before the tape enters the same at every resolution.

## Backdrop-open clip extension

`menu_stm_set_clip` (0xEA2C0; installed only when `intro_scaler` is disabled —
otherwise the intro scaler owns that hook and calls
`scaled_menu_backdrop_clip_extend`) recomputes the clip from the menu free origin
plus frame offsets (global `stm` pages use the menu's global clip dims at
`menu_c+0xA4/+0xA8`) and calls `Draw_RenderRect`.

When the engine computes the clip from `backdrop_c::SetClip` (call site `0xC3F2E`)
and the feature is enabled, the clip is extended to the default frame's canvas
bounds:

- left edge to `0` when the clip would start at or left of the frame canvas edge;
- right edge to `vid_w` when it reaches the default frame's right edge.

Per-menu default-frame bounds are cached in a 12-entry cache keyed by `menu_c`. The
extension is skipped for resize popups and during the intro cinematic.

## Menu images

`gl_findimage_post_callback` scales menu pic sizes: when a `Pic` draw is active and
Scaled Menu is on (and the caller is not `SCR_DrawCrosshair`,
`ExecuteLayoutString` or `SCR_DrawCinemaScope`), `DrawPicWidth`/`DrawPicHeight` =
texture size × `scaled_menu_screen_scale()` (not glyph-snapped). Glyph-grid snapping
applies only to text.

`<pic>`/`<image>` widgets are scaled at `pic_c::Setup` by the content factor. They
are corner-anchored by default, so their top-left stays put.

## Server box

Engine facts:

- `serverbox_c::Parse` (0x200DED80) handles only `column` / `hostname` / `address`;
  everything else (including `height`) falls through to `rect_c::Parse`, so `H` is
  the authored value in **raw pixels** (`OuterH = H * widgetScale + 2 * pad`).
- `serverBox_INit` (0x200DEC90) only supplies a default when `height` is absent:
  `H = 0` and `[+0x30] < 200` → `H = 200`.
- `H` is the list viewport height; rows are laid out with `R_StrHeight`, and the
  engine owns `rect_c::SetHeight` / scrolling.

Scaling policy:

- The `height` token is scaled by `menuScale / widgetScale` like any other rect, in
  every menu. Row pitch also scales with `menuScale`, so the
  number of visible rows and the 4:3 screen fraction are preserved (100 → 225 px at
  1080p = 20.8 %).
- No `SetHeight`-skip or vertical-preserve special cases exist; earlier experiments
  were removed because they changed stock list behaviour and could hide rows.

## Ghouls and bghouls

`<ghoul>`, `<bghoul>` and their `bgbolt` / `gbolt` anchors are staged by the engine.
Their `xoff` / `yoff`, model scale and canvas mapping live in the frame/art space
(canvas scale), not the `menuScale` content space. Scaled Menu does not scale ghoul
rects at all: `menu_actual_rect_dim_factor` returns `1` for the ghoul vtable
(`0x1129E0`), which also skips offset scaling.

This matters for staged menus such as `a_main_ns.rmf`, which builds the main menu
around `<bghoul "b_web_home_ingame" xoff 320 yoff 224 … scale 1.32>`. Without the
exclusion the background ghoul is displaced by `(canvasScale − menuScale) × offset`
(about 240 px left and 168 px up at 1080p), dragging the whole staged menu off
position — the “main menu pushed to the left” symptom.

The menu-load bghoul ratio adjustment multiplies `xoff`, `yoff`, and `scale` by
the vertical-vs-canvas ratio (see `_ss` menus).

The startup intro ghoul is additionally normalised by `intro_scaler` (model scale
and offsets from `vid_w/640`, then the letterbox shift).

## Scaling ratios interact

Frame width keeps the stock `C` (percent-of-`V`) factor. Frames get a height-only
4:3→widescreen correction (`vid_h/480`). `menuScale` affects only their contents.
Staged art — ghouls/bghouls, button anchors, backdrops — follows the canvas space
and is explicitly excluded from content scaling (see Ghouls and bghouls), so the
scene stays anchored while menu areas/text use the vertical scale. `bghoul`
`xoff`/`yoff`/`scale` additionally receive the vertical-vs-canvas ratio at menu load (see
`_ss` menus), which is a ratio correction, not the content scale.

## Intro / disclaimer

Startup **disclaimer** and **intro.ghb** are owned by `intro_scaler`
(`intro_scaler/README.md`). While either is active, Scaled Menu skips its scaling:

- disclaimer: `intro_scaler_disclaimer_menu_text_active()` — menu rect/border/ticker
  scaling is skipped; text uses the intro page-width ratio (`vid_w/640`);
- intro cinematic: `intro_scaler_intro_cinematic_active()` — rect/border/ticker
  scaling is skipped and text measurement/glyphs return stock 1×.

The intro ghoul is normalised by `intro_scaler` itself: model scale and `xoff/yoff`
are set from the page-width ratio (`vid_w/640`), then the letterbox vertical shift is
applied.

## `_ss` menus

SoFPlus's `_ss` menu variants are no longer used because the shipped files have
layout mistakes. Scaled Menu redirects any `_ss` menu name to the matching `_ns`
variant at load time: top-level pages are redirected through the `toke_c` ctor and
included pages through `toke_c::Insert`. The redirect and wide-variant state live
in `menu_variant.cpp`; the FS-path fallback and RMF transforms live in
`internal_menus/hooks/menu_rmf_transform.cpp`. The old `_ss`-specific frame,
border, topic, and join-menu corrections have been removed. `_ns` pages are
authored in 640×480 space and receive the normal frame height correction described
above.

When SoFPlus selects the `_ss` variant (its `_spf_cl_video_shortscreen`
wide-screen test) and the menu is in the whitelist, the FS loader replaces the
outer wrapper's `<exinclude _spf_cl_video_shortscreen ss ns>` with:

```rmf
<frame 16_9_center 0 0>
<frame 16_9_left <side> 0 page 16_9_l cut 16_9_center>
<frame ss_content <main> 0 page <ns> cut 16_9_center>
<frame 16_9_right <side> 0 page 16_9_r cut 16_9_center>
```

`main = ((vid_h * 4/3) / vid_w) * 640` (480 at 16:9) and
`side = (640 - main) / 2` (80 at 16:9). The real `_ns` page is loaded into
`ss_content` and is not chrome-injected. `16_9_center` is full-screen (`0 0`);
the side frames and `ss_content` are cut to it and laid out as
side/main/side. There is no `16_9_bottom`. Because page frames without a
`cut` link to the menu's default frame, the FS hook also adds `cut ss_content` to
the matching `_ns` page's uncut frames, so they become children of `ss_content`.
The `_ns` page's frames receive the global vertical frame shrink, and their
widths are scaled by the same ratio so the 4:3 layout fits the centre. `bghoul` backgrounds are adjusted globally at menu-load time,
not just for `_ss` menus: their authored `xoff`, `yoff` and `scale` are multiplied
by the same vertical-vs-canvas ratio (`(vid_h/480) / (vid_w/640)`), so they track
the frame at any wide ratio. All `_ns` frame heights, including the topic strip, keep
their authored values so the global vertical frame shrink applies to them. The
menu content pages remain the `_ns` versions; no `_ss` content is loaded. The
chrome injection is applied only to a whitelist of standard wrappers (`join`,
`keys`, `game`, `lock`, `bots`, `chat`, `load`, `mail`, `misc`, `motd`, `save`,
`audio`, `stats`, `game2`, `game3`, `scale`, `socks`, `start`, `video`, `outfit`,
`advjoin`, `credits`, `dmflags`, `options`, `pickbot`, `joystick`, `objectives`,
`testmotd`, `language`, `player`). Menus outside the whitelist (for example
`a_main_ns`) and all other content pages keep their `_ns` layout unchanged.

## Tooltips

Engine pipeline:

- the `stm_c` hover pass (`0x200E9F30`) reads the frame-local mouse and, for each rect,
  tests `mx ∈ [ox, ox+OuterW−1]`, `my ∈ [oy, oy+OuterH−1]`. On a hit it calls
  the hover marker (`sub_200CEB60`), which stores the hovered rect in `stm+0x34` /
  `menu_c+0xBC`.
- When the hovered rect changes and it has a `tip` string (`rect+0x6C`), the engine
  calls `menu_c::CreateTip` (`0x200CA9D0`, reached via `sub_200C6A50`). It allocates
  a 0x24-byte `tip_c` and stores:
  ```
  tip.x = menu_system_c::GetMouseX() − menu_c->freeX(+0x9C)
  tip.y = frame->offY(+0x44) + menu_c->freeY(+0xA0) + parentChainY(frame)
          + rect->oy(+0x10) + rect->OuterH(+0x2C) − stm->y(+0x80)
  ```
- `tip_c::Render` (`0x200EA8A0`) draws at `screenX = tip.x + freeX` (exactly the
  mouse X) and `screenY = tip.y`, with the box/text measured by
  `R_StrLen`/`R_StrHeight` (hooked → ×menuScale), clamped to `vid_w`.

Why it broke with scaling: the hover test compares `ox/oy` against the mouse, but
the drawn rect is `GetCoords` which also adds `xoff/yoff` (now scaled ×menuScale).
So the hover box and the visible item diverge at higher scales and tooltips fired
while the cursor was away from the drawn item. The vanilla tip anchor also ignores
`xoff/yoff`, the `−[rect+0x80]` align term and the border pad, and mixes
canvas-space (`frame offY`, `freeY`) with menuScale-space (`oy`, `OuterH`) values,
so the tooltip drifted at different aspect ratios.

Fix:

- `hk_stm_hover_pass` (`0x200E9F30`) wraps the engine's per-frame hover pass. While
  it runs, `hk_rect_hover_prep` (`0x200CEB40`) rewrites each rect's `ox/oy`
  (the values the inline test reads) to
  `GetCoords(rect) − K`, where `K` is the pass's own screen→frame-local mouse
  offset (`menu_system::GetMouseX/Y − frame_c::GetMouseX/Y`, plus `stm+0x80` for
  Y). The engine's inline test then covers the **drawn** rectangle, including the
  scaled `xoff/yoff`, align term and border pad. After the pass returns, all
  `ox/oy` values are restored, so drawing/layout never see the adjustment.
- `hk_menu_create_tip` (`0x200CA9D0`) re-anchors the created tip to the drawn rect:
  `tip.y = GetCoords().y + OuterH` (screen space). `tip.x` stays the engine's
  mouse-based value, so the tooltip appears at the cursor horizontally and just
  under the drawn item.
- The engine does not draw a fresh tip until `elapsed > duration/4` (3.75 s at the
  Buddy `tip_duration` default of 15000). `hk_menu_create_tip` records the new
  `(menu, tip, source rect)` and rewrites `tip->start(+0x18)` to
  `created − duration/4 + _sofbuddy_menu_tip_appear`, so the tooltip appears after
  exactly the configured delay (default **750 ms**, `0` = immediate; the stock
  delay is `duration/4`). The fade-out threshold moves with the start, so the
  visible time is unchanged.
- The value is exposed as **Menu tooltip Appear (ms)** in F12 → General → Menu.
  The tooltip font is selectable there as **Tooltip font** (Small default, Medium,
  Title) and applies when the menu is reopened. **Menu font size style**
  (Small/Normal/Large/Extra Large) selects the F12 font-tier template; F12 is
  reparsed so the authored `<font type>` tags stay consistent.
- `<pic>`/`<image>` widgets are scaled at `pic_c::Setup` (`0x200D2290`) by the same menu-scale factor as other actual-pixel widgets; `noscale` images, bolted tab icons, images with explicit `width`/`height` (already scaled by rect parse), and resize canvases that already carry the scale are skipped.
- `hk_stm_hover_pass` records each `stm_c`'s current hovered rect (`stm+0x34`)
  after the stock hover pass, keyed by the `stm_c*` and a generation counter.
  `hk_menu_create_tip` stores the source rect's `stm_c*` and generation;
  `hk_tip_render` compares against that stm's latest hovered rect. This avoids
  the stock `menu+0xBC` cache, which is overwritten by every stm pass and can
  be null or point at another frame's hover even while a tip is valid.
- If the tip's stm has not run a hover pass since the tip was created, the hook
  falls back to the old `menu+0xBC` check. The hook only reads engine state and
  skips the render call — it never writes menu/tip fields or frees the tip.

No validation/rejection is used: the hitbox itself now matches the drawing.

## Stubborn bug: direct `oM_PushMenu` calls bypass the hook dispatcher

This one cost a long debugging session and is easy to reintroduce.

Symptom: opening F12 before the stock main menu had ever loaded put the Buddy
nav above the viewport; opening any stock menu first (`menu main`, `menu a`,
`menu join`, etc.) made F12 correct. With Scaled Menu enabled **or disabled**,
and with frame W/H translation enabled or disabled, the problem remained. A
standalone copy of the same RMF opened with the stock `menu <file>` command was
always correct.

Root cause: `detour_M_PushMenu::oM_PushMenu` is the **original/trampoline**
function, not the patched entry point. The stock `menu_f` command calls the
patched `M_PushMenu` entry, so the hook manager runs the registered pre
callbacks, the original body, and the post callbacks. `Cmd_SoFBuddy_Menu_f`
called `oM_PushMenu(...)` directly, which skipped the entire hook layer.

The post callback that matters is `internal_menus_M_PushMenu_post`: after a
`sof_buddy` push it fetches the current `menu_c` and zeroes `menu_c+0x94`, `+0x98`,
`+0x9C`, and `+0xA0` (the menu free/scroll offsets used to position the page). A
startup or loading menu leaves those offsets non-zero, which shifts the whole F12
page upward and puts the nav above the viewport. The direct push never ran that
callback, so the stale offsets survived. The stock `menu` path, a missing-menu
push, and `menu join` all enter through the patched entry and therefore either
run the reset or tear the page down; that is why they appeared to “fix” F12.

`scaled_menu_M_PushMenu_pre` is skipped on the direct path too, so
`s_pushed_menu_name`, the `_ss` wide-variant flag, and `s_skip_vertical_shrink` can
carry stale state into the F12 parse. The skipped post-hook offset reset is the
consistent failure across Scaled Menu on/off.

Fix: never call `oM_PushMenu` directly from the Buddy menu command. Route the
resolved page through the patched entry, currently:

```cpp
Cmd_ExecuteString("menu <resolved-page>");
```

Any future code that needs to push a Buddy page must go through
`Cmd_ExecuteString("menu ...")` (or an equivalent wrapper that calls the
patched `M_PushMenu` entry), not `detour_M_PushMenu::oM_PushMenu`.

## Hooks

| Hook target | RVA | Purpose |
|---|---:|---|
| `M_PushMenu` | 0xC7630 | normalize `_ss`→`_ns` pushed name; reset per-menu frame/scale state |
| `menu_stm_set_clip` | 0xEA2C0 | clip recompute + backdrop-open extension (only when `intro_scaler` is disabled) |
| `menu_border_dims_init` | 0xC46D0 | rect borders: `menuScale` pad/thickness; frame borders: vertical ratio |
| `rect_c::Parse` | 0xCF890 | persistent scaling of `width`/`height`/`xoff`/`yoff`, file overrides, `<text talign>` |
| `text_c::Draw` | 0xD8B70 | draw-time `talign center` offset (border/hitbox unchanged) |
| `frame_c::Resize` | 0xC6640 | resize-page canvas/frame scale + recentre |
| `frame_c` ctor | 0xC60A0 | record authored frame W/H and per-menu shrink state |
| `frame_c` parent link/cut | 0xC6530 | height-only 4:3→widescreen frame correction (once per frame); original link |
| `toke_c` ctor | 0xEAC70 | track menu source; redirect `_ss` pages to `_ns` |
| `toke_c::Insert` | 0xEAF60 | track menu source; redirect `_ss` includes to `_ns` |
| ticker init (vtable `+0x28`) | 0xD1B30 | ticker `W`/`OuterW` canvas normalisation, wide-screen box scale, speed scale |
| `ticker_c::Draw` | 0xD1C80 | mark bolted tickers while drawing |
| `Draw_RenderRect` IAT | 0x4035FC | expand bolted-ticker scissor to full screen |
| `pic_c::Setup` | 0xD2290 | scale pic dimensions for normal menus |
| `vbar_c` ctor | 0xD5250 | scale vbar W/OuterW/xoff + cap picture W/H |
| vbar thumb immediates | 0xD5976 / 0xD5979 | scale/center bevel thumb |
| `loadbox_c::Draw` | 0xDC250 | scale/recenter save previews + shift text |
| loadbox layout immediates | 0xDBF21 / 0xDC9D9 / 0xDC002 / 0xDC9F0 / 0xDC4E9 | scale loadbox cell/row layout |
| `hbar_c::Parse` | 0xD51D0 | scale hbar W/H/Outer + tile pitch |
| `hbar_c::Draw` | 0xD5050 | stretch cap/tile pictures to scaled fields |
| `slider_c::Setup` | 0xD08A0 | scale slider bar/button/cap fields |
| `stm_c` hover pass | 0xE9F30 | save/restore rect `ox/oy` around the engine hover pass |
| rect hover prep | 0xCEB40 | rewrite `ox/oy` to the drawn position during the hover pass |
| `menu_c::CreateTip` | 0xCA9D0 | re-anchor tooltip to the drawn rect bottom; record source rect |
| `tip_c::Render` | 0xEA8A0 | suppress drawing when the source stm no longer hovers the source rect |

Font measurement and glyph scaling are handled by `scaled_ui_base` (`R_DrawFont`,
`R_Strlen`, `R_StrHeight`); menu images by its `GL_FindImage` hook. At
`menuScale = 1`, content scaling is identical to vanilla; the frame-height
correction still applies on non-4:3 screens.

Re-open menus after changing the scale so the RMF is parsed with the new value.
