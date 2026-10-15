# more_crosshairs

Extends the in-game crosshair picker beyond the five stock `crosshair` values (0–4).

## Behaviour

- **F12 → Gameplay → Crosshair** lists stock entries first, then community packs shipped under `sof_buddy/crosshairs/`.
- **`_sofbuddy_crosshair_pick`** (archived) selects a catalog row; the feature sets the engine **`crosshair`** cvar to the correct slot (0–4) and stages **`sof_buddy/active_crosshair.pak`** when a pack override is needed.
- Stock rows remove `active_crosshair.pak` so vanilla `pics/menus/status/chN` textures are used.
- Works with **`_sofbuddy_crossh_scale`** (scaled UI) for size.

## Assets

Source packs live in `rsrc/assets/crosshairs/`. `tools/generate_crosshair_catalog.py` builds `mhp.pak` / `sauron_crosshairs.pak` from loose `.m32` files, scans packs for `pics/menus/status/ch*`, and regenerates:

- `build/<config>/crosshair_catalog.inc`
- `menu_library/sof_buddy/more_crosshair_list.rmf`

## Credits

Bundled crosshair packs are classic community assets (Jay, Keifer, Reps, Yump Skulls, FL44, MHP, Sauron, etc.). Authors retain credit for their original work.
