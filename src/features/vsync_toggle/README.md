# VSync Toggle Fix

## Purpose
Ensures that the `gl_swapinterval` cvar (vsync control) is properly applied when the video renderer changes, fixing vsync settings not taking effect after renderer reinitialization.

## Callbacks
- **RefDllLoaded** (Post, Priority: 50)
  - `vsync_on_postcvarinit()` - Registers vsync CVars

## Hooks
- **VID_CheckChanges** (Pre, Priority: 50)
  - `vsync_pre_vid_checkchanges()` - Triggers `gl_swapinterval->modified` flag when renderer changes

## OverrideHooks
None

## CustomDetours
None

## Technical Details

### Problem
When `vid_ref` cvar changes (switching video renderer), `gl_swapinterval` (vsync) settings are not reapplied. This bug is only noticeable when:
1. User changes `gl_swapinterval` AND the change is processed by `R_BeginFrame`
2. Then sometime in the future a `vid_restart` or `gl_mode` change is performed

### Root Cause
The renderer initialization doesn't check `gl_swapinterval->modified` flag during `vid_ref` changes. The order of operations is:
```
Cbuf_Execute(ReadPacketsOrSendCommand) (gl_swapinterval->modified=true)
  → VID_CheckChanges(CL_Frame) (Has vid_ref->modified == true)
  → SCR_UpdateScreen(GL_UpdateSwapInterval) (Apply swap, gl_swapinterval->modified=false)
```

Because `VID_CheckChanges` is between `Cbuf_Execute` and `SCR_UpdateScreen`, if the `vid_ref->modified` is applied in the same frame, it works. But if renderer changes later, vsync isn't reapplied.

### Solution
Hook `VID_CheckChanges()` to trigger `gl_swapinterval->modified` when renderer changes. This ensures that `R_Init() -> GL_SetDefaultState()` will reapply vsync settings.

### Implementation Flow
```
Game calls VID_CheckChanges()
  → vsync_pre_vid_checkchanges() intercepts (Pre hook)
  → Checks if vid_ref->modified is true
  → If renderer is changing, sets gl_swapinterval->modified = true
  → Calls original VID_CheckChanges()
  → Result: Vsync settings are properly applied during R_Init() -> GL_SetDefaultState()
```

## Configuration
- **gl_swapinterval** (game cvar)
  - Set to 0 to disable vsync
  - Set to 1 to enable vsync
  - Automatically reapplied on renderer changes

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
