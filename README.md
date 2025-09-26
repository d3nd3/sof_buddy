# SoF Buddy 🚀

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Build](https://img.shields.io/badge/build-passing-brightgreen.svg)]()
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-lightgrey.svg)]()
[![Release](https://img.shields.io/badge/release-v1.0.0-blue.svg)]()

> **A modern enhancement suite for Soldier of Fortune 1** — scaling, fixes, and quality-of-life improvements for the best SoF experience on modern systems.

SoF Buddy is a DLL-based enhancement for the classic first-person shooter **Soldier of Fortune** (2000). It patches the game at runtime to add modern features like high-resolution font scaling, improved framerate stability, widescreen support fixes, and various quality-of-life improvements without modifying the original game files.

---

## 📋 Table of Contents
- [✨ Features](#-features)
- [📥 Installation](#-installation)
- [🕹️ Usage](#️-usage)
- [⚙️ Configuration](#️-configuration)
- [🍷 Wine/Proton (Linux)](#-wineproton-linux)
- [🔧 Troubleshooting](#-troubleshooting)
- [⚙️ Cvars Reference](#️-cvars-reference)
- [👥 Contributing](#-contributing)
- [📄 License](#-license)

---

## 🎯 What is SoF Buddy?

Soldier of Fortune was released in 2000 and wasn't designed for modern high-resolution displays, multi-core CPUs, or current Windows versions. SoF Buddy enhances the game by:

- **Patching at runtime** - No permanent modifications to game files
- **DLL injection** - Loads alongside the original game
- **Modern compatibility** - Works with Windows 10/11 and modern Linux
- **Enhanced visuals** - Better scaling and rendering for high-DPI displays
- **Performance improvements** - Stable framerates and CPU optimization
- **Quality of life** - Console improvements, bug fixes, and usability enhancements

---

## ✨ Features

### 🎨 **Visual Enhancements**
- 🔠 **Font Scaling** — Crisp, readable fonts at any resolution (`1x`, `2x`, `3x`, `4x`, etc.)
- 🖥️ **HUD Scaling** — Scale the HUD independently from fonts for perfect UI balance
- 🏷️ **Widescreen Team Icons** — Correctly positioned team icons in widescreen resolutions
- 🖼️ **HD Texture Support** — Native support for high-resolution `.m32` textures
- 🌙 **Lighting Improvements** — Optional WhiteMagicRaven lighting blend mode adjustments

### ⚡ **Performance & Stability**
- 🎯 **Uncapped Framerates** — Restored `cl_maxfps` functionality in singleplayer
- ⚡ **CPU Optimization** — Uses high-precision timers with configurable sleep settings
- 🔄 **VSync Reliability** — `gl_swapinterval` applied consistently on every `vid_restart`
- 🛡️ **Crash Prevention** — Fixes console overflow crashes and large paste issues

### 🛠️ **Quality of Life**
- 🖲️ **Console Size Control** — Adjustable console height for any screen setup
- 🛠️ **Smart Defaults** — Optimized configuration defaults after hardware changes
- 🔧 **Runtime Patching** — No permanent game file modifications required
- 🔄 **Multi-Mod Support** — Compatible with SoF Plus and other enhancements

### 🐧 **Cross-Platform**
- ✅ **Windows 10/11** — Full compatibility with modern Windows versions
- 🐧 **Linux Support** — Optimized for Wine/Proton with specific tweaks
- 🎮 **Modern Hardware** — Designed for current CPUs, GPUs, and displays

---

---

## 📥 Installation

### 📋 System Requirements
- **Windows:** Windows 7 or later (tested on Windows 10/11)
- **Linux:** Wine 4.0+ or Proton 4.11-13 or later
- **Soldier of Fortune:** Original game (Gold Edition recommended)
- **Disk Space:** ~2MB for SoF Buddy files

### 🔧 Installation Steps

#### Step 1: Obtain SoF Buddy
**Option A: Download Pre-compiled Release (Recommended)**
- Download the latest release from [GitHub Releases](https://github.com/d3nd3/sof_buddy/releases)
- Extract the ZIP file contents to your Soldier of Fortune installation directory

**Option B: Compile from Source**
```bash
# Clone the repository
git clone https://github.com/d3nd3/sof_buddy.git
cd sof_buddy

# Compile for Windows (requires MinGW cross-compiler)
make

# Or compile for Linux (if supported)
make linux
```

#### Step 2: Prepare Your Game Directory
1. **Locate your SoF installation** - Usually `C:\Program Files\Soldier of Fortune` (Windows) or `~/.wine/drive_c/SoF/` (Linux)
2. **Backup your config** (optional) - Copy `User/config.cfg` to `User/config.cfg.backup`
3. **Clean config** (recommended) - Delete `User/config.cfg` for optimal defaults
4. **Extract files** - Place these files in your SoF root directory:
   - `sof_buddy.dll` (main enhancement DLL)
   - `rsrc/win_scripts/` (Windows scripts)
   - `rsrc/lin_scripts/` (Linux scripts)

#### Step 3: Install Required Files
The following files should be in your SoF root directory:
```
SoF/
├── sof_buddy.dll              # Main enhancement DLL
├── set_sofplus_and_buddy_sof.cmd/.sh  # Activation scripts
├── set_sofplus_sof.cmd/.sh            # SoF Plus only scripts
├── set_vanilla_sof.cmd/.sh            # Vanilla game scripts
└── patch_sof_binary.ps1/.sh           # Binary patching scripts
```

#### Step 4: Choose Your Configuration

**🔄 With SoF Plus (Recommended):**
- Run `set_sofplus_and_buddy_sof.cmd` (Windows) or `set_sofplus_and_buddy_sof.sh` (Linux)
- This loads both SoF Plus and SoF Buddy together

**⚡ SoF Buddy Only:**
- Run `set_sofplus_sof.cmd` (Windows) or `set_sofplus_sof.sh` (Linux)
- This loads only SoF Buddy (if you don't have SoF Plus)

**🎮 Vanilla Game:**
- Run `set_vanilla_sof.cmd` (Windows) or `set_vanilla_sof.sh` (Linux)
- This restores the original unmodified game

### 🚨 Important Notes
- **No permanent changes** - The patching is reversible and doesn't modify your game files
- **Auto-detection** - SoF Buddy automatically detects and loads alongside `spcl.dll` if present
- **Script contents** - View activation script contents: [Windows](rsrc/win_scripts/set_sofplus_and_buddy_sof.cmd) | [Linux](rsrc/lin_scripts/set_sofplus_and_buddy_sof.sh)

---

---

## 🕹️ Usage

### 🚀 Quick Start
1. **Install SoF Buddy** using the steps above
2. **Run the activation script** for your desired configuration
3. **Launch the game** - SoF Buddy will automatically load
4. **Configure settings** in-game using the console or config file

### 📜 Configuration Methods

#### In-Game Console
Press `~` (tilde) to open the console, then type:
```console
set _sofbuddy_font_scale 2     # 2x font scaling
set _sofbuddy_hud_scale 1.5    # 1.5x HUD scaling
set _sofbuddy_sleep 1          # Enable CPU saving
vid_restart                    # Apply changes
```

#### Config File
Edit `User/config.cfg` and add:
```cfg
set _sofbuddy_font_scale "2"
set _sofbuddy_hud_scale "1.5"
set _sofbuddy_console_size "0.4"
set _sofbuddy_sleep "1"
```

#### Command Line
Launch with custom settings:
```bash
SoF.exe +set _sofbuddy_font_scale 2 +set _sofbuddy_sleep 1
```

### 🎛️ Basic Configuration Examples

**High-Resolution Setup (1440p+):**
```cfg
set _sofbuddy_font_scale "2"
set _sofbuddy_hud_scale "1.5"
set _sofbuddy_console_size "0.3"
```

**Performance-Focused Setup:**
```cfg
set _sofbuddy_sleep "1"
set _sofbuddy_sleep_busyticks "1"
set _sofbuddy_high_priority "0"
```

**Visual Quality Setup:**
```cfg
set _sofbuddy_whiteraven_lighting "1"
set _sofbuddy_font_scale "3"
set _sofbuddy_hud_scale "2"
```

---

---

## 🍷 Wine/Proton (Linux)

### 🐧 Linux Setup Guide

#### Recommended Setup
**Wine Configuration:**
1. Install Wine: `sudo apt install wine` (Ubuntu/Debian) or `sudo dnf install wine` (Fedora)
2. Configure Wine prefix: `WINEPREFIX=~/.wine-sof winecfg`
3. Install Soldier of Fortune in the Wine prefix
4. Run the Linux activation script: `./set_sofplus_and_buddy_sof.sh`

#### Launch Commands

**Basic Launch:**
```bash
cd /path/to/SoF
wine SoF.exe +set console 1
```

**With CD Audio:**
```bash
wine SoF.exe +set console 1 +set cddir /path/to/cd/audio
```

**Steam/Proton Launch:**
```bash
cd /path/to/SoF
proton run SoF.exe +set console 1
```

#### Performance Optimization

**Add to `base/autoexec.cfg`:**
```cfg
// Disable resource-intensive effects for better performance
cl_quads 0              // Disable quad-based effects
ghl_light_method 0       // Use simpler lighting
ghl_shadows 0           // Disable shadows

// SoF Buddy specific settings
set _sofbuddy_sleep 1
set _sofbuddy_sleep_busyticks 1
set _sofbuddy_font_scale 2
set _sofbuddy_hud_scale 1.5
```

#### Audio Configuration
**If you experience audio issues:**
1. Run `winecfg` and set audio driver to "ALSA" or "PulseAudio"
2. Test with: `wine SoF.exe +set s_khz 22 +set console 1`
3. If crackling occurs, try: `s_khz 11` (lower quality but stable)

#### Known Issues & Solutions
- **Screen tearing:** Use fullscreen mode instead of windowed
- **Mouse lag:** Add `mouse_raw_input 1` to your config
- **Controller issues:** Use `in_joystick 0` to disable joystick input

### 🔧 Troubleshooting
- **Proton ≤ 4.11-13 recommended** for best compatibility
- **Sound frequency:** May need adjustment on newer Proton versions
- **Visual glitches:** Try different Wine versions if you encounter issues

---

---

## 🔧 Troubleshooting

### 🚨 Common Issues & Solutions

#### ❌ SoF Buddy Won't Load
**Symptoms:** No visual changes, console shows no SoF Buddy messages
**Solutions:**
1. **Check file placement** - Ensure `sof_buddy.dll` is in your SoF root directory
2. **Run activation script** - Must run the appropriate `set_*.cmd/sh` script first
3. **Check permissions** - Ensure the DLL has read/execute permissions
4. **Antivirus interference** - Add exception for `sof_buddy.dll` in your antivirus

#### 🎮 Game Won't Start
**Solutions:**
1. **Run as administrator** (Windows)
2. **Check SoF installation** - Verify game files are not corrupted
3. **Try compatibility mode** - Set SoF.exe to Windows XP compatibility
4. **Check dependencies** - Ensure required DLLs are present

#### 📺 Visual Issues
**Blurry fonts/HUD:**
```cfg
set _sofbuddy_font_scale 2
set _sofbuddy_hud_scale 1.5
vid_restart
```

**HUD too large/small:**
```cfg
set _sofbuddy_hud_scale 1.0  # Reset to default
vid_restart
```

**Widescreen issues:**
- Check if using correct aspect ratio in video settings
- Try different resolutions in `vid_restart`

#### ⚡ Performance Issues
**High CPU usage:**
```cfg
set _sofbuddy_sleep 1
set _sofbuddy_sleep_busyticks 1
```

**Frame drops/stuttering:**
```cfg
set _sofbuddy_sleep_jitter 1  # Only if desperate
set _sofbuddy_classic_timer 1  # Try classic timer
```

#### 🔊 Audio Problems
**Crackling/popping:**
```cfg
set s_khz 11  # Try lower sample rate
set s_mixahead 0.1  # Reduce audio latency
```

**No audio:**
- Check Windows/Linux audio settings
- Try different audio driver in Wine

#### 🐧 Linux/Wine Specific Issues
**Mouse not working:**
```cfg
set mouse_raw_input 1
```

**Screen tearing:**
- Use fullscreen mode instead of windowed
- Enable vsync: `gl_swapinterval 1`

**CD Audio issues:**
```bash
# Mount CD or specify path
wine SoF.exe +set cddir /path/to/cd/audio
```

### 📞 Getting Help
- **Check the logs** - Look in SoF console for error messages
- **Test vanilla first** - Try without SoF Buddy to isolate issues
- **Community support** - Ask questions on [SoF1.org Forums](https://www.sof1.org/)
- **Bug reports** - [Open an issue](https://github.com/d3nd3/sof_buddy/issues)

---

## ⚙️ Cvars Reference

### 🎨 Visual Settings

| Cvar | Default | Range | Description |
|------|---------|-------|-------------|
| `_sofbuddy_font_scale` | `1` | `1-10` | Font scaling multiplier (whole numbers only) |
| `_sofbuddy_hud_scale` | `1` | `0.1-5.0` | HUD elements scaling multiplier |
| `_sofbuddy_console_size` | `0.35` | `0-1` | Console height as percentage of screen |

### ⚡ Performance Settings

| Cvar | Default | Range | Description |
|------|---------|-------|-------------|
| `_sofbuddy_sleep` | `1` | `0-1` | Enable CPU-saving sleep mode |
| `_sofbuddy_sleep_busyticks` | `2` | `0-10` | Busy-loop ticks before sleep (lower = less CPU) |
| `_sofbuddy_sleep_jitter` | `0` | `0-1` | Frame squashing for missed frames (use only if desperate) |
| `_sofbuddy_classic_timer` | `0` | `0-1` | Use classic timer (for vsync/old systems) - set at launch only |
| `_sofbuddy_high_priority` | `1` | `0-1` | Set process priority to HIGH (0 = NORMAL) |

### 🌙 Lighting Settings

| Cvar | Default | Range | Description |
|------|---------|-------|-------------|
| `_sofbuddy_whiteraven_lighting` | `0` | `0-1` | Enable WhiteMagicRaven lighting mode |
| `_sofbuddy_lightblend_dst` | `GL_SRC_COLOR` | OpenGL constants | Lightmap blend destination function |
| `_sofbuddy_lightblend_src` | `GL_ZERO` | OpenGL constants | Lightmap blend source function |

### 🖼️ Texture Filtering Settings

| Cvar | Default | Description |
|------|---------|-------------|
| `_sofbuddy_minfilter_unmipped` | Auto | Minification filter for unmipped textures |
| `_sofbuddy_magfilter_unmipped` | Auto | Magnification filter for unmipped textures |
| `_sofbuddy_minfilter_mipped` | Auto | Minification filter for mipped textures |
| `_sofbuddy_magfilter_mipped` | Auto | Magnification filter for mipped textures |
| `_sofbuddy_minfilter_ui` | Auto | Minification filter for UI elements |
| `_sofbuddy_magfilter_ui` | Auto | Magnification filter for UI elements |

### 📝 Notes
- **OpenGL Blend Values:** See [glBlendFunc documentation](https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glBlendFunc.xhtml)
- **WhiteMagicRaven Lighting:** When enabled, overrides manual blend cvar settings
- **Launch-only Cvars:** `_sofbuddy_classic_timer` must be set before game starts
- **Apply Changes:** Most settings require `vid_restart` to take effect

---

---

## 👥 Contributing

### 🚀 Development Setup

#### Prerequisites
- **Windows:** MinGW cross-compiler (`i686-w64-mingw32-g++`)
- **Linux:** GCC cross-compiler for Windows targets
- **Build tools:** Make, Git

#### Building from Source
```bash
# Clone repository
git clone https://github.com/d3nd3/sof_buddy.git
cd sof_buddy

# Build for Windows
make

# Build debug version
make debug

# Clean build artifacts
make clean
```

#### Project Structure
```
sof_buddy/
├── src/                    # Source code
│   ├── sof_buddy.cpp      # Main DLL entry point
│   ├── cvars.cpp          # Configuration variables
│   ├── features/          # Feature implementations
│   └── DetourXS/          # Function hooking library
├── hdr/                   # Header files
├── makefile              # Build configuration
└── rsrc/                 # Scripts and resources
```

#### Code Style Guidelines
- Use 4-space indentation
- Follow existing naming conventions (`_sofbuddy_*` for cvars)
- Include comments for complex logic
- Keep functions focused and modular

### 🐛 Reporting Bugs
1. **Test with vanilla** - Verify issue occurs without SoF Buddy
2. **Check existing issues** - Search for similar reports
3. **Provide details** - Include system specs, SoF version, and reproduction steps
4. **Use issue template** - Fill out all requested information

### 💡 Feature Requests
- **Be specific** - Describe the desired functionality clearly
- **Explain use case** - Why would this benefit other users?
- **Consider alternatives** - Are there existing ways to achieve this?

### 📝 Documentation
- **Keep updated** - Update README when adding new features
- **Include examples** - Add usage examples for new cvars
- **Clear descriptions** - Write clear, concise documentation

---

## 🤝 Credits
- **WhiteMagicRaven** — Original lighting blend mode implementation
- **d3nd3** — Project lead and main developer
- **Community Contributors** — Bug reports, testing, feedback, and suggestions
- **DetoursXS** — Function hooking library by Michael Platings

---

## 📄 License
This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.

---

## 💬 Support & Community

### 🆘 Getting Help
- **🐛 Bug Reports:** [GitHub Issues](https://github.com/d3nd3/sof_buddy/issues)
- **💬 Forums:** [SoF1.org Community](https://www.sof1.org/)
- **🎮 Discord:** [Join our Discord](https://discord.gg/zZjnsRKPEJ)

### 📚 Resources
- **HD Textures Guide:** [SoF1.org Textures](https://www.sof1.org/viewtopic.php?p=45667)
- **OpenGL Documentation:** [glBlendFunc Reference](https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glBlendFunc.xhtml)
- **Wine HQ:** [Wine Application Database](https://appdb.winehq.org/objectManager.php?sClass=application&iId=1045)

---

## ⚠️ Disclaimer
> _SoF Buddy is not affiliated with Raven Software, Activision, or any official Soldier of Fortune entities. Soldier of Fortune and all related trademarks are property of their respective owners. This project is developed by fans for educational and entertainment purposes._ 
