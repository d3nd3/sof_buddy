# Scaled UI Base

## Purpose
Provides shared infrastructure and base hooks for all scaled UI features (scaled_con, scaled_hud, scaled_menu). This feature contains common functionality including shared drawing hooks, vertex manipulation, text scaling, and global state management.

**Note:** This feature automatically compiles when any of the sub-features (scaled_con, scaled_hud, scaled_menu) are enabled, ensuring the base infrastructure is always available. Text scaling functionality is automatically enabled when `scaled_hud` or `scaled_menu` are enabled.

## Callbacks
- **RefDllLoaded** (Post, Priority: 50)
  - `scaledUIBase_RefDllLoaded()` - Installs custom detours for ref.dll functions (glVertex2f)

## Hooks
- **GL_FindImage** (Post, Priority: 100)
  - `gl_findimage_post_callback()` - Extracts image dimensions from GL_FindImage return value for menu scaling

## OverrideHooks
- **Draw_StretchPic** (SoF.exe)
  - `hkDraw_StretchPic()` - Shared hook for console background and HUD CTF flag scaling
- **Draw_Pic** (SoF.exe)
  - `hkDraw_Pic()` - Shared hook for menu background tiling and pic caller detection
- **R_DrawFont** (SoF.exe)
  - `hkR_DrawFont()` - Scales font rendering and tracks font caller context (enabled with scaled_hud or scaled_menu)
- **glVertex2f** (ref.dll, custom detour)
  - `hkglVertex2f()` - Central vertex scaling hook used by all scaled UI features

## CustomDetours
- **glVertex2f** (ref.dll)
  - Custom detour installed at runtime in `scaledUIBase_RefDllLoaded()`
  - Address: `ref.dll + 0x000A4670`
  - Handles vertex coordinate scaling for console, HUD, text, and menu elements
- **DrawFont vertex functions** (ref.dll, enabled with scaled_hud or scaled_menu)
  - `my_glVertex2f_DrawFont_1/2/3/4()` - Custom vertex scaling for font rendering
  - Installed at runtime in `scaledUIBase_RefDllLoaded()`
  - Addresses: `ref.dll + 0x00004860, 0x00004892, 0x000048D2, 0x00004903`

## Shared Infrastructure

### Global Variables
- `g_activeDrawCall` - Tracks current drawing routine type (StretchPic, Pic, PicOptions, etc.)
- `g_currentPicCaller` - Identifies which function called Draw_Pic
- `g_currentStretchPicCaller` - Identifies which function called Draw_StretchPic
- `DrawPicWidth`, `DrawPicHeight` - Image dimensions extracted from GL_FindImage
- `mainMenuBgTiled` - Flag for menu background tiling mode
- `screen_y_scale`, `current_vid_w`, `current_vid_h` - Screen dimensions and scaling factors

### Shared Headers
- `shared.h` - Main shared header with all global variables, enums, and function declarations
- `caller_from.h` - Caller detection utilities for identifying function call sites

### CVars
Defined in `sui_cvars.cpp` (only compiled when scaled_con or scaled_hud are enabled):
- `_sofbuddy_font_scale` - Font scaling factor
- `_sofbuddy_console_size` - Console size as fraction of screen
- `_sofbuddy_hud_scale` - HUD element scaling factor
- `_sofbuddy_crossh_scale` - Crosshair scaling factor

## Architecture

### Feature Guards
All base implementation files use:
```cpp
#if FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE
```

Text scaling code uses:
```cpp
#if FEATURE_SCALED_HUD || FEATURE_SCALED_MENU
```

This ensures base code compiles when any sub-feature is enabled, and text scaling is available when HUD or menu scaling is enabled.

### Hook Organization
- Hooks are in `hooks/` subdirectory, one file per hook
- Each hook has its own `.cpp` file named after the function (e.g., `draw_pic.cpp`)
- Hook declarations are in `hooks/hooks.json`

### Callback Organization
- Callbacks are in `callbacks/` subdirectory
- Each callback has its own `.cpp` file
- Callback declarations are in `callbacks/callbacks.json`

## Dependencies
- All scaled UI sub-features depend on this base feature
- Provides shared state and hooks that sub-features use
- No external dependencies beyond core SoF Buddy infrastructure

