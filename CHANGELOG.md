# Changelog

User-facing highlights. Older detail is summarized at the bottom.

## v8.45

### Windows installer — optional SoF Plus client

- **Detect retail layout** from the GOG violence / parental-control cvar cluster (`0x201373BF`–`0x20137453`); **CD** when that cluster is absent (file size is not used).
- **Detect KB4571756-style exe fix** when UTF-16 `Raven Software` is cleared at `0x2015F1C0`; maps to `gog` / `gog-kb4571756` or `cd` / `cd-kb4571756` patch kits.
- Bundled **20140531 SoF Plus client patch kits**, `install_sofplus_client.ps1`, and installer task; skips Raven compat script when the exe already has the KB fix.
- Docs: `docs/WINDOWS_INSTALLER.md`, `tools/test_sof_install_detect.py`.

### Updater / stable channel

- **Stable release pointer** (`releases/stable.json`, `raw.githubusercontent.com` feed) and in-game **Stable vs Latest** install target.
- **Release list** filtering, downgrade flow RMF, GUI updater improvements, and expanded `tools/update_test` (robustness + `verify_updater.py`).
- SofVault mirror script notes for stable JSON sync.

### Other

- F12 update prompt RMF refactor (`update_prompt_content.rmf`, downgrade menus).
- Minor crosshair `FS_LoadFile` / UI-scale menu tweaks.

## v8.44

### Crosshairs (F12 → Gameplay)

- **Unified crosshair list** generated from real pack contents (`_sofbuddy_crosshair_pick` sets theme + stock slot **0–4**); legacy `crosshair` values **5+** still migrate on first load.
- **Live menu preview** via `_sb_internal_crosshair_preview_rmf` (`status/chN` or `status/sbxch_{theme}_{slot}`); misc menu reloads when the pick changes so the image updates.
- **Preview rendering:** menu paths use ref_gl **PIC** imagetype (3), not sprite; fixes white squares for pack art while stock **S0–S4** still work.
- **`FS_LoadFile` routing:** serves buddy-pack `m32` for preview and in-game `chN` requests; normalizes `pics/menus/` prefixes; repacks **mhp** / **sauron** paks with correct PACK TOC offsets (fixes `Invalid version` spam).
- **`GL_FindImage` pre-hook** for crosshair menu image paths (sprite retry fallback retained).
- **Stability:** removed nested menu keys from the crosshair list (F12 scroll crash); short list labels stay under SoF’s 240-byte parser limit.

### Release artifacts

- GitHub release zips include **`sof_buddy/crosshairs/*.pak`**; catalog build no longer depends on loose `.m32` sources in the repo (pack-only assets).

### Updater / F12 menu

- Update prompt split into header / notes / footer RMF includes; **`reloadall`** after release-list refresh so open update menus show new notes.
- Main menu content tweaks for update flow.

## v8.43

### Singleplayer framerate (F12 → CPU)

- **Exact FPS cap** (`_sofbuddy_cl_maxfps_exact`, default `0`): with **Singleplayer Max FPS** on, set `1` so `cl_maxfps` is a **literal integer FPS** (10–360, e.g. `65` targets ~65 FPS). With exact off, the legacy **millisecond-budget** list still applies (e.g. list value `63` caps near **62.5** FPS).
- **Bresenham pacing** in the media-timers main loop and a matching **`CL_Frame` gate** (call patch into the DLL, integer-only, applied only while exact is on) so the engine does not early-out on the old float `1000/cl_maxfps` compare.
- **SoFPlus** `_sp_cl_frame_delay` is cleared in exact mode so SoFPlus does not stack a second `ceil(1000/fps)` limiter.
- **Stability:** removed unsafe mid-function detours; restores broken inline `CL_Frame` patches from older test builds; defers the SP listen-server `CL_Frame` NOP until `cl_maxfps` exists.
- **F12 → CPU:** separate RMF widgets for legacy framerate, exact integer cap, and Buddy presets.

### Crosshairs (F12 → Gameplay)

- **Theme picker** uses **`_sofbuddy_crosshair_theme`** plus stock `crosshair` **0–4**; packs load via **`FS_LoadFile`** routing (no `active_crosshair.pak` staging). Catalog/regenerated packs and menu list updated (`tools/generate_crosshair_catalog.py`).

### Other

- **Internal menus:** CPU tab sync for `_sb_internal_feature_cl_maxfps_singleplayer`; assorted tab/RGF layout fixes.
- **VSync display** and **scaled UI** hooks: minor fixes aligned with crosshair asset paths.
- **Docs/README:** exact vs legacy `cl_maxfps` semantics and new cvar table entry.

## v8.42

### Bug fixes

