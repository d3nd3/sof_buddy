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
- **Validation and resolution** — Name is sanitized and resolved against the embedded menu set only (`g_menu_internal_files`); invalid or missing paths are rejected. Converts to `<menu>/<page_stem>` (e.g. `sof_buddy/sb_main`, `loading/loading`) with fallback to `sb_main.rmf` for menu roots.
- **Replace top page for non-sofbuddy** — For targets other than `sof_buddy/*`, runs `killmenu` first so the new menu replaces the current one and ESC behavior is predictable.
- **Loading lock** — For `loading/*`, uses `_sofbuddy_loading_lock_input` for lock behavior. The loading current map name (`_sofbuddy_loading_current`) is set to `"resolving..."` only in the loading reset state (http_maps `http_maps_clear_loading_cvars`), not here.

`sof_buddy/*` targets rely on `<stm nopush>` in wrapper pages, so tab changes do not grow the stack. Use `sofbuddy_menu` for Buddy tab/page navigation so layout is re-synced and paths stay correct.

Direct sub-page form is supported:

```text
sofbuddy_menu <menu>/<page>
```

### 4) Serving embedded menus (`FS_LoadFile` override)
**Internal menus have priority over the filesystem:** the override intercepts the path and filename of the requested `.rmf`, normalizes them, and looks up `menu_name`/`filename` in `g_menu_internal_files`. On a match, the hook serves the in-memory content (via engine `Z_Malloc`) and does not call the original; otherwise it falls through to the original `FS_LoadFile`.

### 5) Loading and menu-disclaimer (start) hooks
**SCR_BeginLoadingPlaque**: At EarlyStartup we NOP 5 bytes at exe 0x13AC7 (engine’s menu push) and 5 at 0x13ACE (SCR_UpdateScreen call), so the original does not show its own loading UI. **SCR_BeginLoadingPlaque** Post (`internal_menus_SCR_BeginLoadingPlaque_post`): when `!noPlaque`, we run killmenu, `oM_PushMenu("loading/loading", "", lock_input)`, then `SCR_UpdateScreen(true)` (exe 0x15FA0). Loading is only pushed by us (this hook and `loading_show_ui()` from http_maps). The `"resolving..."` label is set in the loading reset state (http_maps), not here.

**M_PushMenu** Pre (`internal_menus_M_PushMenu_pre`): Normalizes menu name; only handles menu-disclaimer/update-prompt: when the engine pushes the menu disclaimer (root `start`) and the updater queued a startup prompt, rewrites to `sof_buddy/sb_update_prompt`. No loading-signal detection or rewrite.

**M_PushMenu** Post (`internal_menus_M_PushMenu_post`): Watches menu disclaimer (root `start`) pushes; if updater queued a startup update prompt, opens `sof_buddy/sb_update_prompt` on top once.

### 6) Loading UI cvar updates
Loading UI is fed by direct helpers:
- `loading_set_current(...)`

### 7) Dynamic layout cvars (SoF Buddy tabs/centering)
Created in `PostCvarInit` and recomputed on vid changes:
- `_sofbuddy_menu_vid_w`, `_sofbuddy_menu_vid_h`
- `_sofbuddy_sb_center_panel_px`
- `_sofbuddy_sb_tabs_row1_prefix_px`, `_sofbuddy_sb_tabs_row2_prefix_px`
- `_sofbuddy_sb_tabs_row1_prefix_rmf`, `_sofbuddy_sb_tabs_row2_prefix_rmf`
- `_sofbuddy_loading_lock_input` (`CVAR_SOFBUDDY_ARCHIVE`, default `1`)
- `_sofbuddy_menu_hotkey` (`CVAR_SOFBUDDY_ARCHIVE`, default `F12`). The open key is shown and rebound via RMF `<setkey "sofbuddy_menu sof_buddy" ...>` (Input and main); bind mode updates the cvar.
- `_sofbuddy_perf_profile` (`CVAR_SOFBUDDY_ARCHIVE`, default `0`, used by Perf T profile list)
- Tunables: `_sofbuddy_sb_tabs_row1_content_px`, `_sofbuddy_sb_tabs_row2_content_px`, `_sofbuddy_sb_tabs_center_bias_px`, `_sofbuddy_sb_tabs_row1_bias_px`, `_sofbuddy_sb_tabs_row2_bias_px`

When video size changes, `update_layout_cvars(true)` recomputes runtime layout cvars used by tab-prefix `includecvar` blocks.

## Hooks/callbacks used

From `hooks/hooks.json`:
- `FS_LoadFile` (override): serve embedded RMF from `g_menu_internal_files` when path matches menu_library; otherwise fall through to original.
- `M_PushMenu` Pre: menu-disclaimer/update-prompt only (rewrite `start` to `sof_buddy/sb_update_prompt` when updater queued); no loading signals.
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
sofbuddy_menu sof_buddy/sb_perf
sofbuddy_menu sof_buddy/sb_http
sofbuddy_apply_menu_hotkey
```

## Menu library

Menus under `menu_library/<name>/` are embedded and served directly from memory via the filesystem hooks.

| Menu | Purpose |
|------|---------|
| **loading** | Shown via SCR_BeginLoadingPlaque (engine loading plaque) and `loading_show_ui()` (e.g. http_maps). Pages `loading`, `loading_header`, `loading_files` kept slim: classic loading flow/progress, optional HTTP zip progress, and disconnect action. |
| **sof_buddy** | Main SoF Buddy menu set with tabbed top navigation and per-page content files (`sb_*` + `sb_*_content`). Includes HTTP tab (`sb_http`) for loading-lock toggle, startup update-check toggle (`_sofbuddy_update_check_startup`), HTTP provider mode selection, and direct URL editing via RMF `<input>` fields for `_sofbuddy_http_maps_dl_*` / `_sofbuddy_http_maps_crc_*` plus updater feed URLs (`_sofbuddy_update_api_url`, `_sofbuddy_update_releases_url`); provider inputs can be collapsed via `_sofbuddy_http_show_providers`; includes startup update requester (`sb_update_prompt`) when a newer release is found; plus `sb_margin_backdrop.rmf` for margin/background composition. |

## Editing menus

1. Edit files under `menu_library/<menu_name>/`.
2. Build (`make debug` or `make BUILD=release`).
3. `menu_data.cpp` is regenerated automatically by `tools/generate_menu_embed.py`.

## Notes

- Frame-loaded page files should include `<stm> ... </stm>`.
- `</vbar>` is not a valid closing token in this RMF dialect.
