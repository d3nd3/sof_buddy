# SoF Buddy.
## Contents
1. [Features](#features)
1. [Usage](#usage)  
1. [Installation](#installation) 
1. [Wine/Proton](#wineproton)  
1. [Cvars](#cvars)  

## Features 
<details open> <summary><b>[show/hide]</b></summary>

:capital_abcd: **Font Scaling**  
apply `1x 2x 3x 4x` etc font scaling for larger resolutions.

:capital_abcd: **Hud Scaling**  
Scale the hud separately from the font.

:chart_with_upwards_trend: **Restored *cl_maxfps* functionality in singleplayer**  
`_sp_cl_frame_delay` no longer needed for singleplayer.

:chart_with_upwards_trend: **Improved stable framerate whilst Sleep(1) thus saving cpu and energy**  
`QueryPerformanceCounter` usage as frame loop timer instead of `timeGetTime`.  `_sp_cl_cpu_cool` have been deprecated in favour of a new cvar `_sofbuddy_sleep`.

:syringe: **Widescreen Teamicons GlitchFix**  
Correct positioning of player teamicons above player models in-game when using widescreen.

:deciduous_tree: **HD texture support**  
Supports different sized/scaled default `.m32` textures by hard-coding the `uv` texture mapping sizes instead of reading them from the `.bsp`, thus is required when using any HD texture mod. see [hd_textures](https://www.sof1.org/viewtopic.php?p=45667)

:last_quarter_moon: **Lighting blend mode adjustment**  
Experience WhiteMagicRaven's lighting adjustment, disabled by default. credits: `WhiteMagicRaven`.

:white_square_button: **Console Size Adjustment**  
Change percentage of screen the console takes when opened during gameplay.

:syringe: **gl_swapinterval is applied on every vid_restart**  
Your vsync settings will work hassle free.

:syringe: **Sane first time settings for SoF init if del config.cfg eg.**  
There is a bug when changing hardware that sets very bad defaults that got fixed.

:syringe: **Client Console overflow/crashes fixed**  
A paste large text into console crash and 2048px or wider resolution console crash fixed.
</details>

## Installation
<details> <summary><b>[show/hide]</b></summary>

### Obtaining
Either compile the project using `make` or grab the pre-compiled from release section.  

### Extracting
It is recommended to delete your User/config.cfg file before install so that you get optimal defaults.  

Ensure/place the `sof_buddy.dll` file is in your root SoF folder, along with the other 4 scripts.  
(dump everything into the root SoF folder)  

There are provided 3 useful SoF.exe patchers, which control whether `native_wsock/sofplus_wsock/sof_buddy_wsock` are loaded.  This toggles which mods you want loaded.  
`sof_buddy` will automatically load `spcl.dll` if it exists in your directory.  So SoF Buddy can work `_WITH_` sofplus.
</details>

## Usage
<details> <summary><b>[show/hide]</b></summary>

### Activation of SoF Buddy. (required)
Open the `.cmd` file `set_sofplus_and_buddy_sof.cmd`.  This method works without sofplus too.  
View its contents here: [set_sofplus_and_buddy_sof.cmd](https://github.com/d3nd3/sof_buddy/blob/master/set_sofplus_and_buddy_sof.cmd)
### Restoration of SoF Plus only.
Open the `.cmd` file `set_sofplus_sof.cmd`.  
### Disabling SoF Plus _AND_ SoF Buddy. (Native)
Open the `.cmd` file `set_vanilla_sof.cmd`.
</details>


## Wine/Proton
<details> <summary><b>[show/hide]</b></summary>

If you are on linux, I highly recommend using wine instead of proton.  There is less screen tearing, when using true fullscreen (drm.modeset), only noticeable in slight moments. (Because wine has less tearing and less glitchy gun effects/visuals.)  

`wine SoF.exe +set console 1 +set cddir CDDIR #%command%`  
However, if you do decide to use Proton, I recommend <= 4.11-13 , else you have to adjust sound frequency each startup. (It sticks to 11, but 22 is better).

Optimal FPS, especially if using wine:  
base/autoexec.cfg:  
```
cl_quads 0
ghl_light_method 0
ghl_shadows 0
```  
Although cl_quads makes the game's effects disabled so much less visually enjoyable.
</details>


## Cvars
<details> <summary><b>[show/hide]</b></summary>

**_sofbuddy_classic_timer**  
default = 0  
Use the classic timer if you are running with vsync ON or are on an extremely old/laggy system or have bugs with non-classic timer.  You would launch your SoF.exe with `SoF.exe +set _sofbuddy_classic_timer 1`. This cvar is write-only and can only be set at launch-time.  
**_sofbuddy_high_priority**  
default = 1  
This cvar will be saved in config.cfg.  If you do not want the process have process priority of *HIGH*, set this to 0 for *NORMAL*.  
This cvar can be changed at run-time.  
**_sofbuddy_font_scale**  
default = 1  
The value gets rounded, so whole number multipliers of originally font sizes only apply, 1x, 2x, 3x ... etc.  
**_sofbuddy_hud_scale**  
default = 1  
A multiplier for how large your HUD is, includes the scoreboard in multi-player.  
**_sofbuddy_console_size**  
Percentage of screen the console occupies vertically.  
default = 0.35  
range = 0 - 1  
fullscreen = 1  
**_sofbuddy_sleep**  
default = 1  
**_sofbuddy_sleep_gamma**  
default = 300  
**_sofbuddy_sleep_busyticks**  
default = 2  
**_sofbuddy_minfilter_unmipped**  
**_sofbuddy_magfilter_unmipped**  
**_sofbuddy_minfilter_mipped**  
**_sofbuddy_magfilter_mipped**  
**_sofbuddy_minfilter_ui**  
**_sofbuddy_magfilter_ui**  
**_sofbuddy_whiteraven_lighting**  
When this value is 1, _sofbuddy_lightblend_* cvars are overridden  
default = 0  
values = 1 : Enable sharper lighting.  

---

These 2 cvars adjust the blending algorithm for blending lightmaps with textures. See glBlendFunc() on opengl docs. Note that they do nothing if *_sofbuddy_whiteraven_lighting* is enabled, since it uses them to tweak the lighting.   
values:  
`GL_ZERO,GL_ONE,GL_SRC_COLOR,GL_ONE_MINUS_SRC_COLOR,GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_DST_ALPHA,GL_ONE_MINUS_DST_ALPHA,GL_DST_COLOR,GL_ONE_MINUS_DST_COLOR,GL_SRC_ALPHA_SATURATE,GL_CONSTANT_COLOR,GL_ONE_MINUS_CONSTANT_COLOR,GL_CONSTANT_ALPHA,GL_ONE_MINUS_CONSTANT_ALPHA`  
**_sofbuddy_lightblend_dst**  
default: `GL_SRC_COLOR`  
WhiteMagicRaven: `GL_SRC_COLOR`  
**_sofbuddy_lightblend_src**  
default: `GL_ZERO`  
WhiteMagicRaven default: `GL_DST_COLOR`  
</details>
