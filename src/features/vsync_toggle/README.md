# VSync Toggle Fix

## Overview
Ensures that the `gl_swapinterval` cvar (vsync control) is properly applied when the video renderer changes, fixing vsync settings not taking effect after renderer reinitialization.  
This bug is only noticeable when the user changes gl_swapinterval _AND_ the change is processed by R_BeginFrame, then sometime in the future a vid_restart or gl_mode change is performed.

## Problems Solved

### gl_swapinterval Not Applied on Renderer Change
- **Problem**: When `vid_ref` cvar changes (switching video renderer), `gl_swapinterval` (vsync) settings are not reapplied
- **Root Cause**: The renderer initialization doesn't check `gl_swapinterval->modified` flag during `vid_ref` changes
- **Solution**: Hook `VID_CheckChanges()` to trigger `gl_swapinterval->modified` when renderer changes

## Technical Details

### Hook Implementation
```cpp
// Hook the video mode change checker
REGISTER_HOOK_VOID(VID_CheckChanges, 0x20039030, void, __cdecl);
```

### Memory Layout

#### SoF.exe Memory
- `VID_CheckChanges()`: `0x20039030` - Function that checks for video mode changes
- `vid_ref cvar pointer`: `0x201C1F30` - Pointer to the vid_ref cvar
- `gl_swapinterval cvar pointer`: `0x20403634` - Pointer to the gl_swapinterval cvar
- `viddef.width`: `0x2040365C` - Current video width
- `viddef.height`: `0x20403660` - Current video height

### Implementation Flow

```
Game calls VID_CheckChanges()
  → hkVID_CheckChanges() intercepts
  → Checks if vid_ref->modified is true
  → If renderer is changing, sets gl_swapinterval->modified = true
  → Calls oVID_CheckChanges() (original function)
  → Tracks current video dimensions
  → Result: Vsync settings are properly applied during R_Init() -> GL_SetDefaultState()
```

### Why This Works
When the renderer initializes via `R_Init()`, it calls `GL_SetDefaultState()` which checks the `modified` flag of various cvars including `gl_swapinterval`. By ensuring this flag is set when the renderer changes, we guarantee that vsync settings are reapplied.

## Configuration
No CVars required - the fix is automatically applied.

## Benefits
- **Vsync works reliably** - Settings are properly applied after renderer changes
- **User-friendly** - Players can change vsync without worrying about renderer state
- **Lightweight** - Minimal performance overhead, only triggers on renderer changes

## Usage Example
```
// Player switches renderer
set vid_ref gl
// → vsync_toggle ensures gl_swapinterval is reapplied

// Player changes vsync setting
set gl_swapinterval 0  // Disable vsync
set gl_swapinterval 1  // Enable vsync
// → Settings take effect immediately on next renderer init
```

## Development Notes
- Uses direct cvar pointer access at fixed memory addresses
- Minimal invasiveness - only affects the renderer change flow
- Tracks video dimensions as a side benefit for other potential features
- Part of graphics quality-of-life improvements
