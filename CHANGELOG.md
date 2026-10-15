# Changelog

User-facing highlights. Older detail is summarized at the bottom.

## v8.38

### Graphics and HUD

- Fix SoFPlus `cl_showfps` OSD scaling: digits no longer overlap when HUD scale is above 1.

### Updater

- **`_sofbuddy_update_use_gui`** on **F12 → Version** (and Cvars): choose graphical `sof_buddy_update.exe` vs script/console install on all builds. Defaults **On** for native Windows universal, **Off** under Wine/Proton and on non-universal channels.

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
