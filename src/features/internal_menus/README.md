# internal_menus

`internal_menus` embeds RMF files in the DLL and routes menu opens through `M_PushMenu`.

**The main mechanism: `FS_LoadFile` is intercepted on every menu file load. The path and filename of the requested `.rmf` are normalized and looked up in the embedded menu map; internal menus take priority over the filesystem—if the path matches an embedded menu, we serve it from memory and never call the original `FS_LoadFile`.**

The feature stays event-driven:
- `sofbuddy_menu <name>` pushes menus directly (no queue/retry loop).
- Loading/status/layout cvars are updated on events (not per-frame polling in this feature).

## Architecture

### 1) Embedded menu library
`menu_data.cpp` is generated from:

```text
src/features/internal_menus/menu_library/<menu_name>/*.rmf
```

At runtime `internal_menus_load_library()` fills:

```cpp
g_menu_internal_files[menu_name][filename]
```

### 2) `sofbuddy_menu` command path
Internal menus can be opened without `sofbuddy_menu` (e.g. SCR_BeginLoadingPlaque or `loading_show_ui()` push `loading/loading` via `M_PushMenu`; the FS_LoadFile override serves them from memory either way). The `sofbuddy_menu` command is a convenience that adds behavior the engine's `menu` command does not provide:

- **Layout cvars synced before open** — `update_layout_cvars(false)` so scaling and centering are correct for the current video mode.
- **Validation and resolution** — Name is sanitized and resolved against the embedded menu set only (`g_menu_internal_files`); invalid or missing paths are rejected. Converts to `<menu>/<page_stem>` (e.g. `sof_buddy/main`, `loading/loading`) with fallback to `main.rmf` for menu roots.
- **Replace top page for non-sofbuddy** — For targets other than `sof_buddy/*`, runs `killmenu` first so the new menu replaces the current one and ESC behavior is predictable.
- **Loading lock** — For `loading/*`, uses `_sofbuddy_loading_lock_input` for lock behavior. The loading current map name (`_sb_internal_loading_current`) is set to `"resolving..."` only in the loading reset state (http_maps `http_maps_clear_loading_cvars`), not here.

`sof_buddy/*` targets rely on `<stm nopush>` in wrapper pages, so tab changes do not grow the stack. Use `sofbuddy_menu` for Buddy tab/page navigation so layout is re-synced and paths stay correct.

Direct sub-page form is supported:

```text
sofbuddy_menu <menu>/<page>
```

### 4) Serving embedded menus (`FS_LoadFile` override)
**Internal menus have priority over the filesystem:** the override intercepts the path and filename of the requested `.rmf`, normalizes them, and looks up `menu_name`/`filename` in `g_menu_internal_files`. On a match, the hook serves the in-memory content (via engine `Z_Malloc`) and does not call the original; otherwise it falls through to the original `FS_LoadFile`.

### 5) Loading and menu-disclaimer (start) hooks
**SCR_BeginLoadingPlaque**: At EarlyStartup we NOP 5 bytes at exe 0x13AC7 (engine’s menu push) and 5 at 0x13ACE (SCR_UpdateScreen call), so the original does not show its own loading UI. **SCR_BeginLoadingPlaque** Post (`internal_menus_SCR_BeginLoadingPlaque_post`): when `!noPlaque`, we run killmenu, `oM_PushMenu("loading/loading", "", lock_input)`, then `SCR_UpdateScreen(true)` (exe 0x15FA0). Loading is only pushed by us (this hook and `loading_show_ui()` from http_maps). The `"resolving..."` label is set in the loading reset state (http_maps), not here.

**M_PushMenu** Pre (`internal_menus_M_PushMenu_pre`): Normalizes menu name; only handles menu-disclaimer/update-prompt: when the engine pushes the menu disclaimer (root `start`) and the updater queued a startup prompt, rewrites to `sof_buddy/update_prompt`. No loading-signal detection or rewrite.

**M_PushMenu** Post (`internal_menus_M_PushMenu_post`): Watches menu disclaimer (root `start`) pushes; if updater queued a startup update prompt, opens `sof_buddy/update_prompt` on top once.

### 6) Loading UI cvar updates
Loading UI is fed by direct helpers:
- `loading_set_current(...)`

### 6.5) Cvar naming: user-facing vs `_sb_internal_`
- `_sofbuddy_*` = user-facing settings. Every one of them appears on **F12 → Cvars** (`cvars_content.rmf`) with live value, default and description, and is mirrored in the root `README.md` cvar table.
- `_sb_internal_*` = sof_buddy's own bookkeeping: layout math, migration guards, runtime status read-outs. These are hidden from the Cvars page because editing them has no lasting effect (they are recomputed on launch). See the internal table in the root `README.md`.
- RMF pages may still *read* internal cvars (`<ctext _sb_internal_update_status ...>`, `<includecvar _sb_internal_update_release_list_rmf>`), so the rename must be applied to both C++ and `menu_library/*.rmf`.

