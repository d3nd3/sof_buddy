# Raw Mouse Input

## Purpose
Implements raw mouse input support for Soldier of Fortune using Windows Raw Input API instead of the default GetCursorPos/SetCursorPos cursor management. Provides smoother, more direct mouse response without Windows acceleration/smoothing while preserving all original SoF mouse cvars.

## Callbacks
- **EarlyStartup** (Post, Priority: 70)
  - `raw_mouse_EarlyStartup()` - Patches SetCursorPos calls in exe to install hooks
- **RefDllLoaded** (Post, Priority: 70)
  - `raw_mouse_RefDllLoaded()` - Registers raw input device with Windows API

## Hooks
- **IN_MouseMove** (Post, Priority: 100)
  - `in_mousemove_callback()` - Resets delta accumulators after each frame
- **IN_MenuMouse** (Post, Priority: 100)
  - `in_menumouse_callback()` - Consumes accumulated deltas after menu mouse handling (same as IN_MouseMove)

## OverrideHooks
- **GetCursorPos** (user32.dll)
  - `getcursorpos_override_callback()` - Returns virtual cursor position `(window_center + raw_mouse_delta_x/y)` when raw input enabled; never calls real GetCursorPos for look/menu
- **DispatchMessageA** (user32.dll)
  - `dispatchmessagea_override_callback()` - On WM_INPUT, extracts relative raw deltas (ignores absolute) and accumulates via `raw_mouse_accumulate_delta()`

## CustomDetours
- **SetCursorPos** (user32.dll, via GetProcAddress)
  - `hkSetCursorPos()` - When raw input enabled: updates internal `window_center`, refreshes cursor clip, returns TRUE without calling the real SetCursorPos (OS cursor is never warped)
  - Patched at exe addresses: `0x2004A0B2`, `0x2004A410`, `0x2004A579`

## Technical Details

### How It Works
The implementation works by **faking cursor position changes** so the original mouse processing code continues to work with all its cvars intact:

1. **Registration** (RefDllLoaded callback):
   - Registers for raw mouse input using `RegisterRawInputDevices()`
   - Sets `hwndTarget` to the active window handle

2. **Message Processing** (DispatchMessageA hook):
   - Intercepts WM_INPUT messages
   - Calls `ProcessRawInput()` to extract mouse delta from RAWINPUT structure
   - Ignores absolute mouse packets and accumulates only relative raw deltas

3. **Virtual Cursor Position** (GetCursorPos hook):
   - Returns `(window_center.x + raw_mouse_delta_x, window_center.y + raw_mouse_delta_y)` so the game sees movement (e.g. swipe left → negative delta_x → position left of center)
   - Game logic unchanged: it reads “cursor” position and uses it for look/menu; we never call the real GetCursorPos for that path

4. **No Cursor Warping** (SetCursorPos hook):
   - Game’s recenter calls are intercepted: we update `window_center` and refresh cursor clip only, then return TRUE without calling the real SetCursorPos, so the OS cursor is never moved by the game

5. **Cursor Confinement** (ClipCursor):
   - While raw input is enabled and the game window is foregrounded, cursor is clipped to the game client area inset by 4px on left and right (smaller clip area reduces accidental escape on focus regain)
   - On each clip refresh (e.g. after alt-tab), if the OS cursor is outside the clip rect it is warped back inside via the real SetCursorPos so the cursor is always within bounds when regaining focus
   - Clip is automatically released when raw input is disabled or focus is lost

6. **Delta Consumption** (IN_MouseMove / IN_MenuMouse callbacks):
   - After the game processes the frame, `raw_mouse_consume_deltas()` resets `raw_mouse_delta_x/y` to 0, so the virtual cursor is effectively back at window_center for the next frame

### Preserved CVars
All original SoF mouse cvars remain functional:
- `sensitivity` - Mouse sensitivity multiplier
- `m_filter` - Mouse smoothing/averaging
- `m_yaw` - Yaw (horizontal) sensitivity
- `m_pitch` - Pitch (vertical) sensitivity
- `m_side` - Strafe sensitivity
- `m_forward` - Forward/backward sensitivity
- `freelook` - Free look mode toggle
- `lookstrafe` - Look strafe mode

## Configuration
- **_sofbuddy_rawmouse** (default: 0, archived)
  - Set to 1 to enable raw mouse input
  - Set to 0 to use legacy cursor-based input

## Benefits
- Smoother, more direct mouse response (no Windows acceleration/smoothing)
- Better support for high DPI mice
- Elimination of cursor warping issues
- Prevents accidental focus loss from cursor hitting desktop/taskbar edges
- Preserves all original SoF mouse cvars
