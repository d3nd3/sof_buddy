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
  - `in_menumouse_callback()` - Post-processing for menu mouse handling

## OverrideHooks
- **GetCursorPos** (user32.dll)
  - `getcursorpos_override_callback()` - Returns fake cursor position (center_point + raw_deltas) when raw input enabled
- **DispatchMessageA** (user32.dll)
  - `dispatchmessagea_override_callback()` - Intercepts WM_INPUT messages to extract raw mouse hardware deltas

## CustomDetours
- **SetCursorPos** (user32.dll, via GetProcAddress)
  - `hkSetCursorPos()` - No-op when raw input enabled to prevent cursor recentering
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

3. **Cursor Position Emulation** (GetCursorPos hook):
   - Returns `center_point + accumulated_deltas` to simulate cursor movement
   - The game calculates: `mx = cursor_pos.x - window_center_x` which yields the raw delta
   - **This preserves the entire original mouse processing pipeline**

4. **Cursor Warping Prevention** (SetCursorPos hook):
   - Updates internal center tracking only
   - Returns TRUE immediately without physically warping the OS cursor

5. **Cursor Confinement** (ClipCursor):
   - While raw input is enabled and the game window is foregrounded, cursor is clipped to the game client area
   - Prevents pointer escape to screen/taskbar edges during fast movement
   - Clip is automatically released when raw input is disabled or focus is lost

6. **Delta Consumption** (IN_MouseMove hook):
   - Resets accumulated deltas to 0 after each `IN_MouseMove` and `IN_MenuMouse`
   - Prevents delta accumulation between frames

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
