# UI Caller Responsibilities

This document maps each UI caller to the game component it is responsible for rendering. The scaled UI system uses caller detection to identify which part of the game UI is being drawn, enabling context-aware scaling.

## Overview

The caller detection system works by:
1. Intercepting drawing function calls (e.g., `Draw_Pic`, `Draw_StretchPic`, `R_DrawFont`)
2. Using stack unwinding to identify the function that called the drawing function
3. Mapping the caller's RVA (Relative Virtual Address) to a semantic enum
4. Applying appropriate scaling based on the identified caller

See `docs/HARMONY.md` for technical details on the caller detection mechanism.

## Function Call Hierarchies

Understanding the call stack is crucial for understanding how UI rendering works and where hooks intercept calls. Here are important function call hierarchies:

### SCR_UpdateScreen Call Stack

This is a critical call stack showing how screen text rendering flows through the system:

```
SCR_UpdateScreen
	SCR_DrawCenterPrint
		R_DrawFont
			GLTexCoord2f
			GLVertex2f
	SCR_CheckDrawCinematicString(void)
		SCR_DrawCinematicString(int,int,int)
			Draw_CharExtra(float, float, float, paletteRGBA_c &, int)
				GLTexCoord2f
				GLVertex2f
```

**Key Points:**
- `SCR_DrawCenterPrint` is a `FontCaller` (RVA: `0x000163C0`) that triggers screen-scale text rendering
- It's called from `SCR_UpdateScreen` and uses `R_DrawFont` for text rendering
- `R_DrawFont` eventually calls `GLVertex2f` which is hooked for vertex scaling
- `SCR_CheckDrawCinematicString` provides an alternate path through `Draw_CharExtra` which also calls `GLVertex2f`
- Both paths ultimately reach `GLVertex2f`, allowing the hook to apply appropriate scaling based on the detected caller

**Related Files:**
- `hooks/r_drawfont.cpp` - Intercepts `R_DrawFont` calls from `SCR_DrawCenterPrint`
- `hooks/glvertex2f.cpp` - Intercepts `GLVertex2f` calls for vertex scaling
- `text_scaling.cpp` - Applies screen scale to `FontCaller::SCR_DrawCenterPrint` (line 56-60)

## Caller Types

### StretchPicCaller

Callers of `Draw_StretchPic` - used for stretched/scaled images:

| Caller | UI Component | RVA (SoF.exe) | Notes |
|--------|--------------|---------------|-------|
| `CON_DrawConsole` | Console background | `0x00020F90` | Console rendering |
| `CtfFlagDraw` | CTF flag display | - | Capture the flag indicator |
| `ControlFlagDraw` | Control point flag | - | Control point indicator |
| `ScopeDraw` | Sniper scope overlay | - | Scope reticle |
| `ExecuteLayoutString` | Menu/scoreboard elements | - | Layout-based UI |
| `VbarDraw` | Vertical bar elements | - | Progress/status bars |
| `DrawStretchPic` | Generic stretched pic | `0x000C8960` | Fallback caller |
| `loadbox_c_GetIndices` | Load box menu | `0x000DBF10` | Loading screen elements |
| `Draw_Line` | Line drawing | `0x00002A40` (ref.dll) | Line rendering |
| `ScopeCalcXY` | Scope calculations | `0x00006DC0` | Scope positioning |

**Related Files:**
- `caller_from.h` - Enum definition
- `caller_from.cpp` - RVA to caller mapping (lines 13-43)
- `hooks/draw_stretchpic.cpp` - Hook implementation and `CON_DrawConsole` handling
- `hooks/glvertex2f.cpp` - Vertex scaling for StretchPic callers
- `src/features/scaled_hud/hooks/cCtfFlag_Draw.cpp` - CTF flag render type setup
- `src/features/scaled_con/` - Console scaling implementation

### PicCaller

Callers of `Draw_Pic` - used for fixed-size images:

