# Scaled Menu

Scales SoF menu content (text, areas, borders, inputs, tickers) and frame boxes to
actual screen pixels while preserving the authored 640×480 design. This document
reflects the current implementation, including the verified engine semantics behind it.

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

### Frames follow the authored 480p design

The stock `frame_c` ctor (0xC60A0) multiplies authored frame W/H by `C`
(`vid_w/640`). Scaled Menu hooks the ctor and, for menus that are **not** `_ss`,
replaces that factor with `menuScale` (`resY/480`, the same factor as content):

```
non-_ss: frame W/H = authored * menuScale
_ss:     frame W/H = authored * C      (stock)
```

A 640×480 frame authored at 480p therefore becomes 1440×1080 at 1080p (4:3 fitted
to the screen height) instead of 1920×1440 (cropped). A dimension of 0 keeps the
engine's inherit/split behaviour. The source file is tracked through `toke_c`
construction and `toke_c::Insert` (include/exinclude); a frame stays stock if its
source name ends in `_ss` **or any ancestor frame is `_ss`** (propagated in the
frame parent-link hook). This also covers non-`_ss` pages that include `_ss`
content (e.g. `m_join.rmf` → `m_join_ss.rmf`) and frames cut from an `_ss` parent.
The `game_ss` override is still applied at frame finalize.

### Internal F12 and loading menus

The embedded `sof_buddy/*` and `loading/*` pages are excluded from the frame
conversion. They keep the stock frame canvas (`authored × C`) while their text,
rects and layout continue to use `menuScale`; the F12 redesign authors the tab
grid and content spacers in 640×480 units so the two tab rows and page body stay
together at the source resolution. This is the conservative path used for the
in-game F12 overlay; a dedicated frame-scale pass can be evaluated separately.

### Content is scaled by the vertical design (menuScale)

Everything inside the frames — text, area rects, borders, offsets — is scaled by
**`menuScale`**:

- **Max scale**:
  `auto_scale_resolution_ratio()` = `screen_y_scale` = `vid_h / 480`
  (1.0 at 480p height, 2.25 at 1080p).
- **Manual** (`_sofbuddy_menu_scale`): clamped to 0.25–max and snapped with
  `round_scale_value()`; a value of ~1 is forced to exactly `1.0`
  (**1 = vanilla actual pixels**). Values above the max use the exact max scale.
  The F12 UI Scale tab adds **Auto** (`round(max × 0.75)`), **Max** (exact max),
  and **Min** (`1`) preset buttons next to the 0.25-step list.

Non-`_ss` frames use `menuScale` (the vertical design), so frame boxes and their
contents share the same space. `_ss` frames keep the stock `C` factor because they
are already authored for the wide layout; see "Scaling ratios interact".

Rounding cvars:

- `_sofbuddy_scale_round_auto` (default `0`) — snap the effective scale to the
  round step;
- `_sofbuddy_scale_round_ratio` (default `0.25`) — the step; manual `menuScale` is
  always snapped with it;
- `_sb_internal_menu_scale_rounded` — mirror written by `menuscale_change()` for
  the settings UI.

### Scale synchronisation

`scaled_menu_sync_scale_from_engine()` reads the engine's live
`viddef_width` / `viddef_height` (0x40365C / 0x403660), sets
`screen_x_scale = vid_w/640`, `screen_y_scale = vid_h/480`, and re-clamps
`menuScale` to the new resolution max. It runs from the `VID_CheckChanges`
post-hook and from the first menu font measurement, so the first menu opened after
startup uses the same scale as later opens. Menus keep the scale they were parsed with (rect geometry is
baked at RMF parse), so a scale change requires re-opening the menu.
`scaled_menu_on_vid_layout_changed()` intentionally does nothing for open menus.

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
  `canvas > 1.001 && canvas <= max(menuScale, screen_x_scale, screen_y_scale) * 2`).

## Rect fields

### Dimensions and hit-testing

Rect geometry (`W/H`, `OuterW/OuterH`, `xoff/yoff`) is scaled **once at RMF parse
time** and written back into the rect. The stored fields are the single source of
truth for drawing, layout and hit-testing: `rect_c::GetCoords` (0xCF570),
`rect_c::Onscreen` (0xCF6E0) and the engine's cursor line-picker (0xCF780) all read
the same values, so hotspots (`<blank ... key mouse1>`) line up with what is drawn.
There is no per-frame scale/restore pass.

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

