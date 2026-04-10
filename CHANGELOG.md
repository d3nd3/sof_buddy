# Changelog

## v5.7

- Removed vendored `ida_plugin_mcp` from the repository and switched Cursor MCP wiring to external checkout paths.
- Ignored Python bytecode artifacts and removed tracked `__pycache__` files.
- Updated internal menus:
  - Moved the former profiles/cpu-eaters content into the `CPU` tab.
  - Removed the standalone profile tab navigation.
  - Added profile preview block with `Will set...` target values and `Custom/Current` view mode.
  - Simplified profile apply flow to a single `Apply Profile` action.
  - Updated tab/page labels from `Profiles` to `CpuEaters` where applicable.
  - Removed `Map study` from menu navigation tabs.
- Lighting menu controls:
  - Enforced integer slider behavior for `_sofbuddy_lighting_cutoff`.
  - Enforced integer slider behavior for `_sofbuddy_water_size`.
- UI scaling/runtime updates:
  - Included recent tweaks in `scaled_con`, `scaled_hud`, and `scaled_ui_base` cvar handling.

# Changelog

## v5.6

- **Detours / codegen:** Expanded `detours.yaml` and `tools/generate_hooks.py` for pointer-backed hook targets; `rsrc/funcmaps/SoF.exe.json` and per-feature `hooks/pointers.json` (plus `src/core/pointers.json`); `docs/DETOUR_SYSTEM.md` and `hdr/detours.h` updates; `DetourXS` and core detour registration (`detours.cpp`, `shared_hook_manager.cpp`) aligned with the new flow.
- **Core:** Init and loader path work (`simple_init.cpp`, `module_loaders.cpp`, `wsock_entry.cpp`, `update_command.cpp`, `sofbuddy_cfg.cpp`) so startup and update behavior match the hook system changes.
- **entity_visualizer (new, optional):** Map-study tooling (spawn/draw, cvars, QCommon frame and loading hooks). Still **off by default** in `features/FEATURES.txt` alongside other experimental toggles; enable by uncommenting the feature line for local builds.
- **Internal menus:** Map debug RMF assets (`map_debug.rmf`, `map_debug_content.rmf`); tab content and loading/menu hook follow-ups; `hooks/pointers.json` for internal menu hooks.
- **Graphics / ref-DLL features:** `lighting_blend`, `scaled_ui_base`, `scaled_menu`, `texture_mapping_min_mag`, `vsync_toggle` — cvar/refdll and hook wiring updates where pointers apply; feature README tweaks where noted.
- **Other features:** `http_maps`, `media_timers`, `new_system_bug` — small hook/cvar/README adjustments; `raw_mouse` message-pump and hook touch-ups.
- **Cbuf / config:** `cbuf_limit_increase` README and early callback updates; `FEATURES.txt` comment clarifies deprecation in favor of `sofbuddy.cfg` persistence.
- **Tooling / docs:** IDA MCP plugin (`ida_plugin_mcp/`), `ida_dump_main.py`, and Cursor MCP/command helpers for IDA workflows; `README.md` and several feature READMEs refreshed.

## v5.5

- Build defaults: `entity_visualizer` remains under Game Features but is **commented out by default** in auto-generated `features/FEATURES.txt` (`tools/generate_features_txt.py`). Default-disabled features are `scaled_menu`, `cbuf_limit_increase`, and `entity_visualizer` (map entity / wireframe tools).
- HTTP maps: stabilize deferred precache and loading-status flow; harden HTTPS, size fallbacks, loading status, and logging (`http_maps.cpp`, README).
- Internal menus: restore early loading UI with a pak-aware fast path; harden loading menu behavior in unlock mode; loading safe RMF updates; `SCR_BeginLoadingPlaque` post-hook adjustments.

## v5.4

- Raw mouse: removed the `IN_MenuMouse` engine hook (SofExe `0x4A420`), Pre/Post callbacks, and `in_menumouse.cpp`. `GetCursorPos` no longer takes a special “real API” path while menu-mouse runs; with raw mouse enabled it always drains pending mickeys and reports the synthetic cursor position like other code paths.

## v5.3

- Raw mouse: if `RegisterRawInputDevices` returns `ERROR_INVALID_PARAMETER` (87) for focus-following registration (`hwndTarget=NULL`), retry once with an explicit same-process top-level HWND (resolved via `cl_hwnd`, window heuristics, or foreground when it belongs to the game process). Non-null `hwndTarget` must be owned by the calling process; comments in `raw_shared.cpp` document that Win32 rule.
- Raw mouse: `DispatchMessageA` calls `raw_mouse_ensure_registered` while `_sofbuddy_rawmouse` is on and registration is not complete yet, so enable still settles if the first attempt runs before the pump has ticked.
- Raw mouse: `RegisterRawInputDevices` may still be invoked from the cvar path or other threads (same as v5.2); raw input registration is per-process and must not be restricted to the window thread only.

