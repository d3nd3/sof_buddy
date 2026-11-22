# sof2bud changelog

User-facing highlights for **Soldier of Fortune II: Double Helix — MP 1.03 Gold** (`SoF2MP.exe` + `sof2bud.dll`).  
Published tags: **`sof2-vX.Y-buildN`** (separate from SoF Buddy `v…` tags). Discord **#sof2-releases** should mirror the `## vX.Y` section for that tag.

## Unreleased

### Launch & compatibility

- **Startup crash fixes:** baseline GfxInfo patch runs before `R_Init` (init from `DllMain`, no delayed thread sleep); no feature work from `WSAStartup`; load system `WSOCK32.dll` by full path; strict MP 1.03 `SizeOfImage` check before memory patches.

### Baseline (engine)

- **GL_EXTENSIONS:** skip the overflowing `GfxInfo_f` printf on modern GPUs (1fx MP gold layout); `gfxinfo` still works.
- **`r_mode` bounds:** unlock **-1 … 11** (stock modes + custom index range).
- **Master server:** redirect to `master.1fxmod.org`.
- **`rate 20000`:** forced at startup (same requirement as SoF Buddy on SoF1).

### Graphics

- **Extended `modelist`:** desktop and common resolutions via `EnumDisplaySettingsA`; raised `r_mode` max for high indices.
- **`r_mode -1`:** uses **current desktop** resolution (hook on `R_GetModeInfo`), not stale `r_customwidth` / `r_customheight`.
- **F12 → Graphics tab:** `r_mode`, `r_fullscreen`, **Vid Restart**, refresh mode list.

### Menus (F12)

- Native **`menus/sof_buddy/`** RMF pack (Version, Graphics, General).
- **F12** → `sof2buddy_f12_menu` (main-thread bind via `Cbuf_Execute` hook; re-bind with `sof2buddy_apply_f12`).
- Console: `sof2buddy_menu sof_buddy/<page>`, `menu sof_buddy <page>`.

## v0.1

First public **sof2bud** milestone — injection shell and baseline feature registry.

### Install

- MP **1.03 Gold** only: `WSOCK32` import hijack → `sof2bud.dll` next to `SoF2MP.exe`.
- Deploy: `sof2/deploy_to_sof2.sh` or `sof2bud_setup_v0.1.exe` (patches EXE import on finish).

### Baseline

- GfxInfo **GL_EXTENSIONS** skip, **`r_mode`** bounds, **master** string (runtime patches; no permanent EXE edit).

### Plumbing

- Engine API: `Com_Printf`, `Cvar_Get`, `Cvar_Set2`, `Cbuf_*`, `Cmd_*`.
- Feature registry: `baseline`, `core`, `video_modes`, `buddy_menu`.
