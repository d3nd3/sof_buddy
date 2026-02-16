# internal_menus

`internal_menus` serves RMF menu files from memory instead of disk.

The feature is intentionally small:
- `FS_LoadFile` is patched at known callsites and can return embedded RMF bytes.
- `sofbuddy_menu <name>` pushes an internal menu directly (no frame-queue system).
- Loading cvars are event-driven (no `Qcommon_Frame` hook in this feature).

## Design

### 1) Embedded menu library
`menu_data.cpp` is generated from:

```text
src/features/internal_menus/menu_library/<menu_name>/*.rmf
```

At runtime `internal_menus_load_library()` fills:

```cpp
g_menu_internal_files[menu_name][filename]
```

### 2) FS override
`hk_FS_LoadFile` does two things:
- Always serves `loading*.rmf` files from embedded `loading/` assets.
- Serves any other embedded menu file only when `sofbuddy_menu_internal=1`.

That keeps normal game menu loads untouched.

### 3) Direct menu push
`sofbuddy_menu <name>`:
1. Validates `name`.
2. Resolves entry page as `<name>.rmf` (or `<name>/sb_main.rmf` when no root file exists).
3. Temporarily sets `sofbuddy_menu_internal=1`.
4. Calls `M_PushMenu(resolved_name, "", false)`.
5. Restores `sofbuddy_menu_internal=0`.

It also accepts `sofbuddy_menu <menu>/<page>` for direct embedded sub-pages.

No pending queue, no delayed retries.

For engine-driven loading pushes, the `M_PushMenu` pre/post hooks temporarily
toggle `sofbuddy_menu_internal=1` only while `M_PushMenu(\"loading\", ...)`
is executing. That swaps the loading assets without keeping internal mode on.

### 4) Loading cvar updates
Loading UI values are updated by direct helper calls:
- `loading_set_current(...)`
- `loading_push_status(...)`
- `loading_set_files(...)`
- `loading_push_history(...)`

This keeps the feature off the per-frame hot path.

## Hooks used

From `hooks/hooks.json`:
- `M_PushMenu` Pre: turns on internal mode only for external `loading` pushes
- `M_PushMenu` Post: restores external mode after that loading push

From `callbacks/callbacks.json`:
- `EarlyStartup`: install `FS_LoadFile` callsite patches
- `PostCvarInit`: register cvars and `sofbuddy_menu`

## Usage

```text
sofbuddy_menu loading
sofbuddy_menu http_downloading
sofbuddy_menu sof_buddy
sofbuddy_menu sof_buddy/sb_perf
```

## Editing menus

1. Edit files under `menu_library/<menu_name>/`.
2. Build with `make debug` (or your target).
3. `menu_data.cpp` is regenerated automatically by `tools/generate_menu_embed.py`.

## Notes

- Frame-loaded page files should include `<stm> ... </stm>`.
- `</vbar>` is not a valid closing token in this RMF dialect.
