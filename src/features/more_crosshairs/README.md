# more_crosshairs

Community crosshair packs under `sof_buddy/crosshairs/` while keeping the engine **`crosshair`** cvar on stock values **0–4**.

## How it works

- **F12 → Gameplay:** `<list>` for **`crosshair`** (`0`–`4`) plus a **Crosshair theme** list bound to **`_sofbuddy_crosshair_theme`**. Theme names are the pack stems (`fl44`, `jay_crosshairs_v4`, …); `stock` uses vanilla art.
- A live **preview** (`status/chN` for stock theme, `status/sbxch_{theme}_{slot}` otherwise) reloads when theme or slot changes (unique paths bust the GL image cache).
- **`FS_LoadFile` override** (`internal_menus/hooks/fs_loadfile_override.cpp`): for non-stock themes, `pics/menus/status/ch{N}.m32` (`N` = `crosshair`) is read from the selected pack. No exe patch and no `active_crosshair.pak` staging.
- **Migration:** saved `_sofbuddy_crosshair_pick` or old `crosshair` values `5+` map through the generated legacy table into theme + slot.

## Catalog

Source packs: `rsrc/assets/crosshairs/`. `tools/generate_crosshair_catalog.py` builds `mhp.pak` / `sauron_crosshairs.pak`, then regenerates:

- `build/<config>/crosshair_catalog.inc` (`kCrosshairThemes`, legacy pick map)
- `menu_library/sof_buddy/more_crosshair_list.rmf`

## Credits

Bundled packs are classic community crosshair art (FL44, Jay, Keifer, MHP, Reps, Sauron, Yump Skulls, etc.). See the root [README](../../../README.md) **Credits** section for filenames; original authors retain credit for their work.
