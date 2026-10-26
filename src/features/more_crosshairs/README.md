# more_crosshairs

Community crosshair packs under `sof_buddy/crosshairs/` while keeping the engine **`crosshair`** cvar on stock values **0–4**.

## How it works

- **F12 → Gameplay → Crosshair:** theme list (`_sofbuddy_crosshair_pick`): **S0–S4** (stock), **sof_buddy** bundled packs (`sof_buddy/crosshairs/*.pak`), plus any extra crosshair `.pak` in `base/` / gamedir (not `pak0`–`pak9`; skips stems already in the buddy catalog, e.g. `jay_crosshairs_v4.pak` in `base` when that pack is shipped under `sof_buddy/crosshairs/`). Each discovered pak gets menu rows like `pcr1` / `pcr2` for `pcross.pak` (ch1–ch2 only is fine). Sets **`_sofbuddy_crosshair_theme`** and engine **`crosshair`** (`0`–`4`). **`_sofbuddy_crossh_scale`** lives on the same section.
- A live **preview** (`status/chN` for stock theme, `status/sbxch_{theme}_{slot}` otherwise) reloads when theme or slot changes (unique paths bust the GL image cache).
- **`_sofbuddy_crosshair_tint`** (F12 → Gameplay): primary colors for **stock S0–S4** only (applied at `glVertex2f` during `SCR_DrawCrosshair`, after ref_gl sets white).
- **`FS_LoadFile` override** (`internal_menus/hooks/fs_loadfile_override.cpp`): theme packs load only via `pics/menus/status/sbxch_{theme}_{slot}`. **Standard (S0–S4, theme 0)** loads `ch0`..`ch4` from `gamedir` / `base` paks and loose files only — **`sof_buddy/` is skipped** so mounted community crosshair paks do not override stock.
- **Engine path:** `SCR_DrawCrosshair` only runs `Com_sprintf("pics/menus/status/ch%i")` when the slot is **non-zero**, so slot **0** pack picks (e.g. Yump 0) must set `crosshair_pic_ptr` (`SoF.exe` `0x225940`) to `sbxch_{theme}_{slot}` explicitly. `Draw_GetPicSize` / `GL_FindImage` pre-hooks refresh that buffer so ref_gl does not keep serving cached vanilla `ch0`..`ch4` art (same reason `vid_restart` used to “fix” it).
- **Migration:** saved `_sofbuddy_crosshair_pick` or old `crosshair` values `5+` map through the generated legacy table into theme + slot.

## Catalog

Source packs: `rsrc/assets/crosshairs/*.pak`. `tools/generate_crosshair_catalog.py` normalizes `mhp.pak` / `sauron_crosshairs.pak` TOC entries, then regenerates:

- `build/<config>/crosshair_catalog.inc` (`kCrosshairThemes`, legacy pick map)
- `menu_library/sof_buddy/more_crosshair_list.rmf`

## Credits

Bundled packs are classic community crosshair art (FL44, Jay, Keifer, MHP, Reps, Sauron, Yump Skulls, etc.). See the root [README](../../../README.md) **Credits** section for filenames; original authors retain credit for their work.