### 7) Dynamic layout cvars (SoF Buddy tabs/centering)
Created in `PostCvarInit` and recomputed on vid changes:
- `_sb_internal_menu_vid_w`, `_sb_internal_menu_vid_h`
- `_sb_internal_center_panel_px`
- `_sb_internal_tabs_row1_prefix_px`, `_sb_internal_tabs_row2_prefix_px`
- `_sb_internal_tabs_row1_prefix_rmf`, `_sb_internal_tabs_row2_prefix_rmf`
- `_sofbuddy_loading_lock_input` (`CVAR_SOFBUDDY_ARCHIVE`, default `0`)
- `_sofbuddy_menu_hotkey` (`CVAR_SOFBUDDY_ARCHIVE`, default `F12`). The open key is shown and rebound via RMF `<setkey "sofbuddy_menu sof_buddy" ...>` (Input and main); bind mode updates the cvar.
- `_sofbuddy_perf_profile` (`CVAR_SOFBUDDY_ARCHIVE`, default `0`, used by Perf T profile list)
- Tunables: `_sb_internal_tabs_row1_content_px`, `_sb_internal_tabs_row2_content_px`, `_sb_internal_tabs_center_bias_px`, `_sb_internal_tabs_row1_bias_px`, `_sb_internal_tabs_row2_bias_px`

When video size changes, `update_layout_cvars(true)` recomputes runtime layout cvars used by tab-prefix `includecvar` blocks.

## Hooks/callbacks used

From `hooks/hooks.json`:
- `FS_LoadFile` (override): serve embedded RMF from `g_menu_internal_files` when path matches menu_library; otherwise fall through to original.
- `M_PushMenu` Pre: menu-disclaimer/update-prompt only (rewrite `start` to `sof_buddy/update_prompt` when updater queued); no loading signals.
- `M_PushMenu` Post: on menu disclaimer (`start`) push, consume queued startup-update prompt request and open prompt once.
- `SCR_BeginLoadingPlaque` Post: when `!noPlaque`, push `loading/loading` and call `SCR_UpdateScreen(true)` (engine’s own push/update are NOP’d at 0x13AC7 / 0x13ACE).

From `callbacks/callbacks.json`:
- `EarlyStartup` (Post): load embedded library; NOP exe 0x13AC7 and 0x13ACE; resolve SCR_UpdateScreen at 0x15FA0.
- `PostCvarInit` (Post): register runtime cvars; register `sofbuddy_menu`, `sofbuddy_apply_menu_hotkey`, and Perf profile apply commands; bind `_sofbuddy_menu_hotkey` to `sofbuddy_menu sof_buddy`.

Cross-feature integration:
- `scaled_ui_base` calls `internal_menus_OnVidChanged()` from `vid_checkchanges_post()` so layout cvars stay in sync with current vid mode.

## Usage

```text
sofbuddy_menu loading
sofbuddy_menu sof_buddy
sofbuddy_menu sof_buddy/cpu
sofbuddy_menu sof_buddy/network
sofbuddy_apply_menu_hotkey
```

## Menu library

**CPU tab** (`cpu_content.rmf`) holds the frame-pacing controls: a **Framerate** section with a
`cl_maxfps` `<list>` (`Auto` + `ceil(1000/n)` for `n = 1…16, 20, 25, 30, 40, 100ms`, `cvar` + `match`), with no
display-refresh-specific FPS presets, plus a `gl_swapinterval` VSync `Off`/`On` list,
and `cl_maxfps 30` / `gl_swapinterval 0` folded into "Restore Perf Defaults". Below VSync, a live `Display` readout refreshed on every CPU-page open by `vsync_toggle`
shows a request picker generated from the modes the driver reports for the current
resolution (`Requested Display Refresh Rate`, `Auto`/`0` first; selecting a rate runs
`vid_restart`),
then the measured system truth (`Actual Display Refresh Rate`, theme-accent readout tint since it
is display-only); a static
`Fullscreen` row (`vid_fullscreen`); a sofplus-only `Window Border` row (`_sp_cl_vid_border`,
generated only when `findCvar` proves the cvar exists *and* the game is windowed — bible
`<cinclude>` tests values, not existence, so it can't do this job); and a mismatch warning (orange, via RMF `<cinclude>` on a
`_sb_internal_fps_cap_mismatch` flag) that appears only when VSync is on and the display could
deliver more than the cap allows. `Auto` is match value `923`.
With `list_match_fix` on (default), off-list values are preserved: the row
shows the live cvar string, left-click goes to the next-highest match entry,
right-click to the next-lowest (numeric leading-double order, else
case-insensitive lex; wrap at the ends) — so `142.453` shows as-is, left
goes to `143` (whose 6.993ms budget fits inside 7ms), right to `125`.
Without the fix, off-list values resolved to fallback index 0, the widget
wrote `"923"`, and `cl_maxfps_change` rounded the pre-reset value (still in
`previous_cl_maxfps`) up to the smallest listed whole-ms cap that covers it,
so `923` could never be held; opening `sof_buddy/cpu` also pre-quantized
eagerly via `sofbuddy_quantize_cl_maxfps()` before parse so first paint
showed the settled value instead of the fallback label (still the path when
the fix is runtime-disabled). Every entry is at or above the `10` floor.
Deliberately no `<slider>` (decimals), no `<input>` (writes every keystroke), no menu
path to `0`. `cl_maxfps` is registered by `media_timers`, so picking a cap also retargets
the sleep/busy-wait budget.
RMF reference used here is the community RMF bible: `list` + `match` (parallel value list) + `cvar`
("associates a cvar with this area") is the correct form for a multi-valued cvar — with two hard
constraints, both verified against the engine's `list_c` methods *and* against live `sbtest_*`
experiments in `User/menus/`:
- `cvari` must only be combined with `match` when values stay inside `0 … labelCount-1`. The
  resolver returns the raw integer for `cvari` rows, but the normalize step still indexes
  `match[]` with it — the v8.9 `cvari cl_maxfps` row read `match[30]` of 11 entries and crashed
  the CPU tab. (`cvari` *without* `match` is safe: it falls into a clamped path. `cvar` +
  `match` is safe: bounded `strcmp` scan, fallback index 0 without the fix, `-1`
  (preserved, shown, no `Cvar_Set`) with `list_match_fix`. Both proven by the sbtest pages.)
