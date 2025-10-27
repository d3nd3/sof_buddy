# Raw Mouse Input

Implements raw mouse input support for Soldier of Fortune using Windows Raw Input API instead of the default GetCursorPos/SetCursorPos cursor management.

## What it does

This feature provides raw mouse input by intercepting the Windows Raw Input API messages and feeding raw hardware deltas into the existing mouse processing pipeline. **All original sensitivity and filtering cvars remain functional.**

Benefits:
- Smoother, more direct mouse response (no Windows acceleration/smoothing)
- Better support for high DPI mice
- Elimination of cursor warping issues
- Preserves all original SoF mouse cvars: `sensitivity`, `m_filter`, `m_yaw`, `m_pitch`, `m_side`, `m_forward`

## Technical Details

### Hooks

The feature hooks the following functions in SoF.exe:

1. **IN_MouseMove** (Address: `0x2004A1F0`)
   - Main mouse movement handler in SoF.exe
   - Uses `GetCursorPos` to get mouse position and `SetCursorPos` to center the cursor
   - **We hook this only to reset delta accumulators after each frame**
   - The original function still performs all sensitivity/filtering calculations

2. **GetCursorPos** (user32.dll export - resolved via `GetProcAddress`)
   - **Key Hook:** Returns fake cursor position = `center_point + raw_deltas`
   - The game calculates `mx = fake_cursor - window_center` which yields the raw delta
   - This preserves all original mouse processing (sensitivity, filtering, etc.)

3. **SetCursorPos** (user32.dll export - resolved via `GetProcAddress`)
   - Blocked (no-op) to prevent cursor recentering
   - The game normally calls `SetCursorPos(window_center_x, window_center_y)` after each frame
   - We prevent this to maintain our fake cursor position for delta accumulation

4. **DispatchMessage** (user32.dll export - resolved via `GetProcAddress`)
   - Intercepts `WM_INPUT` messages to extract raw mouse hardware deltas
   - Accumulates deltas in `raw_mouse_delta_x/y` for consumption by `GetCursorPos`

### Configuration

- **_sofbuddy_rawmouse** (default: 0, archived)
  - Set to 1 to enable raw mouse input
  - Set to 0 to use legacy cursor-based input

## Implementation Status

### Completed ✅
- Windows Raw Input API registration in `RefDllLoaded` callback
- RAWINPUTDEVICE structure setup with correct HID constants:
  - `HID_USAGE_PAGE_GENERIC` (0x01) - Generic desktop controls
  - `HID_USAGE_GENERIC_MOUSE` (0x02) - Mouse
  - `RIDEV_INPUTSINK` flag for background input capture
- WM_INPUT message processing via `DispatchMessage` hook
- Mouse delta accumulation in `ProcessRawInput()` function
- Modified `GetCursorPos` to return accumulated cursor position when raw input is enabled
- Modified `SetCursorPos` to be a no-op when raw input is enabled (no cursor warping)
- Raw mouse delta tracking with automatic reset after reading

**Verified Against Microsoft Documentation:** Implementation uses correct constants and API calls as per MSDN examples

### Address Status ✅
All required addresses are configured:

- ✅ **IN_MouseMove** - Address `0x2004A1F0` (provided by user)
- ✅ **GetCursorPos** - Uses `GetProcAddress` on user32.dll
- ✅ **SetCursorPos** - Uses `GetProcAddress` on user32.dll  
- ✅ **DispatchMessage** - Uses `GetProcAddress` on user32.dll

**Ready for testing!**

### Implementation Details

The raw input implementation works by **faking cursor position changes** so the original mouse processing code continues to work with all its cvars intact:

1. **Registration** (`RefDllLoaded` callback):
   - Registers for raw mouse input using `RegisterRawInputDevices()`
   - Sets `hwndTarget` to the active window handle
   - Uses `RIDEV_INPUTSINK` flag to receive input even when window is unfocused

2. **Message Processing** (`DispatchMessage` hook):
   - Intercepts WM_INPUT messages
   - Calls `ProcessRawInput()` to extract mouse delta from RAWINPUT structure
   - Accumulates delta in `raw_mouse_delta_x` and `raw_mouse_delta_y` variables

3. **Cursor Position Emulation** (`GetCursorPos` hook):
   - Initializes `center_point` from real cursor position on first call
   - Returns `center_point + accumulated_deltas` to simulate cursor movement
   - The game calculates: `mx = cursor_pos.x - window_center_x` which yields the raw delta
   - **This preserves the entire original mouse processing pipeline:**
     ```c
     // From Quake 2 IN_MouseMove (lines 272-326 in SDK/Quake-2/win32/in_win.c)
     GetCursorPos(&current_pos);  // Returns our faked position
     mx = current_pos.x - window_center_x;  // Gets raw delta
     mouse_x *= sensitivity->value;  // Original sensitivity still works!
     mouse_y *= m_filter->value;     // Original filtering still works!
     cl.viewangles[YAW] -= m_yaw->value * mouse_x;  // Original cvars work!
     ```

4. **Cursor Warping Prevention** (`SetCursorPos` hook):
   - Returns TRUE immediately without calling original function
   - Prevents cursor from being recentered (which would interfere with delta tracking)

5. **Delta Consumption** (`IN_MouseMove` hook):
   - Resets accumulated deltas to 0 after the game processes them
   - Prevents delta accumulation between frames

## How Original Cvars Are Preserved

The implementation **does not replace** `IN_MouseMove`, it only provides it with raw deltas through faked `GetCursorPos` calls. This means:

### ✅ Still Functional
- `sensitivity` - Mouse sensitivity multiplier (line 305-306 in in_win.c)
- `m_filter` - Mouse smoothing/averaging (lines 291-294)
- `m_yaw` - Yaw (horizontal) sensitivity (line 312)
- `m_pitch` - Pitch (vertical) sensitivity (line 316)
- `m_side` - Strafe sensitivity (line 310)
- `m_forward` - Forward/backward sensitivity (line 320)
- `freelook` - Free look mode toggle (line 314)
- `lookstrafe` - Look strafe mode (line 309)

### Why It Works
The original `IN_MouseMove` code calculates movement as:
```c
mx = current_pos.x - window_center_x;  // Delta from center
```

Our `GetCursorPos` hook returns:
```c
lpPoint->x = center_point.x + raw_mouse_delta_x;
```

So the game still computes: `mx = (center + delta) - center = delta` ✅

Then all original processing applies:
- Filtering: `mouse_x = (mx + old_mx) * 0.5` if `m_filter` enabled
- Sensitivity: `mouse_x *= sensitivity->value`
- View angles: `cl.viewangles[YAW] -= m_yaw->value * mouse_x`

## References

Based on Quake 2 SDK mouse input implementation (see `SDK/Quake-2/win32/in_win.c`):
- `IN_MouseMove` function (lines 272-326)
- Uses `GetCursorPos`/`SetCursorPos` for cursor management
- Calculates mouse delta: `mx = current_pos.x - window_center_x`
- Our implementation transparently injects raw deltas into this existing flow