The **join** menu is a scoped exception: while `menu join` is current and the
menu scale is above 1, its content scale is `max(0.75 × menuScale, 1)`. The override is tied to that menu’s `menu_c` and does
not leak onto the menu revealed when it closes.

`<slider>` is picture-sized, so `slider_c::Setup` (0x200D08A0) recomputes W/H and
the bar/button/cap dimensions from native pictures after rect parsing. A post-
`Setup` hook scales those final fields by the same `menuScale / widgetScale`
factor, so the bar, button, caps and flow-layout width all follow menuScale.

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

For ordinary elements (`widgetScale = 1`, flags clear) this is exactly `resY/480`.
The canvas is divided out when the engine would multiply the offset itself. Tickers
use a different split (see Ticker): `xoff` canvas-space, `yoff` vanilla/authored.

## Borders

`border_c::Init` (0xC46D0) sets:

- pad (`border P`, `border+0`) = `atol(dimA) * scale`;
- thickness (`border T`, `border+4`) = `atol(dimB)` (raw).

While Scaled Menu is active, `hk_menu_border_dims_init` passes `menuScale` as the
scale argument (the engine's canvas-scale argument is ignored), so **pad = authored
× menuScale**. Afterwards the thickness is multiplied by `menuScale`. Frame borders
are created through the same path and follow the same rules.

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
- The small black version stamp (cvar text starting with a digit and containing a
  dot) is kept fully stock: `R_Strlen`, glyph scaling and the ctext's centring are
  bypassed, and its draw is given a full-screen scissor. The scaled ctext right-clip
  otherwise cut the stock-sized stamp out of the green panel.
- At `menuScale = 1` every text path is identical to vanilla.

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

Engine behaviour at the end of `ticker_c::Parse` (vtable `+0x28`, `sub_200D1B30`):

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
  is `authored * canvasScale`;
- captures the authored `delay` before the stock init and restores it afterwards.
- multiplies the ticker speed (`ticker+0xAC`) by the height-derived 4:3 width scale (`vid_h / 480`, e.g. 1080p gives 1440/640 = 2.25), so scroll speed stays a fixed percentage of that content width per second at any resolution.
  Stock multiplies the delay by the canvas for bolted tickers, so at 1080p a 6 s
  delay became 18 s and the tape also spawned 3× further right of the band. Restoring
  the authored value keeps the wait-until-entry identical at every resolution and
  leaves the spawn point nearer the band's right edge on wider screens.

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
  every menu (including `_ss` files). Row pitch also scales with `menuScale`, so the
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

This matters for staged menus such as `amain.rmf`, which builds the main menu around
`<bghoul "b_web_home_ingame" xoff 236 yoff 170 … scale 0.97>`. Without the exclusion
the background ghoul is displaced by `(canvasScale − menuScale) × offset` (about
177 px left and 127 px up at 1080p), dragging the whole staged menu off position —
the “main menu pushed to the left” symptom.

The startup intro ghoul is additionally normalised by `intro_scaler` (model scale
and offsets from `vid_w/640`, then the letterbox shift).

## Scaling ratios interact

Non-`_ss` frames use `menuScale` (`resY/480`) just like their contents, so frame
boxes and menu areas share the vertical design space. `_ss` frames keep the stock
`C` (percent-of-`V`) factor because they are already authored for the wide layout.
Staged art — ghouls/bghouls, button anchors, backdrops — follows the canvas space
and is explicitly excluded from content scaling (see Ghouls and bghouls), so the
scene stays anchored while menu areas/text use the vertical scale.

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

Menus whose file name ends in `_ss` (e.g. `game_ss.rmf`, `m_join_ss.rmf`,
`armoryticker_ss.rmf`) are manually authored for wide aspect ratios. Only their
**frame dimensions** are kept out of the feature's scaling; **all content scales
normally**, exactly like any other menu:

- rect dimensions/offsets, borders, rect-parse fixed dims, text measurement/glyphs
  and ticker normalisation are **not** disabled for `_ss` menus;
- frame W/H conversion is skipped for `_ss` files: they keep the stock
  authored × `C` dimensions, while non-`_ss` frames use the vertical `resY/480`
  factor. The only other frame mutation is the explicit `game_ss` correction below;
- the `M_PushMenu` pre-hook still normalises and records the pushed name (leaf,
  `.rmf` stripped, lowercase) because the frame correction keys off it.

### `game_ss.rmf` override

`game_ss.rmf` contains `<frame topic 0 92 page t_game center>`; the correct height is
96. A frame-finalize hook (`0xC6500`, just before the engine snapshots frame dims
into the border rect and before `frame_c::CreatePage`) rewrites only that case:

```
if (pushed menu == "game_ss" && frame name == "topic")
    frame->H = (int)(96.0f * canvasScale)
```

## Tooltips

Engine pipeline:

- `stm_c::Render` (`0x200E9F30`) reads the frame-local mouse and, for each rect,
  tests `mx ∈ [ox, ox+OuterW−1]`, `my ∈ [oy, oy+(OuterH+oy)−1]`. On a hit it calls
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
- The engine does not draw a fresh tip until `elapsed > duration/4` (2.5 s at the
  Buddy `tip_duration` default of 5500). The same hook moves `tip->start(+0x18)` back by
  `duration/4 − _sofbuddy_menu_tip_appear`, so the tooltip appears after the
  configured delay (default **250 ms**, `0` = immediate, `2500` = stock). The
  fade-out threshold moves with the start, so the visible time is unchanged. The
  value is exposed as **Menu tooltip Appear (ms)** in F12 → General → Menu. The tooltip font is selectable there as **Tooltip font** (Small default, Medium, Title) and applies when the menu is reopened. **Menu font size style** (Small/Normal/Large/Extra Large) selects the F12 font-tier template; F12 is reparsed so the authored `<font type>` tags stay consistent.

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

The post callback that matters is `internal_menus_M_PushMenu_post`: after the
push it fetches the current `menu_c` and zeroes `menu_c+0x94`, `+0x98`, `+0x9C`,
and `+0xA0` (the menu free/scroll offsets used to position the page). A startup
or loading menu leaves those offsets non-zero, which shifts the whole F12 page
upward and puts the nav above the viewport. The direct push never ran that
callback, so the stale offsets survived. The stock `menu` path, a missing-menu
push, and `menu join` all enter through the patched entry and therefore either
run the reset or tear the page down; that is why they appeared to “fix” F12.

`scaled_menu_M_PushMenu_pre` is skipped on the direct path too, so
`s_pushed_menu_name`, `s_menu_source_is_ss`, and `s_menu_skip_frame_convert`
can carry stale `_ss`/frame-filter state into the F12 parse. The skipped
post-hook offset reset is the consistent failure across Scaled Menu on/off.

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
| `M_PushMenu` | 0xC7630 | record pushed menu name (used by the `game_ss` override) |
| `menu_stm_set_clip` | 0xEA2C0 | clip recompute + backdrop-open extension (only when `intro_scaler` is disabled) |
| `menu_border_dims_init` | 0xC46D0 | border pad with `menuScale`, linewidth post-scale |
| `rect_c::Parse` | 0xCF890 | persistent scaling of `width`/`height`/`xoff`/`yoff`, file overrides |
| `frame_c::Resize` | 0xC6640 | resize-page canvas scale + recentre |
| `frame_c` ctor | 0xC60A0 | record authored 480p frame dims and source `_ss` state |
| `frame_c` parent link/cut | 0xC6530 | convert non-`_ss` frame dims; inherit `_ss` from parent |
| `frame_c::ParseFrame` finalize | 0xC6500 | `game_ss` `topic` height override |
| `toke_c` ctor | 0xEAC70 | track the menu source file name |
| `toke_c::Insert` | 0xEAF60 | track `_ss`/`_ns` includes |
| ticker init (vtable `+0x28`) | 0xD1B30 | ticker `W`/`OuterW` canvas normalisation |
| `stm_c` hover pass | 0xE9F30 | save/restore rect `ox/oy` around the engine hover pass |
| rect hover prep | 0xCEB40 | rewrite `ox/oy` to the drawn position during the hover pass |
| `menu_c::CreateTip` | 0xCA9D0 | re-anchor tooltip to the drawn rect bottom |

Font measurement and glyph scaling are handled by `scaled_ui_base` (`R_DrawFont`,
`R_Strlen`, `R_StrHeight`); menu images by its `GL_FindImage` hook. At
`menuScale = 1` every path is identical to vanilla.

Re-open menus after changing the scale so the RMF is parsed with the new value.
