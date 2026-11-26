# Scaled Console

## Purpose
Scales the in-game console to be readable on modern high-resolution displays. Provides configurable font scaling and console size adjustment.

## Callbacks
- **EarlyStartup** (Post, Priority: 70)
  - `scaledCon_EarlyStartup()` - Initializes console scaling system

## Hooks
None (screen dimensions are updated by scaled_ui_base's VID_CheckChanges Post hook).

## OverrideHooks
- **Con_Init** (SoF.exe)
  - `hkCon_Init()` - Registers console scaling CVars during console initialization
- **Con_DrawConsole** (SoF.exe)
  - `hkCon_DrawConsole()` - Scales console background and text rendering
- **Con_DrawNotify** (SoF.exe)
  - `hkCon_DrawNotify()` - Scales console notification messages
- **Con_CheckResize** (SoF.exe)
  - `hkCon_CheckResize()` - Handles console resize events with scaling
- **SCR_DrawPlayerInfo** (SoF.exe)
  - `hkSCR_DrawPlayerInfo()` - Scales player info display in console

## CustomDetours
None

## Configuration CVars
- `_sofbuddy_font_scale` - Font scaling factor (default: 1.0)
- `_sofbuddy_console_size` - Console size as fraction of screen height (default: 0.5)

## Dependencies
- **scaled_ui_base** - Uses shared Draw_StretchPic hook for console background scaling
- Uses shared `glVertex2f` hook for vertex scaling
- Uses shared `g_activeDrawCall` state for context tracking

## Technical Details

### Console Background Scaling
The console background is scaled via the shared `Draw_StretchPic` hook in `scaled_ui_base`. When `CON_DrawConsole` is detected as the caller, the hook adjusts the console height based on `draw_con_frac` and `current_vid_h`.

### Font Scaling
Font rendering is scaled through the shared `glVertex2f` hook. When `g_activeRenderType == Console` and `g_activeDrawCall != StretchPic`, vertices are scaled by `fontScale`.

### Console Size
The `_sofbuddy_console_size` CVar controls what fraction of the screen height the console occupies when fully opened.

### Version stamp

`CON_DrawConsole` draws the bottom-right `v1.07f` version stamp at
`viddef_width - 44`. `Draw_String` advances 8 px per glyph, so the six-character
stamp is already 48 px wide and clipped before font scaling. Early startup
patches the `-44` immediate to `-56` (48 px text + 8 px margin). The offset is
in the virtual width that `hkCon_DrawConsole` divides by the console font scale,
so the margin scales with the font instead of disappearing at 2x.

