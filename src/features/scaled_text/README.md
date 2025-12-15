# Scaled Text

## Purpose
Scales text rendering throughout the game to be readable on modern high-resolution displays. Handles font scaling for menus, HUD text, and in-game messages.

## Callbacks
- **RefDllLoaded** (Post, Priority: 60)
  - `scaledText_RefDllLoaded()` - Installs custom detours for font rendering vertex manipulation

## Hooks
- **VID_CheckChanges** (Post, Priority: 100)
  - `vid_checkchanges_post()` - Updates screen dimensions when video mode changes

## OverrideHooks
- **R_DrawFont** (SoF.exe)
  - `hkR_DrawFont()` - Scales font rendering and tracks font caller context

## CustomDetours
- **DrawFont vertex functions** (ref.dll)
  - `my_glVertex2f_DrawFont_1/2/3/4()` - Custom vertex scaling for font rendering
  - Installed at runtime in `scaledText_RefDllLoaded()`
  - Addresses: `ref.dll + 0x00004860, 0x00004892, 0x000048D2, 0x00004903`

## Configuration CVars
- `_sofbuddy_font_scale` - Font scaling factor (default: 1.0, shared with scaled_con)

## Dependencies
- **scaled_ui_base** - Uses shared infrastructure for font scaling
- Uses shared `g_activeDrawCall` state for context tracking

## Technical Details

### Font Scaling Mechanism
Text scaling is handled through custom ref.dll detours that intercept vertex calls during font rendering. Different font sizes (title, small, medium, interface) are detected and scaled appropriately.

### Font Caller Detection
The `R_DrawFont` hook identifies which function is calling font rendering to apply context-specific scaling:
- Menu items
- HUD text
- In-game messages
- Scoreboard text

### Character Index Tracking
The feature tracks character position within strings to apply proper spacing when scaling, ensuring text remains readable and properly aligned.

