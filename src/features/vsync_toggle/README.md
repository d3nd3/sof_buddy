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

## Display refresh readout (F12 → CPU)

Besides the toggle fix, this feature measures the real display refresh rate and compares it
against the framerate cap:

- `gl_displayrefresh` (engine cvar, `R_Register` default `"0"`, archived) requests a refresh
  rate from the renderer at `R_SetMode` (`0` = auto, driver decides). Resolved here with the
  identical spec, so resolving early is harmless.
- `sofbuddy_refresh_display_info()` reads the system truth via `EnumDisplaySettings`
  (`dmDisplayFrequency`, `GetDeviceCaps(VREFRESH)` fallback) and recomputes two internal
  cvars, refreshed on every CPU-page open (and once at `RefDllLoaded`):
  - `_sb_internal_display_info` — e.g. `240 Hz`, always shown (system truth only), in the
    theme-accent (`sb_accent`) readout tint with the `Actual Display Refresh Rate` label, below the picker.
  - `_sb_internal_fps_cap_mismatch` — `"1"`/`"0"` flag driving RMF `<cinclude>` so the orange
    warning block (`cpu_capwarn.rmf`) appears only when VSync is on and the display could
    deliver more than the cap allows. `cl_maxfps` is resolved locally (same engine default)
    so this needs no coupling to `media_timers`; the name is prefixed to avoid colliding
    with its global of the same name.

  - `_sb_internal_display_modes_rmf` — generated `<list>` row for the request, rebuilt on
    every CPU-page open from the modes the driver actually reports for the current
    resolution (`Auto`/`0` first, then ascending; capped so quoted strings stay far under
    the ~240-byte menu parse limit). Inserted via `<includecvar>`, same pattern as the
    updater's release list. Selecting a rate writes `gl_displayrefresh` and runs
    `vid_restart`; `Auto` lets the driver decide.

## Console command

- `sofbuddy_display_refresh` — recompute the display readout + mismatch flag on demand.
  Wired into the Framerate/VSync rows as `key mouse1 "sofbuddy_display_refresh;refresh"`
  (same shape as the Profile row's `key mouse1 "refresh"`): native cycling applies the new
  value first, then this recomputes, then `refresh` re-parses — so the conditional warning
  appears or clears on the same click instead of lagging a page behind.

- `vid_fullscreen` (engine cvar, `VID_Init` default `"1"`, archived) gets a static
  `Windowed`/`Fullscreen` row (`0` first, safe fallback).
- `_sp_cl_vid_border` (sofplus cvar, `0` = No, `1` = Yes default) gets a `No`/`Yes` row that is
  generated only when the cvar exists *and* `vid_fullscreen` is `0`: the builder checks with
  `findCvar` (read-only walk, never creates), so vanilla installs never see a dead row and
  fullscreen never shows meaningless chrome. Bible `<cinclude>` was rejected for this — it
  tests the cvar *value*, so a deliberate `0` would wrongly hide the row. The Fullscreen row
  itself carries `sofbuddy_display_refresh;refresh` so toggling it shows/hides Border on the
  same click.
