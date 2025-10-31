# Add Crosshair Scaling to scaled_ui Feature

## Overview
Add a new cvar `_sofbuddy_crossh_scale` to control crosshair texture scaling. Apply scaling directly in `GL_FindImage` hook by modifying the image_t structure width/height. Crosshair textures are `pics/menus/status/ch0.m32` through `ch4.m32` (ch0 is transparent/disabled).

## Implementation Steps

### 1. Add Cvar Registration (`src/features/scaled_ui/cvars.cpp`)
- Declare `cvar_t * _sofbuddy_crossh_scale`
- Add to `create_scaled_ui_cvars()` with default "1"
- Add forward declaration for change callback

### 2. Add Global Variables (`src/features/scaled_ui/hooks.cpp`)
- Add `float crosshairScale = 1.0f` variable
- Add change callback `crosshairscale_change(cvar_t * cvar)` similar to `hudscale_change`

### 3. Update Header (`src/features/scaled_ui/scaled_ui.h`)
- Add extern declaration for `crosshairScale`
- Add extern declaration for `crosshairscale_change`

### 4. Detect and Scale Crosshair Textures in GL_FindImage (`src/features/scaled_ui/hooks.cpp`)
- In `hkGL_FindImage`, efficiently check for crosshair pattern: `pics/menus/status/ch` followed by digit 0-4 and `.m32`
- After calling `oGL_FindImage` and getting `image_t`, if it matches crosshair pattern:
  - Apply `crosshairScale` by modifying `*(short*)((char*)image_t + 0x44)` (width) and `*(short*)((char*)image_t + 0x46)` (height)
- This ensures `Draw_GetPicSize` gets the scaled size, and `hkglVertex2f` (with `isDrawPicCenter=true`) uses the correct scaled dimensions

### 5. Update External Declarations (`hdr/features.h`)
- Add extern declaration for `_sofbuddy_crossh_scale` cvar

## Key Files to Modify
- `src/features/scaled_ui/cvars.cpp` - Add cvar registration
- `src/features/scaled_ui/hooks.cpp` - Add variable, change callback, and GL_FindImage detection/scaling
- `src/features/scaled_ui/scaled_ui.h` - Add declarations
- `hdr/features.h` - Add extern cvar declaration

