# Changelog

## v3.8

- Linux/Wine package completeness: `release_linux_wine.zip` now includes `update_from_zip.cmd` in CI packaging, matching the current auto-install launch path.
- Windows updater script hardening: normalized relative zip paths to absolute/backslash form for extraction/deletion reliability under Wine cmd.
- Extraction command robustness: `update_from_zip.cmd` now calls `tar.exe`/`7z.exe` explicitly and hardens VBScript fallback temp-file creation + `cscript.exe` probing.

## v3.7

- Misc tab UX: updated labels to show actual cvar names (matching Profiles tab style) for faster config auditing and copy/paste workflows.

## v3.6

- Wine/Proton updater downloads: runtime Wine detection now biases asset selection toward `release_linux_wine.zip` during `sofbuddy_update download` (non-XP builds).
- Auto-install launch hardening: reverted Wine `/unix` launch path and standardized `winstart` install command to `update_from_zip.cmd` to avoid repeated `Invalid Name` dialogs from `start` wrapper parsing.
- Installer scripts: `update_from_zip.cmd` now prefers non-PowerShell extractors first (`tar`, `7z`, VBScript) and uses PowerShell only as fallback.
- Post-install cleanup: both Windows and Linux/Wine update scripts now delete the extracted zip on successful install to prevent `sof_buddy/update/` accumulation.

## v3.5

- Updater install: `sofbuddy_update_install` now detects Wine (`wine_get_version`) and prefers `sof_buddy/update_from_zip.sh` under Wine/Proton.
- Shutdown launch path: Wine install commands are queued in `winstart` as `/unix "<script>" "<zip>"`, with fallback to `.cmd` when `.sh` is unavailable.
- Native Windows behavior remains unchanged (`update_from_zip.cmd`) while still using direct `winstart` buffer queueing.

## v3.4

- Updater install: `sofbuddy_update_install` now writes the `winstart` command buffer directly (no `start` command seeding), improving reliability for auto-install on shutdown.
- Internal menus (480p safety): removed risky quoted flow-text lines and standardized long prose blocks to use left layout with inset spacing instead of centered long text.
- Update UX: update prompt/help copy/layout refreshed for auto-install-first flow (compact prompt sizing, solid background, clearer action text, trimmed obsolete manual-only text).
- SoF Buddy tabs: Network now shows only a white `Current map` bound to `mapname`; Social removed link-status/last-URL rows; Profiles button label simplified to `Apply Profile`.
- Menu docs: `menu-know-how` guidance now explicitly recommends avoiding long prose under `<center>` and using left+inset for 480p.

## v3.3

- Updater: added `sofbuddy_update_install` to queue install command through the engine `winstart` pointer (`0x40353C`) and quit for fully automatic post-exit update flow.
- Updater install targeting: `sofbuddy_update_install` now passes the exact `_sofbuddy_update_download_path` zip into `update_from_zip.cmd`, avoiding wrong-zip selection when multiple zips exist.
- Internal menus: update prompt/help actions now support one-click download+install+exit flow.
- Build hygiene: `src/features/internal_menus/menu_data.cpp` is now ignored and untracked as generated output to avoid recurring dirty status.

## v3.2

- http_maps: when a map is downloaded/extracted, loading status stays at "HTTP Downloading..." (MAP PRESENT is only used when no HTTP download was needed).
- Core: `sofbuddy_list_features` now logs via PRINT_DEV instead of the normal console, keeping the feature listing in the dev log channel.

## v3.1

- Internal menus: startup update prompt now triggers on menu disclaimer (root `disclaimer`) instead of `start`; README and docs use "menu disclaimer" naming.

## v3.0

- Internal menus (loading): engine loading UI is disabled via NOPs at exe 0x13AC7 (menu push) and 0x13ACE (SCR_UpdateScreen call); we push `loading/loading` and call SCR_UpdateScreen(true) in SCR_BeginLoadingPlaque Post hook. SCR_UpdateScreen at exe 0x15FA0. Additional 2-byte NOP at 0x13AAA so the plaque shows when the console is open.
- Internal menus: Reconnect_f hooks removed; loading trigger is SCR_BeginLoadingPlaque only. M_PushMenu Pre simplified (start/update-prompt only). FS_LoadFile override and READMEs updated.

## v2.9

- Updater: default `_sofbuddy_update_check_startup` is now enabled (`1`) so new installs check for updates on startup automatically.

## v2.8

- Build system: removed `http_maps` from `tools/generate_features_txt.py` default-disabled set, so `make`/CI auto-generation no longer silently disables it in `features/FEATURES.txt`.

## v2.7

- HTTP maps: removed experimental/default-disabled note from docs; feature is now treated as normal.
- Updater: startup update prompt now pre-intercepts menu disclaimer (start menu) flow; redundant post-hook injection removed.
- Updater downloads: zip asset selection tightened for build target (Windows vs Windows XP, Linux/Wine fallback penalties), with stricter fallback filtering.
- Updater UX: selected/downloaded release asset filename is now printed via `PRINT_DEV` and surfaced in update prompt/help menus.

## v2.6

- HTTP maps: reconnect-stage hook path added, frame/config handling tightened, and provider/update wiring refined.
- Internal menus: SoF Buddy RMF tab/content layout pass, loading screen variants added, and FS load override integrated.
- Update flow: update command and zip-update scripts hardened for Linux/Windows and compatibility glue cleaned up.
- Build/docs: detour registrations regenerated and feature READMEs/root docs refreshed.

## v2.5

- Internal menus: loading screen RMF consolidated (loading_recent/loading_status removed; loading, loading_files, loading_header reworked); SoF Buddy tabs (main, input, HTTP content) layout tweaks; internal_menus/menu_data/README cleanup.
- HTTP maps: code simplification and trim.

## v2.4

- Internal menus: SoF Buddy in-game tabs (main, input, lighting, perf, scaling, social, texture, tweaks), loading screens, RMF layout rework.
- HTTP maps: download/install maps from URLs; console progress; provider config.
- sofbuddy config file and update-from-zip flow (Linux/Windows scripts).
- Perf: reduced hook hot-path overhead and stutter.
- raw_mouse: keep engine cursor centering to avoid edge-of-screen issues.
- cbuf overflow bypass; internal_menus/http_maps/cbuf_limit_increase docs.
- SoF Buddy side-frame backfill centering fix; lighting blend null checks.

