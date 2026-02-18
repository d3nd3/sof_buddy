# SoF Buddy 🚀

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Build](https://img.shields.io/badge/build-passing-brightgreen.svg)]()
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-lightgrey.svg)]()

> **A modern enhancement suite for Soldier of Fortune 1 — scaling, fixes, and quality-of-life improvements for the best SoF experience.**

---

## ✨ Features
<details open>
<summary><b>Click to expand</b></summary>

- 🔠 **Font Scaling** — Crisp, readable fonts at any resolution: `1x`, `2x`, `3x`, `4x`, etc.
- 🖥️ **HUD Scaling** — Scale the HUD independently from the font for perfect UI balance.
- 🎯 **Crosshair Scaling** — Scale crosshair textures independently for improved visibility.
- 🎯 **Restored `cl_maxfps` in Singleplayer** — Enjoy smooth, uncapped framerates without legacy workarounds.
- ⚡ **Stable Framerate & CPU Saver** — Uses `QueryPerformanceCounter` for precise timing and energy savings. New cvar: `_sofbuddy_sleep`.
- 🖱️ **Raw Mouse Input** — Direct hardware input bypassing Windows acceleration. All original sensitivity cvars still work!
- 🏷️ **Widescreen Teamicons GlitchFix** — Team icons are always correctly positioned, even in widescreen.
- 🖼️ **HD Texture Support** — Native support for high-res `.m32` textures. [Learn more](https://www.sof1.org/viewtopic.php?p=45667)
- 🌙 **Lighting Blend Mode Adjustment** — Experience WhiteMagicRaven's lighting tweaks (optional).
- 🖲️ **Console Size Adjustment** — Set how much of the screen the console covers, for any setup.
- 🔄 **VSync Reliability** — `gl_swapinterval` is applied on every `vid_restart` for hassle-free vsync.
- 🛠️ **Sane Defaults on First Run** — Fixes bad config defaults after hardware changes.
- 🛡️ **Console Overflow/Crash Fixes** — No more crashes from large pastes or ultra-wide resolutions.
- 🧾 **Large `config.cfg` Exec Fix** — Avoid `Cbuf_AddText: overflow` when running `exec config.cfg` with very large configs.
- 🧩 **Embedded Loading / Internal Menus** — Serve RMF menu assets from memory (includes loading UI) and open internal pages via `sofbuddy_menu`.
- 🌐 **HTTP Map Download Assist** — Download missing maps over HTTP and resume precache; provider URLs configurable via cvars.
- ⬆️ **In-Game Updater + Startup Prompt** — Check/download release zips in-game, with optional startup check and internal-menu prompt.
- 🧓 **Windows XP Updater Mirror Defaults** — XP builds default updater feed URLs to `sofvault.org` (overrideable via cvars).
- 📚 **Feature Docs** — See `src/features/internal_menus/README.md`, `src/features/http_maps/README.md`, and `src/features/cbuf_limit_increase/README.md`.

</details>

---

## 🚀 Installation
<details>
<summary><b>Click to expand</b></summary>

### 1. Get SoF Buddy
- **Option A:** [Download pre-compiled release](https://github.com/d3nd3/sof_buddy/releases)
- **Option B:** Compile from source:
  ```sh
  make               # Release build (optimized)
  make BUILD=xp      # Windows XP-targeted build
  make debug         # Debug build (with logging)
  make BUILD=xp-debug # XP-targeted debug build
  make debug-gdb     # Debug build with GDB breakpoint function
  make debug-collect # Debug build with func_parents collection
  ```
  See [docs/DEBUGGING.md](docs/DEBUGGING.md) for details on build configurations.

### 2. Prepare Your Game Folder
- **Recommended:** Delete your `User/config.cfg` for optimal defaults.
- Extract the release `.zip` directly into your SoF root (where `SoF.exe` lives). It contains:
  - `sof_buddy.dll` (goes in the SoF root)
  - `sof_buddy/` folder (created under the SoF root)
    - `sof_buddy/funcmaps/` (contains JSON function maps)
    - Windows: `sof_buddy/enable_*.cmd` scripts and `sof_buddy/patch_sof_binary.ps1`
    - Linux/Wine: `sof_buddy/enable_*.sh` scripts and `sof_buddy/patch_sof_binary.sh`
- Use the helper scripts to patch SoF.exe to load different DLLs:
  - **`enable_sofplus_and_buddy.cmd`** → Loads `sof_buddy.dll` (recommended: SoF Buddy + optional SoF Plus)
  - **`enable_sofplus.cmd`** → Loads `spcl.dll` (SoF Plus only, disables SoF Buddy)
  - **`enable_vanilla.cmd`** → Loads `WSOCK32.dll` (vanilla SoF, no mods)
  - **`update_from_zip.cmd`** → Extract newest downloaded SoF Buddy update zip (`sof_buddy/update/*.zip`) into SoF root
- SoF Buddy auto-loads `spcl.dll` if present, so it works _with_ SoF Plus.

</details>

---

## 🕹️ Usage
<details>
<summary><b>Click to expand</b></summary>

### Enable SoF Buddy (Recommended)
- **Windows:** Run `sof_buddy/enable_sofplus_and_buddy.cmd`
- **Linux/Wine:** Run `sof_buddy/enable_sofplus_and_buddy.sh`
- This enables SoF Buddy and optionally loads SoF Plus (if `spcl.dll` is present)
- To apply a downloaded update zip, run:
  - **Windows:** `sof_buddy/update_from_zip.cmd`
  - **Linux/Wine:** `sof_buddy/update_from_zip.sh`

### Enable SoF Plus Only
- **Windows:** Run `sof_buddy/enable_sofplus.cmd`
- **Linux/Wine:** Run `sof_buddy/enable_sofplus.sh`
- This disables SoF Buddy and uses only SoF Plus

### Restore Vanilla SoF
- **Windows:** Run `sof_buddy/enable_vanilla.cmd`
- **Linux/Wine:** Run `sof_buddy/enable_vanilla.sh`
- This removes all mods and restores the original game

### In-Game Commands
- `sofbuddy_list_features` — Print compiled features (and whether they are on/off).
- `sofbuddy_menu <name>` — Open an embedded internal menu page (examples: `loading`, `sof_buddy`).
- `sofbuddy_menu <menu>/<page>` — Open a specific embedded page inside a menu (example: `sofbuddy_menu sof_buddy/sb_perf`).
- `F12` is auto-bound at startup to `sofbuddy_menu sof_buddy`.
- `sofbuddy_apply_menu_hotkey` — Re-apply bind from `_sofbuddy_menu_hotkey` (default `F12`).
- `sofbuddy_apply_profile_comp` — Apply competitive/low-visual profile preset.
- `sofbuddy_apply_profile_visual` — Apply visual/high-fidelity profile preset.
- `sofbuddy_update` — Check latest release from configured update API endpoint and compare with your current version.
- `sofbuddy_update download` — Download latest release zip to `sof_buddy/update/` (apply after closing game; release zips are preferred over debug zips).
- `sofbuddy_openurl <https_url>` — Open trusted community links in your default browser (used by the Social Links menu page).

</details>

---

## 🧓 Windows XP
<details>
<summary><b>Click to expand</b></summary>

- Use the **XP package** from releases (`release_windows_xp.zip`) or compile with:
  ```sh
  make BUILD=xp
  ```
- XP builds define `SOFBUDDY_XP_BUILD` and default updater endpoints to:
  - `_sofbuddy_update_api_url` = `http://sofvault.org/sof_buddy/releases/latest.json`
  - `_sofbuddy_update_releases_url` = `http://sofvault.org/sof_buddy/releases/latest`
- If you run your own mirror/proxy feed, override in console:
  ```cfg
  set _sofbuddy_update_api_url "http://your-endpoint/latest.json"
  set _sofbuddy_update_releases_url "http://your-endpoint/releases/latest"
  ```
- XP users may hit WinHTTP TLS failures on modern HTTPS endpoints (for example GitHub API). The mirror defaults above avoid that path.

</details>

---

## 🍷 Wine/Proton (Linux)
<details>
<summary><b>Click to expand</b></summary>

- **Recommendation:** Use Wine for best fullscreen experience and fewer visual glitches.
- **Launch Example:**
  ```sh
  wine SoF.exe +set console 1 +set cddir CDDIR #%command%
  ```
- **Proton Note:** Proton ≤ 4.11-13 recommended. Otherwise, adjust sound frequency each startup.
- **Raw Mouse Input (`_sofbuddy_rawmouse`):** For true raw mouse input (bypassing system acceleration), you need **Proton ≥ 9.0** or **GloriousEggroll's custom Proton builds**. Standard Wine/wine-staging may still apply system mouse acceleration.
- **Optimal FPS Tweaks:** Add to `base/autoexec.cfg`:
  ```
  cl_quads 0
  ghl_light_method 0
  ghl_shadows 0
  ```
  (Note: `cl_quads 0` disables many effects.)

</details>

---

## ⚙️ Cvars (Configuration Variables)
<details>
<summary><b>Click to expand</b></summary>

| Cvar | Default | Description |
|------|---------|-------------|
| `_sofbuddy_classic_timer` | 0 | Use classic timer (set at launch, for vsync/old systems) |
| `_sofbuddy_high_priority` | 1 | Set process priority to HIGH (set to 0 for NORMAL) |
| `_sofbuddy_font_scale` | 1 | Font scaling multiplier (1x, 2x, 3x, ...) |
| `_sofbuddy_hud_scale` | 1 | HUD scaling multiplier |
| `_sofbuddy_crossh_scale` | 1 | Crosshair scaling multiplier |
| `_sofbuddy_console_size` | 0.35 | Console height as % of screen (0-1, 1=fullscreen) |
| `_sofbuddy_sleep` | 1 | Enable CPU-saving sleep |
| `_sofbuddy_sleep_jitter` | 0 | Frame squashing for missed frames (desperate use only) |
| `_sofbuddy_sleep_busyticks` | 2 | 1ms busyloop ticks (lower = less CPU, 0 = stutter) |
| `_sofbuddy_minfilter_unmipped` | — | Texture filtering |
| `_sofbuddy_magfilter_unmipped` | — | Texture filtering |
| `_sofbuddy_minfilter_mipped` | — | Texture filtering |
| `_sofbuddy_magfilter_mipped` | — | Texture filtering |
| `_sofbuddy_minfilter_ui` | — | Texture filtering |
| `_sofbuddy_magfilter_ui` | — | Texture filtering |
| `_sofbuddy_lighting_overbright` | 0 | Enable overbright lighting (1 = on) |
| `_sofbuddy_lighting_cutoff` | 64 | Lighting cutoff value (float) |
| `_sofbuddy_water_size` | 64 | warp polygon size - energy swirl frequency (polygonsize smaller=faster) (minimum: 16) |
| `_sofbuddy_lightblend_dst` | GL_SRC_COLOR | Lightmap blend func (see OpenGL docs) |
| `_sofbuddy_lightblend_src` | GL_ZERO | Lightmap blend func (see OpenGL docs) |
| `_sofbuddy_shiny_spherical` | 1 | should shiny gl_detailtexturing change with viewangles? |
| `_sofbuddy_rawmouse` | 0 | Enable raw mouse input (1 = on, bypasses Windows acceleration) |
| `sofbuddy_menu_internal` | 0 | Internal menus: serve embedded RMF assets from memory (normally toggled automatically) |
| `_sofbuddy_update_status` | `idle` | Last updater status text (for console/internal menus) |
| `_sofbuddy_update_latest` | `""` | Latest release tag discovered by `sofbuddy_update` from configured updater feed |
| `_sofbuddy_update_download_path` | `""` | Last downloaded update zip path |
| `_sofbuddy_update_checked_utc` | `""` | UTC timestamp of last update check |
| `_sofbuddy_update_check_startup` | 0 | If `1`, runs updater check on startup and opens internal prompt when update is found |
| `_sofbuddy_update_api_url` | build-dependent | Updater JSON feed URL (XP build defaults to sofvault mirror, non-XP defaults to GitHub API) |
| `_sofbuddy_update_releases_url` | build-dependent | Human release page URL shown in updater status/help |
| `_sofbuddy_openurl_status` | `idle` | Last social-link open status (`opened`, `blocked`, or `open failed`) |
| `_sofbuddy_openurl_last` | `""` | Last URL requested through `sofbuddy_openurl` |
| `_sofbuddy_menu_hotkey` | `F12` | Preferred key token used by `sofbuddy_apply_menu_hotkey` / internal menu bind sync |
| `_sofbuddy_perf_profile` | 0 | Perf profile selector (`0=Competitive`, `1=Visual`) used by internal Perf Tweaks page |
| `_sofbuddy_loading_lock_input` | 1 | Loading menu input lock mode (`1` locked, `0` unlocked) |
| `_sofbuddy_http_maps` | 1 | HTTP map assist mode: `0=Off`, `1=Primary`, `2=Random`, `3=Rotate` |
| `_sofbuddy_http_maps_dl_1` | `https://raw.githubusercontent.com/plowsof/sof1maps/main` | Zip download base URL (index 1) |
| `_sofbuddy_http_maps_dl_2` | `""` | Zip download base URL (index 2) |
| `_sofbuddy_http_maps_dl_3` | `""` | Zip download base URL (index 3) |
| `_sofbuddy_http_maps_crc_1` | `https://raw.githubusercontent.com/plowsof/sof1maps/main` | CRC lookup base URL (index 1, Range fetch of zip central directory) |
| `_sofbuddy_http_maps_crc_2` | `""` | CRC lookup base URL (index 2) |
| `_sofbuddy_http_maps_crc_3` | `""` | CRC lookup base URL (index 3) |
| `_sofbuddy_http_show_providers` | 0 | HTTP tab toggle to show/hide provider + updater URL input fields |

- See [OpenGL glBlendFunc docs](https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glBlendFunc.xhtml) for blend values.
- If `_sofbuddy_lighting_overbright` is enabled, it overrides blend cvars.
- `internal_menus` loading UI cvars used by current slim loading pages: `_sofbuddy_loading_progress`, `_sofbuddy_loading_current`.

</details>

---

## 🤝 Credits
- **WhiteMagicRaven** — Lighting blend mode
- **d3nd3** — Project lead
- **Community** — Bug reports, testing, and support

---

## 📄 License
This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.

---

## 💬 Need Help?
- [Open an issue](https://github.com/d3nd3/sof_buddy/issues)
- [SoF1.org Forums](https://www.sof1.org/)
- **Discord Server:** [Join our Discord](https://discord.gg/zZjnsRKPEJ)

---

> _SoF Buddy is not affiliated with Raven Software or Activision. Soldier of Fortune is © their respective owners._ 