- **Gameplay tab crash/freeze:** the crosshair `<list>` label string exceeded SoF’s 240-byte menu parser buffer; labels are abbreviated in the picker (tooltip still documents packs).
- **Crosshair picker now drives the engine `crosshair` cvar directly:** the shadow `_sofbuddy_crosshair_pick` cvar, `sof_buddy/active_crosshair.pak` staging, and the startup pick/config reconciliation are gone. The list writes values 0–32 onto `crosshair` itself (archived, shared with community configs). SCR_DrawCrosshair's stock 4.0f clamp is bypassed (branch patch at SoF.exe RVA `0x15DE8`), and the new extended values are served by the FS_LoadFile route: an engine request for `pics/menus/status/chN` (5–32) returns that entry's `ch{src_slot}.m32` sprite from its pack under `sof_buddy/crosshairs/`, which also fixes the restart-reset reports; stock rows 0–4 still use vanilla art.
- **`cl_showfps` / `cl_maxfps` not persisting with SoFPlus:** F12/console changes save `spm_settings.cfg` on change and on exit (after SoFPlus’s startup restore), with chained callbacks so SoFPlus’s own handlers still run; programmatic master-server bootstrap no longer overwrites `spm_settings.cfg`.

### F12 menu

- Tab bar: `align center` stacked every tab onto its own centered line (regression of the v8.42 changelog bullet: the engine treats widget `align` as line alignment, not label alignment inside the box). Tabs now flow side by side in a flush 4x3 wrapped grid with uniform 144px boxes (fitting the measured widest label, Profile GUID 121px + 8px border pads), so no label overflows and rows wrap cleanly.

### Updater (developer)

- Local update-test kit: `make update-test` builds old/new universal DLLs, fixture JSON/zips, and `bin/sof_buddy-update-test-kit.zip`; `make update-test-serve` runs the fake feed on `127.0.0.1:8765`. See **DEBUGGING_UPDATER.md** and `tools/update_test/README.md`.
- Update-test DLLs block production GitHub/SofVault URLs unless `_sofbuddy_update_allow_production_urls` is set; supports generic `latest.json` release feeds.

## v8.41

First stable release in this line; **v8.39** and **v8.40** GitHub builds were withdrawn (broken).

### Bug fixes