| Caller | UI Component | RVA (SoF.exe) | Notes |
|--------|--------------|---------------|-------|
| `ExecuteLayoutString` | Menu/scoreboard images | `0x00014510` | Layout-based UI images |
| `SCR_DrawCrosshair` | Crosshair | `0x00015DA0` | Weapon crosshair |
| `NetworkDisconnectIcon` | Network status icon | `0x000165A0` | Connection indicator |
| `BackdropDraw` | Menu backdrop | - | Menu background |
| `DrawPicWrapper` | Generic pic wrapper | `0x000C8A40` | Fallback caller |

**Related Files:**
- `caller_from.h` - Enum definition
- `caller_from.cpp` - RVA to caller mapping (lines 45-72)
- `hooks/draw_pic.cpp` - Hook implementation and menu backdrop detection
- `hooks/gl_findimage.cpp` - Image dimension extraction for menu scaling
- `hooks/glvertex2f.cpp` - Vertex scaling for Pic callers (lines 105-133)
- `src/features/scaled_hud/hooks/SCR_ExecuteLayoutString.cpp` - Scoreboard scaling
- `src/features/scaled_menu/` - Menu scaling implementation

### PicOptionsCaller

Callers of `Draw_PicOptions` - used for scaled images with options:

| Caller | UI Component | RVA | Module | Notes |
|--------|--------------|-----|--------|-------|
| `cInterface_DrawNum` | Interface numbers | `0x000065E0` | SoF.exe | Numeric displays |
| `cDMRanking_CalcXY` | Deathmatch ranking | `0x00007AF0` | SoF.exe | DM scoreboard elements |
| `SCR_DrawCrosshair` | Crosshair (alternate) | `0x00015DA0` | SoF.exe | Crosshair variant |
| `spcl_DrawFPS` | FPS counter | `0x00003420` | spcl.dll | FPS display |

**Related Files:**
- `caller_from.h` - Enum definition
- `caller_from.cpp` - RVA to caller mapping (lines 74-103)
- `src/features/scaled_hud/hooks/draw_picoptions.cpp` - Hook implementation with DM ranking and inventory scaling
- `hooks/glvertex2f.cpp` - Vertex scaling for PicOptions callers (lines 96-104)
- `src/features/scaled_hud/hooks/cDMRanking_Draw.cpp` - DM ranking render type setup

### CroppedPicCaller

Callers of `Draw_CroppedPic` - used for cropped image regions:

| Caller | UI Component | RVA (SoF.exe) | Notes |
|--------|--------------|---------------|-------|
| `cInventory2_Constructor` | Inventory display | `0x00008260` | Health, armor, ammo, inventory items |

**Related Files:**
- `caller_from.h` - Enum definition
- `caller_from.cpp` - RVA to caller mapping (lines 105-129)
- `src/features/scaled_hud/hooks/draw_croppedpicoptions.cpp` - Hook implementation
- `src/features/scaled_hud/hooks/cInventory2_And_cGunAmmo2_Draw.cpp` - Inventory/ammo render type setup
- `src/features/scaled_hud/hooks/cHealthArmor2_Draw.cpp` - Health/armor render type setup
- `src/features/scaled_hud/callbacks/scaledHud_RefDllLoaded.cpp` - Custom vertex detours for cropped pics

### FontCaller

Callers of `R_DrawFont` - used for text rendering:

