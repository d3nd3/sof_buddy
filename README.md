# SoF Buddy.

## Features
* **Widescreen Teamicons GlitchFix**: correct positioning of player teamicons above player models in-game when using widescreen.
* **HD texture support**: supports different sized/scaled default .m32 textures by hard-coding the uv texture mapping sizes instead of reading them from the .bsp, thus is required when using any HD texture mod.
* **Improved stable framerate**: QueryPerformanceCounter() usage as frame loop timer instead of timeGetTime().
* Improved lighting contrast.

## Usage
### Installation + Setup
Either compile the project using `make` or grab the pre-compiled from release section.  
Ensure/place the `sof_buddy.dll` file is in your root SoF folder, along with the other 4 scripts.  
(dump everything into the root SoF folder)  

There are provided 3 useful SoF.exe patchers, which control whether native_wsock/sofplus_wsock/sof_buddy_wsock are loaded.  This toggles which mods you want loaded.  
`sof_buddy` will automatically load `spcl.dll` if it exists in your directory.

So to enable sof_buddy, you must run the `.cmd` file **set_sofplus_and_buddy_sof.cmd**.

### Cvars
#### _sofbuddy_classic
default = 0  
If you run on ancient operating system like windows XP, you might not benefit from the improved stable framerate fix.  You would launch your SoF.exe with `SoF.exe +set _sofbuddy_classic 1`. This cvar is write-only and can only be set at launch-time. 
#### _sofbuddy_high_priority
default = 1  
This cvar will be saved in config.cfg.  If you do not want the process have process priority of *HIGH*, set this to 0 for *NORMAL*.  This cvar can be changed at run-time.
#### _sofbuddy_lightblend_dst , _sofbuddy_lightblend_src
Credits WhiteMagicRaven for discovering that lightmaps can be blended differently to give lights more punch. 
For default lighting, change these back to _sofbuddy_lightblend_dst GL_DST_COLOR;_sofbuddy_lightblend_src GL_ZERO