- **Load/save slots empty** ([#24](https://github.com/d3nd3/sof_buddy/issues/24)): mount `sof_buddy/*.pak` during filesystem init **after** `basedir/base` and **before** the `user` directory, so the `user` search path stays on top and saves remain visible.
- **`sofbuddy.cfg`**: startup load matches save location (`basedir/base/sofbuddy.cfg`); MP profile **`sf_login.cfg`** read/write uses the engine **`user`** cvar path (not hardcoded `basedir/user`).
- **`FS_AddGameDirectory` hook:** automatic detour length so the engine prologue is not split (startup crash).

### Bundled assets

- Release zips include **`sof_buddy/sndrstfindimage.pak`** (placeholder textures so `GL_FindImage` stops spamming the console after **`snd_restart`**).
- **`sof_buddy/extended_skins.pak`** — extended multiplayer player models (skins by **Acadie**). Buddy mounts all `sof_buddy/*.pak` after `basedir/base` (below `user` / `-game` on the search path).

### F12 menu

- **Gameplay:** Audio section (`s_musicenabled`, `s_volume`, `s_khz`, `s_loadas8bit`, **Restart Audio** → `snd_restart`); kill-feed section (`con_notifytime`, `cl_obit_filter`).
- **Gameplay → Crosshair:** stock crosshairs plus community packs under **`sof_buddy/crosshairs/`** (catalog picker, **`active_crosshair.pak`** staging).
- **Lighting / Graphics:** clearer labels (lightmap multiplier, Ghoul lighting block, dynamic light threshold, specular textures); warp chunk size control moved to **Graphics**.
- Tab bar: centered tab labels; **MP Profile GUID** tab width adjusted.
- Loading tips default **`tip_duration`** to **10000** ms (migrates legacy 2500/5000).

### Updater

- Startup update prompt **What’s new** text fixed (chunked RMF cvars for long release notes).
- **`sofbuddy_update prompt`** — check for updates and open the update or up-to-date menu.
- Release notes shown in the prompt strip markdown/link noise for RMF text fields.
- Universal builds default **`_sofbuddy_update_gui_updater`** to on under Wine/Proton as well.
- **Wine `update_from_zip.cmd`:** “fresh settings” prompt and cleanup of `install_context.env` work under Wine `cmd` (no `set /p` colon parse bug).
- GitHub release bodies are generated from **CHANGELOG.md** (`tools/changelog_release_body.py`).

### Docs

- README features list extended skins (Acadie); cvar tables scroll horizontally on narrow viewports.

## v8.38

### Graphics and HUD

- Fix SoFPlus `cl_showfps` OSD scaling: digits no longer overlap when HUD scale is above 1.

### Updater

- **`_sofbuddy_update_use_gui`** on **F12 → Version** (and Cvars): choose graphical `sof_buddy_update.exe` vs script/console install on all builds. Defaults **On** on universal builds (all flavours); **Off** on non-universal channels.

## v8.37

### F12 menu

- Renamed tabs (Version, Cpu, Graphics, Gameplay, WebLinks, General); removed the Updates tab.
- Features tab section headers layout; Profile GUID tab label fits the tab bar.
- Web Links: **Server List megalag** and **Server List 333Net**; installer docs wording updates.

### Graphics and HUD

- SoFPlus top-right FPS counter follows **Auto HUD Scale** / `_sofbuddy_hud_scale` when SoFPlus is loaded.
- Fresh installs (or deleted `config.cfg`) default `gl_mode` to **0** instead of **3** to avoid the community-edition refresh glitch.

### Project

- GitHub issue forms require `_sp_cl_info_sofbuddy` and OS version; repo layout cleanup (`config/`, `scripts/`, `docs/`).

## v8.36

### Build and updater

- Fix release builds when the environment sets a native `CC` (always use MinGW for the DLL).
- Fix compile errors in startup userinfo (`proton` / `wine` channel) and in-game zip extract (directory creation).
- First GitHub release build after repository cleanup (no prior published tags).

## v8.35

- Bundles recent updater, Proton/Wine zip install, F12 flavour/build display, and VERSION-gated CI releases.

## v8.34

### Updater (Wine / Proton / universal)

- In-game zip install extracts in-process on Wine/Proton before quit, with script fallbacks when needed.
- Install context file for cross-platform update scripts; Windows installer can replace a locked DLL via its own helper script.
- F12 **Flavour** shows **Universal (Proton)** or **Universal (Wine)** when detected; Home shows **Build** number.

## v8.33

### Startup update prompt

- Scrollable prompt with installed version, available release, title, asset name, and release notes when an update is queued.

### Build

- CI builds universal DLL and Windows updater reliably; compiler cache key fixed for container workflows.

## v8.32

- F12 **Texture → Resolution**: `gl_mode` picker from SoFPlus video mode labels.
- Faster repeat CI builds (ccache).

## v8.31

- F12 **MP Profile Guid** tab (view, copy, persist GUID).
- Graphical Windows updater (`sof_buddy_update.exe`) on universal channel; console path unchanged elsewhere.
- Default GameSpy master `sof1master.sofvault.org`; installer handles locked DLL at install time.

## v8.30

- With SoFPlus: FPS overlay, framerate cap, and master server persist via SoFPlus settings save on exit.
- Vanilla: `cl_showfps` / `cl_maxfps` still use `config.cfg`.

## v8.29

- Installer verifies installed DLL hash and reports lock/read failures clearly.

## v8.28

- F12 **Network**: master server `<list>` on `_sp_cl_master_gamespy` (MegaLag, 333Networks, SofVault, Qtracker).

## v8.27

- `scripts/cptosof-universal` copies the universal build and bundled helper files to a game folder safely.

## v8.26 – v8.20

- Versioned Windows installer filenames; bonus menu key binding fixes; GameSpy master fallback and health checks.
- Font pack selector (Default / SoFPlus / Vanilla); CPU profile detection on F12; feature rows show **Unavailable** when not in the DLL.
- Translation font priority fix; centered DM scoreboard; installer feature grouping and optional task clarity.

## v8.19 – v8.11

- `list_match_fix` for off-list menu values; build number in userinfo; generated F12 Features tab from compile-time feature set.
- Universal vs standard release zips and updater channel matching; runtime feature toggles in F12 with `sofbuddy.cfg` persistence.

## v8.10 – v8.5

- F12 **CPU**: display refresh readout, refresh picker, VSync vs cap warning, improved `cl_maxfps` list (no crash on open).
- Team icon / player name HUD autoscale; texture filter reapply after `vid_restart`; read-only Cvars reference tab.
- `cl_maxfps` registered for media timer frame budget; Map Study cvars hidden when entity tools are not built in.
- Internal cvar prefix `_sb_internal_*` for bookkeeping (user cvars unchanged).
- Removed experimental focus workaround; safer high-resolution timers on Wine; releases only when `VERSION` bumps.

## v8.0 – v7.0

- Texture UI filter options clamped so menus cannot select invalid mip modes.
- Menu theme presets and Buddy tab; loading-screen options; deathmatch-only custom loading rules.
- Expanded F12 internal menus (tabs, persistence, layout fixes).

## v6.0 – v2.0

- Scaled UI, HUD, console, and menus; raw mouse; HTTP map download; in-game updater; Windows installer.
- HD textures, VSync, lighting blend, master server helpers, console overflow fixes, SoFPlus integration.
- Optional compile-time features (e.g. map entity tools) off by default.