## v5.2

- Raw mouse: stop binding normal foreground raw input registration to a specific HWND. `RegisterRawInputDevices` now uses focus-following mode (`hwndTarget = NULL`), while window resolution is kept only for cursor clip/focus decisions. This avoids `ERROR_INVALID_PARAMETER` 87 from bad or overlay-polluted HWNDs and lets registration succeed earlier in startup.
- Raw mouse: prefer the engine’s real `cl_hwnd` (RVA `0x403564`) as the game window, with `ActiveApp` (RVA `0x40351C`) as another guard before draining raw input. Heuristic fallback still exists, but same-process helper/tool windows now score much lower and class `"Quake 2"` scores higher.
- Raw mouse: add richer registration diagnostics, including candidate window dumps and `GetRegisteredRawInputDevices` state, to make injected-overlay conflicts (for example ShadowPlay) much easier to spot.

## v5.1

- Raw mouse: hook `IN_MenuMouse` (SofExe `0x4A420`) with Pre/Post callbacks; while inside it, `GetCursorPos` uses the real API so menu cursor motion matches the OS (avoids synthetic cursor lag on connect / low FPS). `detours.yaml` uses `cvar_t*` parameters; `hooks.json` registers Pre/Post; new `in_menumouse.cpp` tracks scope with a depth counter.
- Raw mouse: fold pending mickeys at read time — `GetCursorPos` calls `raw_mouse_drain_pending_raw_for_cursor()` (shared `GetRawInputBuffer` drain with bounded multi-batch emptying) so the virtual cursor stays fresh without waiting on the next pump boundary.
- Raw mouse: `Sys_SendKeyEvents` only resets deltas (`consume_deltas`); raw queue draining is no longer done there — only the `GetCursorPos` path drains (plus lazy registration when not yet registered). Removed `raw_processed_this_frame` / `raw_mouse_on_peek_returned_no_message`; `raw_mouse_process_raw_mouse()` forwards to the same drain helper.
- Raw mouse: registration cleanup — `RawMouseDropRegistration()`, single `RawMouseCommitRawInputRegistration()` for Win32 register + validation; `raw_mouse_ensure_registered` fast-paths when already registered for the same root (skips heavy window resolution on common `DispatchMessageA` calls). Dropped public `raw_mouse_register_input` (only `ensure_registered` needed it).
- Raw mouse: message pump default `MAX_MSG_PER_FRAME` set to **10** with a short comment on the vanilla full-drain vs capped tradeoff.

## v5.0

- Raw mouse: message pump no longer removes `WM_INPUT` from the queue (avoids high-frequency peel cost on high-poll mice). Uses filtered `PeekMessage` bands around `WM_INPUT` when it blocks the head; otherwise `GetMessage` for strict FIFO. When both low-ID and high-ID candidates exist, orders by `MSG.time` with high-band tie-break for mouse/client messages. Raises per-pump cap to 10 dispatched messages; updates `sys_msg_time` and centralizes quit/dispatch in `DispatchOneMsg`.
- Raw mouse: fix enabling `_sofbuddy_rawmouse` when the engine runs the cvar change callback before `Cvar_Get` returns — set `in_mouse_raw = cvar` at the start of `raw_mouse_on_change`; drop redundant second `raw_mouse_on_change` after registration (avoids false “not enabled” / “registration failed” pairs and duplicate “ENABLED” logs).
- Raw mouse: optional dev logging when `raw_mouse_ensure_registered` is asked to log — “not enabled or API not supported”, “already registered for this window”; `raw_mouse_register_input` logs “Raw input registration succeeded” when registration completes.

## v4.9

- Raw mouse: register raw input only with HWNDs owned by the game process (`GetWindowThreadProcessId` vs `GetCurrentProcessId`). Resolve the window via `GetGUIThreadInfo`, then `GetActiveWindow` / `GetForegroundWindow` only when they are local, plus `EnumThreadWindows` fallback—fixes `RegisterRawInputDevices` failing with error 87 (`ERROR_INVALID_PARAMETER`) when foreground belonged to another app or at startup. Cvar enable uses the same `raw_mouse_ensure_registered` path as the message pump; clearer log lines for invalid HWND and for error 87.
- Raw mouse: the “Wine/X11 raw input” hint is shown only when actually running under Wine/Proton (`is_running_under_wine()`), not on native Windows failures.
- Core: `is_running_under_wine()` implementation moved to `src/utils/util.cpp` with declaration in `hdr/util.h` (shared helper for updater and raw mouse).