| Caller | UI Component | RVA | Module | Scaling | Notes |
|--------|--------------|-----|--------|---------|-------|
| `ScopeCalcXY` | Scope text | `0x00006DC0` | SoF.exe | None | Scope overlay text |
| `DMRankingCalcXY` | Deathmatch ranking text | `0x00007AF0` | SoF.exe | HUD scale | DM scoreboard text |
| `Inventory2` | Inventory text | `0x00008260` | SoF.exe | HUD scale | Item names, ammo counts |
| `SCRDrawPause` | Pause screen text | `0x00013710` | SoF.exe | Screen scale | Pause menu |
| `SCR_DrawCenterPrint` | Center print text | `0x000163C0` | SoF.exe | Screen scale | Center screen text (called from SCR_UpdateScreen) |
| `RectDrawTextItem` | Menu item text | `0x000CECC0` | SoF.exe | Screen scale | Menu items |
| `RectDrawTextLine` | Menu line text | `0x000CF0E0` | SoF.exe | Screen scale | Menu text lines |
| `TickerDraw` | Ticker text | `0x000D19F0` | SoF.exe | Screen scale | Scrolling text |
| `InputHandle` | Input field text | `0x000D4060` | SoF.exe | Screen scale | Text input fields |
| `FileboxHandle` | File box text | `0x000D9F80` | SoF.exe | Screen scale | File browser |
| `LoadboxGetIndices` | Load box text | `0x000DBF10` | SoF.exe | Screen scale | Loading screen text |
| `ServerboxDraw` | Server browser text | `0x000DE430` | SoF.exe | Screen scale | Server list |
| `TipRender` | Tip text | `0x000EA8A0` | SoF.exe | Screen scale | Help/tip messages |
| `DrawLine` | Line text | `0x00002A40` | ref.dll | None | Text on lines |

**Related Files:**
- `caller_from.h` - Enum definition
- `caller_from.cpp` - RVA to caller mapping (lines 195-234)
- `hooks/r_drawfont.cpp` - Hook implementation and caller detection
- `text_scaling.cpp` - Font vertex scaling logic (lines 33-89) - shows scaling behavior for each caller
- `sui_hooks.cpp` - Custom font vertex detours setup
- `src/features/scaled_hud/` - HUD text scaling (DMRankingCalcXY, Inventory2)
- `src/features/scaled_menu/` - Menu text scaling (RectDrawTextItem, RectDrawTextLine, etc.)

### VertexCaller

Callers of `glVertex2f` - used for vertex coordinate scaling:

| Caller | UI Component | RVA (ref.dll) | Notes |
|--------|--------------|---------------|-------|
| `Draw_Char` | Character rendering | `0x00001850` | Individual character vertices |
| `Draw_StretchPic` | Stretched image vertices | `0x00001D10` | Image quad vertices |
| `Draw_Line` | Line vertices | `0x00002A40` | Line segment vertices |

**Related Files:**
- `caller_from.h` - Enum definition
- `caller_from.cpp` - RVA to caller mapping (lines 171-193)
- `hooks/glvertex2f.cpp` - Main vertex scaling hook implementation
- `sui_hooks.cpp` - Custom glVertex2f detour setup
- `callbacks/scaledUIBase_RefDllLoaded.cpp` - Runtime detour installation

## Scaling Behavior by Caller

### HUD Scale (`hudScale`)
Applied to:
- `FontCaller::DMRankingCalcXY` - Deathmatch ranking text
- `FontCaller::Inventory2` - Inventory text
- `PicOptionsCaller::cDMRanking_CalcXY` - DM ranking images
- `PicOptionsCaller::cInterface_DrawNum` - Interface numbers
- `StretchPicCaller::CtfFlagDraw` - CTF flag (when `g_activeRenderType == HudCtfFlag`)

**Related Files:**
- `text_scaling.cpp` (lines 43-49) - Font scaling with HUD scale
- `src/features/scaled_hud/hooks/draw_picoptions.cpp` (lines 24-61) - PicOptions scaling
- `hooks/draw_stretchpic.cpp` (lines 60-64) - CTF flag scaling
- `src/features/scaled_hud/sui_cvars.cpp` - CVar definition

