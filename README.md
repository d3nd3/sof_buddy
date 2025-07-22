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
- 🎯 **Restored `cl_maxfps` in Singleplayer** — Enjoy smooth, uncapped framerates without legacy workarounds.
- ⚡ **Stable Framerate & CPU Saver** — Uses `QueryPerformanceCounter` for precise timing and energy savings. New cvar: `_sofbuddy_sleep`.
- 🏷️ **Widescreen Teamicons GlitchFix** — Team icons are always correctly positioned, even in widescreen.
- 🖼️ **HD Texture Support** — Native support for high-res `.m32` textures. [Learn more](https://www.sof1.org/viewtopic.php?p=45667)
- 🌙 **Lighting Blend Mode Adjustment** — Experience WhiteMagicRaven's lighting tweaks (optional).
- 🖲️ **Console Size Adjustment** — Set how much of the screen the console covers, for any setup.
- 🔄 **VSync Reliability** — `gl_swapinterval` is applied on every `vid_restart` for hassle-free vsync.
- 🛠️ **Sane Defaults on First Run** — Fixes bad config defaults after hardware changes.
- 🛡️ **Console Overflow/Crash Fixes** — No more crashes from large pastes or ultra-wide resolutions.

</details>

---

## 🚀 Installation
<details>
<summary><b>Click to expand</b></summary>

### 1. Get SoF Buddy
- **Option A:** [Download pre-compiled release](https://github.com/d3nd3/sof_buddy/releases)
- **Option B:** Compile from source:
  ```sh
  make
  ```

### 2. Prepare Your Game Folder
- **Recommended:** Delete your `User/config.cfg` for optimal defaults.
- Place `sof_buddy.dll` and the 4 provided scripts in your SoF root folder.
- Use the included patchers to toggle between `native_wsock`, `sofplus_wsock`, and `sof_buddy_wsock`.
- `sof_buddy` will auto-load `spcl.dll` if present, so it works _with_ SoF Plus.

</details>

---

## 🕹️ Usage
<details>
<summary><b>Click to expand</b></summary>

### Activate SoF Buddy
- Run: `set_sofplus_and_buddy_sof.cmd` (works with or without SoF Plus)
- [View script contents](https://github.com/d3nd3/sof_buddy/blob/master/set_sofplus_and_buddy_sof.cmd)

### Restore SoF Plus Only
- Run: `set_sofplus_sof.cmd`

### Disable All Mods (Vanilla)
- Run: `set_vanilla_sof.cmd`

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
| `_sofbuddy_whiteraven_lighting` | 0 | Enable WhiteMagicRaven lighting (1 = on) |
| `_sofbuddy_lightblend_dst` | GL_SRC_COLOR | Lightmap blend func (see OpenGL docs) |
| `_sofbuddy_lightblend_src` | GL_ZERO | Lightmap blend func (see OpenGL docs) |

- See [OpenGL glBlendFunc docs](https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glBlendFunc.xhtml) for blend values.
- If `_sofbuddy_whiteraven_lighting` is enabled, it overrides blend cvars.

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
- **Discord Server:** [Join our Discord](https://discord.com/invite/690023187274399779)

---

> _SoF Buddy is not affiliated with Raven Software or Activision. Soldier of Fortune is © their respective owners._ 