# Changelog

## v2.5

- **Internal menus:** SoF Buddy in-game tabs (main, input, scaling, perf, lighting, texture, social, tweaks, update help); reworked RMF layouts for loading and tabs; side-frame backfill centering fix.
- **HTTP maps:** Optional map download assist; console progress bar (replaced http_downloading menu); docs in `src/features/http_maps/README.md`.
- **Config & update:** `sofbuddy.cfg` for feature toggles; update-from-zip flow and scripts (`rsrc/lin_scripts`, `rsrc/win_scripts`).
- **Performance:** Reduced hook and hot-path overhead; less stutter.
- **Raw mouse:** Engine cursor centering retained to avoid edge-of-screen issues.
- **Other:** cbuf overflow bypass (cbuf_limit_increase); lighting blend null checks; menu library docs; Cursor skills (new-release, git-commit, rmf-menu-authoring).

## v2.4

- Release checkpoint (see v2.5 for cumulative changes since v2.3).
