# Changelog

## v2.9

- Updater: default `_sofbuddy_update_check_startup` is now enabled (`1`) so new installs check for updates on startup automatically.

## v2.8

- Build system: removed `http_maps` from `tools/generate_features_txt.py` default-disabled set, so `make`/CI auto-generation no longer silently disables it in `features/FEATURES.txt`.

## v2.7

- HTTP maps: removed experimental/default-disabled note from docs; feature is now treated as normal.
- Updater: startup update prompt now pre-intercepts intro/start menu flow; redundant post-hook injection removed.
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

