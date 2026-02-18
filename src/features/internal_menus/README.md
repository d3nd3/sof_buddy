# internal_menus

`internal_menus` embeds RMF files in the DLL and routes menu opens through `M_PushMenu`.

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
`sofbuddy_menu <name>`:
1. Syncs layout cvars via `update_layout_cvars(false)`.
2. Validates/sanitizes name (`/` normalization, rejects `..`).
3. Resolves entry as `<name>.rmf` or `<name>/sb_main.rmf` fallback.
4. Converts push target to `<menu>/<page_stem>` (for example `sof_buddy/sb_main`, `loading/loading`).
5. For non-`sof_buddy/*` targets, runs `killmenu` first to replace the current page.
6. `sof_buddy/*` targets rely on `<stm nopush>` in wrapper pages, so tab changes do not grow the stack.
7. For `loading/*`, uses `_sofbuddy_loading_lock_input` (`1` default) to choose lock behavior.

**CRITICAL: All internal navigation within the Buddy menus must use `sofbuddy_menu` instead of the engine's `menu` command.** This ensures that:
- Layout cvars (for scaling and centering) are re-synced before the menu opens.
- The correct internal path is resolved.
- The `killmenu` logic is handled correctly for tabbed navigation.

Direct sub-page form is supported:

```text
sofbuddy_menu <menu>/<page>
```

### 4) Serving embedded menus (`FS_LoadFile` override)
When the engine requests a menu file (e.g. `menus/sof_buddy/sb_main.rmf`), `internal_menus_fs_loadfile_override_callback` intercepts `FS_LoadFile`. If the path matches an embedded menu (normalized to `menu_name/filename` and looked up in `g_menu_internal_files`), the hook returns the in-memory content via engine `Z_Malloc` and does not call the original. Otherwise it falls through to the original `FS_LoadFile`.

### 5) Engine-driven loading push path (`M_PushMenu` hooks)
Pre-hook (`internal_menus_M_PushMenu_pre`):
- Normalizes menu name (case/path/optional `.rmf`).
- Detects loading root signals (`loading`, `m_loading*`, `f_loading*`) and rewrites to `loading/loading`.
- Applies `_sofbuddy_loading_lock_input` to `loading/loading` and `loading/*` paths.

Post-hook (`internal_menus_M_PushMenu_post`):
- Watches root `start` pushes.
- If updater queued a startup update prompt, opens `sof_buddy/sb_update_prompt` on top once.

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
- `M_PushMenu` Pre: detect loading signals, reroute to `loading/loading`, apply configurable lock for loading paths.
- `M_PushMenu` Post: on `start` push, consume queued startup-update prompt request and open prompt once.
- `Reconnect_f` Pre: push `loading/loading` when `reconnect` is issued, with configurable lock.

From `callbacks/callbacks.json`:
- `EarlyStartup` (Post): load embedded library.
- `PostCvarInit` (Post): register runtime cvars; register `sofbuddy_menu`, `sofbuddy_apply_menu_hotkey`, and Perf profile apply commands; bind `_sofbuddy_menu_hotkey` to `sofbuddy_menu sof_buddy`.
- `Cmd_ExecuteString` (override): when a `bind KEY "sofbuddy_menu sof_buddy"` is executed (e.g. from RMF setkey), sync KEY to `_sofbuddy_menu_hotkey` so it persists.

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
| **loading** | Engine loading override pages (`loading`, `loading_header`, `loading_files`) kept slim: classic loading flow/progress, optional HTTP zip progress, and disconnect action. |
| **sof_buddy** | Main SoF Buddy menu set with tabbed top navigation and per-page content files (`sb_*` + `sb_*_content`). Includes HTTP tab (`sb_http`) for loading-lock toggle, startup update-check toggle (`_sofbuddy_update_check_startup`), HTTP provider mode selection, and direct URL editing via RMF `<input>` fields for `_sofbuddy_http_maps_dl_*` / `_sofbuddy_http_maps_crc_*` plus updater feed URLs (`_sofbuddy_update_api_url`, `_sofbuddy_update_releases_url`); provider inputs can be collapsed via `_sofbuddy_http_show_providers`; includes startup update requester (`sb_update_prompt`) when a newer release is found; plus `sb_margin_backdrop.rmf` for margin/background composition. |

## Editing menus

1. Edit files under `menu_library/<menu_name>/`.
2. Build (`make debug` or `make BUILD=release`).
3. `menu_data.cpp` is regenerated automatically by `tools/generate_menu_embed.py`.

## Notes

- Frame-loaded page files should include `<stm> ... </stm>`.
- `</vbar>` is not a valid closing token in this RMF dialect.
