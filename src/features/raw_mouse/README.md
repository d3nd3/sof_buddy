# Raw Mouse Input

## Purpose
Implements raw mouse input support for Soldier of Fortune using the Windows Raw Input API instead of the default GetCursorPos/SetCursorPos cursor management. Provides smoother, more direct mouse response without Windows acceleration/smoothing while preserving all original SoF mouse cvars.

## Callbacks
- **EarlyStartup** (Post, Priority: 70)
  - `raw_mouse_EarlyStartup()` - Patches three `SetCursorPos` call sites in the exe (IN_Frame, IN_MouseMove, IN_MenuMouse) to redirect through `hkSetCursorPos`, and resolves the real `SetCursorPos` via `GetProcAddress` for passthrough use
- **RefDllLoaded** (Post, Priority: 70)
  - `raw_mouse_RefDllLoaded()` - Creates the `_sofbuddy_rawmouse` cvar with a change callback; if the archived value is non-zero, immediately registers raw input

## Hooks
- **IN_MouseMove** (Post, Priority: 100)
  - `in_mousemove_callback()` - Polls buffered raw input to ensure minimum latency, then resets `raw_mouse_delta_x/y` to 0 after the game has consumed the frame's mouse input.
- **IN_MenuMouse** (Post, Priority: 100)
  - `in_menumouse_callback()` - Same as IN_MouseMove; polls and resets deltas.

## OverrideHooks
- **GetCursorPos** (user32.dll)
  - `getcursorpos_override_callback()` - When enabled, returns virtual cursor position `(window_center + delta_x/y)` instead of the real OS cursor position. On the first call (or after a reset), seeds `window_center` from the real cursor position via one call to the original `GetCursorPos`. When disabled, passes through to the original.
- **DispatchMessageA** (user32.dll)
  - `dispatchmessagea_override_callback()` - When enabled: intercepts `WM_INPUT` messages, processes the current packet, and immediately polls `GetRawInputBuffer` to drain any queued input. This handles high-polling-rate mice efficiently. Also handles registration/clipping on window events.

## CustomDetours
- **SetCursorPos** (user32.dll, via binary patch + GetProcAddress)
  - `hkSetCursorPos()` - When enabled: updates internal `window_center` to the coordinates the game wants to recenter to, and returns TRUE **without** calling the real `SetCursorPos` (the OS cursor is never warped by the game). When disabled: calls the real `SetCursorPos` normally.
  - Patched at exe addresses: `0x2004A0B2` (IN_Frame), `0x2004A410` (IN_MouseMove), `0x2004A579` (IN_MenuMouse)

## Technical Details

### How It Works
The game's original mouse pipeline is: `SetCursorPos` (center cursor) → game logic → `GetCursorPos` (read cursor) → compute delta → apply to view. This feature intercepts that pipeline so the game's own mouse code continues to work unchanged, but the underlying input comes from raw hardware deltas instead of OS cursor position:

1. **CVar Creation** (RefDllLoaded callback):
   - `create_raw_mouse_cvars()` registers `_sofbuddy_rawmouse` with a change callback (`raw_mouse_on_change`)
   - If the archived value is already non-zero (user previously enabled it), the change callback fires immediately

2. **Registration** (cvar change callback):
   - When the cvar is set to non-zero, `raw_mouse_on_change()` calls `raw_mouse_register_input()`
   - This registers for raw mouse input using `RegisterRawInputDevices()` with `hwndTarget` set to the active window
   - Deltas are reset and `window_center` is invalidated so the next `GetCursorPos` call seeds it fresh

3. **Message Processing** (DispatchMessageA hook, only when enabled):
   - Intercepts `WM_INPUT` messages
   - Processes the current message's data immediately via `GetRawInputData`.
   - Calls `GetRawInputBuffer` to drain any remaining input events in the queue in one batch. This prevents the message queue from being flooded by high-Hz mice (e.g. 1000Hz-8000Hz).
   - Absolute mouse packets are ignored; only relative deltas are accumulated via `raw_mouse_accumulate_delta()`
   - On window focus/move/size events, ensures registration is current and refreshes the cursor clip rect

4. **Virtual Cursor Position** (GetCursorPos hook):
   - Returns `(window_center.x + raw_mouse_delta_x, window_center.y + raw_mouse_delta_y)`
   - The game sees this as a normal cursor position offset from center, so its existing delta calculation works unchanged
   - On the very first call (or after a toggle), `window_center` is seeded from the real OS cursor position via one call to the original `GetCursorPos`

5. **No Cursor Warping** (SetCursorPos hook):
   - The game calls `SetCursorPos` every frame to recenter the cursor
   - The hook intercepts this: it records the coordinates as `window_center` and returns TRUE, but **does not call the real SetCursorPos**
   - This prevents the OS cursor from being physically moved, which would otherwise generate synthetic mouse events that conflict with raw input

6. **Cursor Confinement** (ClipCursor):
   - While raw input is enabled and the game window is foregrounded, the OS cursor is clipped to the game client area inset by 64px on all sides to prevent edge-of-screen issues
   - Clip is automatically released when raw input is disabled, focus is lost, or the window cannot be resolved
   - Clip refresh is only performed when raw mouse is enabled — when disabled, the DispatchMessageA hook has zero side effects

7. **Delta Consumption** (IN_MouseMove / IN_MenuMouse post-callbacks):
   - After the game processes the frame's mouse input, `raw_mouse_consume_deltas()` resets `raw_mouse_delta_x/y` to 0
   - This means the next `GetCursorPos` call returns exactly `window_center`, making the game see zero delta until new raw input arrives

### Disabled Path (Feature Compiled In, CVar Off)
When `_sofbuddy_rawmouse` is 0 (the default), all hooks are pure passthroughs:
- **GetCursorPos** → calls original
- **SetCursorPos** → calls original `oSetCursorPos`
- **DispatchMessageA** → calls original with no pre-processing
- **IN_MouseMove / IN_MenuMouse** → returns immediately

No cursor clipping, delta accumulation, or registration occurs. The game behaves identically to an unhooked state.

### Jitter & high polling rate
- **No cursor warp on clip refresh.** We do not call `SetCursorPos` to snap the OS cursor back inside the clip rect when refreshing. Warping generates synthetic mouse events that mix with raw deltas and cause spikey/jittery movement. Only `ClipCursor` is applied.
- **Zero-delta packets skipped.** `ProcessRawInput` ignores `WM_INPUT` with `lLastX == 0 && lLastY == 0` so spurious (0,0) reports don’t add noise.
- **Buffered Input.** Uses `GetRawInputBuffer` in both the message loop (draining the queue) and the frame loop (fetching latest data). This minimizes overhead for 1000Hz+ mice and ensures the lowest possible input latency.
- **Legacy input left enabled.** We register with `dwFlags = 0` (no `RIDEV_NOLEGACY`) so window dragging and other system mouse behavior keep working.
- **Windows 11.** KB5028185 and 24H2 improve behavior for high-polling-rate mice (throttling/coalescing for background listeners, USB polling optimizations). No code change required; up-to-date Win11 can help on high-Hz hardware.

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
  - Changes take effect immediately via the cvar change callback

## Benefits
- Smoother, more direct mouse response (no Windows acceleration/smoothing)
- Better support for high DPI mice
- Elimination of cursor warping issues
- Prevents accidental focus loss from cursor hitting desktop/taskbar edges
- Preserves all original SoF mouse cvars
