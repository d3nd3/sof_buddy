## SoF Buddy

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE) [![Build](https://img.shields.io/badge/build-passing-brightgreen.svg)]() [![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-lightgrey.svg)]()

Modern enhancements for Soldier of Fortune 1: scaling, fixes, and QoL improvements.

## Features
- Font scaling (`1x`–`4x+`), independent HUD scaling
- Restored `cl_maxfps` in singleplayer
- Stable frame pacing and CPU savings (`_sofbuddy_sleep`)
- Raw mouse input (bypasses OS acceleration)
- Widescreen teamicons fix
- HD `.m32` texture support — see `https://www.sof1.org/viewtopic.php?p=45667`
- Optional lighting blend mode (WhiteMagicRaven)
- Adjustable console size
- Reliable vsync application on `vid_restart`
- Safer defaults and console overflow fixes

## Installation
1) Get SoF Buddy: release `https://github.com/d3nd3/sof_buddy/releases` or build:
```sh
make
```
2) Extract the zip into your SoF root (where `SoF.exe` is). Recommended: delete `User/config.cfg` once to reset bad defaults.
3) Contents: `sof_buddy.dll`, `sof_buddy/` (with `funcmaps/`, enable scripts for Windows `.cmd` and Linux/Wine `.sh`, and patch helpers).
4) Script presets (SoF root):
- `enable_sofplus_and_buddy.*` → loads `sof_buddy.dll` (recommended; auto-loads `spcl.dll` if present)
- `enable_sofplus.*` → loads `spcl.dll` only
- `enable_vanilla.*` → loads `WSOCK32.dll` (vanilla)

## Usage
- Windows: run the matching `.cmd` in `sof_buddy/`
- Linux/Wine: run the matching `.sh` in `sof_buddy/`

## Wine/Proton (Linux)
- Prefer Wine for fullscreen stability
- Launch example:
```sh
wine SoF.exe +set console 1 +set cddir CDDIR #%command%
```
- Proton ≤ 4.11-13 recommended; otherwise adjust sound frequency each start
- True raw mouse (`_sofbuddy_rawmouse`) requires Proton ≥ 9.0 or GE-Proton
- For FPS, add to `base/autoexec.cfg`:
```cfg
cl_quads 0
ghl_light_method 0
ghl_shadows 0
```

## Cvars
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
| `_sofbuddy_rawmouse` | 1 | Enable raw mouse input (1 = on, bypasses Windows acceleration) |

See OpenGL `glBlendFunc` docs for blend values. If `_sofbuddy_whiteraven_lighting` is enabled, it overrides blend cvars.

## Credits
- WhiteMagicRaven — lighting blend mode
- d3nd3 — project lead
- Community — reports, testing, support

## License
MIT — see `LICENSE`.

## Support
- Issues: `https://github.com/d3nd3/sof_buddy/issues`
- Forums: `https://www.sof1.org/`
- Discord: `https://discord.gg/zZjnsRKPEJ`

_SoF Buddy is not affiliated with Raven Software or Activision. Soldier of Fortune is © their respective owners._