## v4.8

- Updater: refactored zip selection to strict priority order (`preferred_release_zip_names`, `pick_best_zip_url`) instead of scoring; native Windows prefers `release_windows.zip` then `release_windows_xp.zip` when the main zip is missing (fixes Windows 7 receiving wine_linux build). Wine detection hardened (validate `wine_get_version` return).
- Update scripts: clarified prompts in `update_from_zip.sh` and `update_from_zip.cmd` for user input options when applying fresh settings.

## v4.7

- Raw mouse: SetCursorPos hook now always calls the original so the OS cursor is warped when the game recenters, fixing dual-monitor focus loss (e.g. left edge on Wine/Kubuntu).

## v4.6

- Raw mouse: message-pump–based flow. Replaced engine-frame hooks (IN_MouseMove, IN_MenuMouse, PeekMessageA, GetMessageA) with Sys_SendKeyEvents replacement and dedicated capped two-range pump so WM_INPUT is never peeked or delivered. One GetRawInputBuffer per frame in Sys_SendKeyEvents path; DispatchMessageA only handles window events (registration/clip). Virtual cursor via GetCursorPos/SetCursorPos. WOW64: buffer size×8 and RAWMOUSE at offset 24. Single cvar 0=off, non-zero=on. README updated.
- Media timers: Sys_SendKeyEvents call site (exe 0x65D5E) now uses my_TimeGetTime (QPC) instead of my_Sys_Milliseconds for consistent frame timing.

## v4.5

- Raw mouse: reimplemented with single setting (0=off, non-zero=on). PH3-style only: two-range PeekMessageA so WM_INPUT is never seen; GetMessageA swallows WM_INPUT (no GetRawInputData); one GetRawInputBuffer per frame in IN_MouseMove/IN_MenuMouse Pre; DispatchMessageA returns 0 for WM_INPUT. No per-message processing or drain-from-handle.
- Internal menus: SoF Buddy RMF assets renamed from sb_* to shorter names (e.g. sb_perf → cpu, sb_input → input, sb_http → network, sb_tweaks_perf → profiles, sb_scaling → ui_scale).
- Docs: README menu.png hero image; raw_mouse and internal_menus README updates. Update-from-zip scripts (Windows/Linux) refinements.

## v4.4

- Raw mouse: reworked raw input pipeline to buffer and drain high-Hz `WM_INPUT` via `GetRawInputBuffer`, skip zero-delta packets, and keep the game's original mouse cvars/flow intact while sourcing movement from hardware deltas instead of OS cursor position.
- Raw mouse: hardened window/foreground and clip handling (no cursor warping on clip refresh, proper unregister on failure/teardown, disabled-path hooks now pure passthrough with no side effects when `_sofbuddy_rawmouse` is 0).
- Raw mouse docs: expanded feature README with callback/override details, disabled-path behavior, jitter/high-polling guidance, and configuration notes so users understand how and when to enable it.
- Docs: README now surfaces a prominent “#1 thing you need to know” section and in-game command docs that emphasize `F12` / `sofbuddy_menu sof_buddy` as the primary entry point into SoF Buddy.

## v4.3

- Docs: add http.png hero image to README.

## v4.2

- Internal menus: RMF layout and content pass across SoF Buddy tabs (main, input, lighting, perf, scaling, social, texture, tweaks, HTTP, update help/prompt), loading header, and HTTP providers; FS_LoadFile override and internal menu hooks refined.
- Menu docs: menu-know-how guidance and build/makefile tweaks.

## v4.1

- Windows updater UX polish: silenced noisy extractor stderr/stdout (`tar`, `7z`, PowerShell fallback) in `update_from_zip.cmd` so successful fallback installs do not show misleading error spam.

## v4.0

- Windows updater launch fix: switched queued installer script path to Windows separators (`sof_buddy\update_from_zip.cmd`) so post-exit `cmd.exe` execution no longer misparses the script path on Win10/11.

## v3.9

- Windows updater path handling: canonicalized `%ZIP_PATH%` to full absolute form before existence/extract checks, preventing malformed Wine path edge-cases.
- VBScript fallback noise cleanup: removed a fragile redirection from `.vbs` generation in `update_from_zip.cmd` to avoid spurious `'2' is not recognized` console errors.

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
