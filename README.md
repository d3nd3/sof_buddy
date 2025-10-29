# SoF Buddy

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-lightgrey.svg)]()

Modern enhancements for Soldier of Fortune 1: scaling, stability, QoL.

## Features
- Font/HUD scaling
- Restored `cl_maxfps` (singleplayer)
- Stable framerate, lower CPU (`_sofbuddy_sleep`)
- Raw mouse input
- Widescreen teamicons fix
- HD `.m32` textures
- Lighting blend tweaks (optional)
- Console resize
- VSync reliability
- Safer console, sane defaults

See `src/features/FEATURES.md` and feature READMEs under `src/features/*/README.md`.

## Quick start
1) Download a release: `https://github.com/d3nd3/sof_buddy/releases`
2) Extract the zip into your SoF root (where `SoF.exe` lives)
3) Run one script:
   - Windows: `sof_buddy/enable_sofplus_and_buddy.cmd`
   - Linux/Wine: `sof_buddy/enable_sofplus_and_buddy.sh`

Notes
- Delete `User/config.cfg` once to get sane defaults (optional)
- Buddy auto-loads `spcl.dll` if present (works with SoF Plus)

## Build from source
```sh
make
```

More: `docs/ADVANCED.md`, `docs/MODULE_LOADING.md`, `src/dispatchers/README.md`.

## Linux (Wine/Proton)
- Prefer Wine for fewer fullscreen issues
- Launch example:
```sh
wine SoF.exe +set console 1 +set cddir CDDIR
```
- Proton: ≤ 4.11-13 is safest for audio; newer may need per-run sound tweaks
- Raw mouse (`_sofbuddy_rawmouse`): needs Proton ≥ 9.0 or GE-Proton to truly bypass accel

## Cvars (selection)
| Cvar | Default | Description |
|------|---------|-------------|
| `_sofbuddy_font_scale` | 1 | Font scale (1,2,3,...) |
| `_sofbuddy_hud_scale` | 1 | HUD scale |
| `_sofbuddy_console_size` | 0.35 | Console height (0-1) |
| `_sofbuddy_sleep` | 1 | Reduce CPU usage |
| `_sofbuddy_rawmouse` | 1 | Raw mouse input |
| `_sofbuddy_whiteraven_lighting` | 0 | Enable lighting tweak |

Full list: see feature docs and source comments.

## Help
- Issues: `https://github.com/d3nd3/sof_buddy/issues`
- Forum: `https://www.sof1.org/`
- Discord: `https://discord.gg/zZjnsRKPEJ`

## Credits
- WhiteMagicRaven (lighting)
- d3nd3 (lead)
- Community

## License
MIT. See `LICENSE`.

_Not affiliated with Raven/Activision. Soldier of Fortune © their owners._