### Screen Scale (`screen_y_scale`)
Applied to:
- `FontCaller::SCRDrawPause` - Pause screen text
- `FontCaller::SCR_DrawCenterPrint` - Center print text (called from SCR_UpdateScreen)
- `FontCaller::RectDrawTextItem` - Menu item text
- `FontCaller::RectDrawTextLine` - Menu line text
- `FontCaller::TickerDraw` - Ticker text
- `FontCaller::InputHandle` - Input field text
- `FontCaller::FileboxHandle` - File box text
- `FontCaller::LoadboxGetIndices` - Load box text
- `FontCaller::ServerboxDraw` - Server browser text
- `FontCaller::TipRender` - Tip text

**Related Files:**
- `text_scaling.cpp` (lines 55-61, 68-80) - Font scaling with screen scale
- `text_scaling.cpp` (lines 111-137) - Text measurement scaling (R_Strlen, R_StrHeight)
- `src/features/scaled_menu/` - Menu scaling implementation

### Console Scale (`consoleSize`)
Applied to:
- `StretchPicCaller::CON_DrawConsole` - Console background

**Related Files:**
- `hooks/draw_stretchpic.cpp` (lines 36-50) - Console background scaling
- `src/features/scaled_con/` - Console scaling implementation
- `src/features/scaled_con/sui_cvars.cpp` - CVar definition

### No Scaling
These callers render at original size:
- `FontCaller::ScopeCalcXY` - Scope text (must remain pixel-perfect)
- `FontCaller::DrawLine` - Line text
- `VertexCaller::*` - Vertex callers (handled separately)

**Related Files:**
- `text_scaling.cpp` (lines 51-53, 63-65) - No scaling cases

## Usage Examples

### Detecting Console Rendering
```cpp
if (caller == StretchPicCaller::CON_DrawConsole) {
    // Scale console background
    int consoleHeight = draw_con_frac * current_vid_h;
    // ... console rendering logic
}
```

### Detecting HUD Elements
```cpp
switch (g_currentFontCaller) {
    case FontCaller::DMRankingCalcXY:
    case FontCaller::Inventory2:
        // Apply HUD scaling
        x = pivotx + (x - pivotx) * hudScale;
        y = pivoty + (y - pivoty) * hudScale;
        break;
}
```

### Detecting Menu Elements
```cpp
switch (g_currentFontCaller) {
    case FontCaller::RectDrawTextItem:
    case FontCaller::RectDrawTextLine:
    case FontCaller::InputHandle:
        // Apply screen scaling for menus
        x = pivotx + (x - pivotx) * screen_y_scale;
        y = pivoty + (y - pivoty) * screen_y_scale;
        break;
}
```

## Implementation Files

### Core Caller Detection
- **`caller_from.h`** - All caller enum definitions
- **`caller_from.cpp`** - RVA to caller mapping functions for all caller types
- **`shared.h`** - Global state variables (`g_currentPicCaller`, `g_currentStretchPicCaller`, etc.)

### Hook Implementations
- **`hooks/draw_stretchpic.cpp`** - StretchPic hook with console and CTF flag handling
- **`hooks/draw_pic.cpp`** - Pic hook with menu backdrop detection
- **`hooks/draw_picoptions.cpp`** - PicOptions hook (in `scaled_hud/`)
- **`hooks/draw_croppedpicoptions.cpp`** - CroppedPic hook (in `scaled_hud/`)
- **`hooks/r_drawfont.cpp`** - Font rendering hook with caller detection
- **`hooks/glvertex2f.cpp`** - Main vertex scaling hook
- **`hooks/gl_findimage.cpp`** - Image dimension extraction

### Scaling Logic
- **`text_scaling.cpp`** - Font vertex scaling with caller-specific behavior
- **`sui_hooks.cpp`** - Custom detour setup and shared hook utilities

### Feature-Specific
- **`src/features/scaled_hud/`** - HUD component hooks and scaling
- **`src/features/scaled_menu/`** - Menu scaling implementation
- **`src/features/scaled_con/`** - Console scaling implementation

## Related Documentation

- `README.md` - Scaled UI Base overview
- `docs/HARMONY.md` - Caller detection system architecture
- `src/features/scaled_hud/README.md` - HUD component details