- Without `list_match_fix`, a `<list>` must only wrap a cvar whose value domain is
  confined to its `match` set: the normalize step runs `Cvar_Set(name, match[index])`
  on layout, so opening the page can reset off-list values to `match[0]`
  (`cl_maxfps` was pre-quantized to a listed whole-ms cap before the page parsed
  for exactly this reason). With the fix, off-list `cvar`/`cvari` + `match`
  values are preserved (`GetMatchedValue` → `-1`, `SetValue` skips,
  `Draw` shows the live string, `Handle` goes to next-highest/lowest).
  (Entry count itself is irrelevant: the scan is count-bounded, and 16-entry `cl_showfps` /
  32-entry Crosshair ship.) See `src/features/list_match_fix/README.md`.

Menus under `menu_library/<name>/` are embedded and served directly from memory via the filesystem hooks.

| Menu | Purpose |
|------|---------|
| **loading** | Shown via SCR_BeginLoadingPlaque (engine loading plaque) and `loading_show_ui()` (e.g. http_maps). Pages `loading`, `loading_header`, `loading_files` kept slim: classic loading flow/progress, optional HTTP zip progress, and disconnect action. |
| **sof_buddy** | Main SoF Buddy menu set with tabbed top navigation and per-page content files named by tab (e.g. `main`, `cpu`, `network`, `input`, `updates`, `social`, `sofbuddy`, `cvars` + `*_content`). **Cvars** tab (`cvars_content.rmf`) is a scrollable raw editor for every user-facing `_sofbuddy_*` cvar (value via `<ctext>`/`<input>`, default + description below each row); internal layout/status cvars are hidden. **Buddy** tab (`sofbuddy_content.rmf`) for loading-screen options (`_sofbuddy_loading_lock_input`, `_sofbuddy_loading_show_mapname`, `_sofbuddy_loading_show_download`), menu theme/hotkey/tooltips, and startup update-check toggle (`_sofbuddy_update_check_startup`). **Network** tab for HTTP provider mode selection and direct URL editing via RMF `<input>` fields for `_sofbuddy_http_maps_dl_*` / `_sofbuddy_http_maps_crc_*` plus updater feed URLs (`_sofbuddy_update_api_url`, `_sofbuddy_update_releases_url`); provider inputs can be collapsed via `_sofbuddy_http_show_providers`; includes startup update requester (`update_prompt`) when a newer release is found; plus `margin_backdrop.rmf` for margin/background composition. |

## Editing menus

1. Edit files under `menu_library/<menu_name>/`.
2. Build (`make debug` or `make BUILD=release`).
3. `menu_data.cpp` is regenerated automatically by `tools/generate_menu_embed.py`.

## Notes

- Frame-loaded page files should include `<stm> ... </stm>`.
- `</vbar>` is not a valid closing token in this RMF dialect.
- `<list>` value mapping: `cvari` uses the cvar's integer value **directly as the label index**,
  so it is only safe when values stay inside `0 … labelCount-1` (all shipped `cvari` rows are
  `Off`/`On` pairs, `0`/`1`, or `0`…`3`). Anything sparse (`cl_maxfps` values `0,30,60,…,300`;
  `_sp_cl_vid_fov` values `0,1,90,…,111`) must use `cvar` + `match` instead, which looks the
  cvar string up in the match list. Using `cvari` for a sparse cvar reads out of bounds and
  crashes the page on open (this exact bug shipped once on the CPU tab's Framerate row).
