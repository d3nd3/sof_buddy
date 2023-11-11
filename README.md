# SoF Buddy.

## Features
* Widescreen Teamicons GlitchFix: correct positioning of player teamicons above player models in-game when using widescreen.
* HD texture support: supports different sized/scaled default .m32 textures by hard-coding the uv texture mapping sizes instead of reading them from the .bsp, thus is required when using any HD texture mod.
* Improved stable framerate: QueryPerformanceCounter() usage as frame loop timer instead of timeGetTime().