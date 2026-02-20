# Raw Mouse Input

## Purpose
Implements raw mouse input support for Soldier of Fortune using the Windows Raw Input API instead of the default GetCursorPos/SetCursorPos cursor management. Provides smoother, more direct mouse response without Windows acceleration/smoothing while preserving all original SoF mouse cvars.

## Callbacks
- **EarlyStartup** (Post, Priority: 70)
  - `raw_mouse_EarlyStartup()` - Patches three `SetCursorPos` call sites in the exe (IN_Frame, IN_MouseMove, IN_MenuMouse) to redirect through `hkSetCursorPos`, and resolves the real `SetCursorPos` via `GetProcAddress` for passthrough use
- **RefDllLoaded** (Post, Priority: 70)
  - `raw_mouse_RefDllLoaded()` - Creates the `_sofbuddy_rawmouse` cvar with a change callback; if the archived value is non-zero, immediately registers raw input

## Hooks
- **IN_MouseMove** (Pre, Priority: 100) / (Post, Priority: 100)
  - Pre: Mode 2 only — `raw_mouse_drain_wm_input()` then `raw_mouse_poll()`. Post: `raw_mouse_consume_deltas()`; mode 1 also calls `raw_mouse_poll()` before consume.
- **IN_MenuMouse** (Pre, Priority: 100) / (Post, Priority: 100)
  - Same as IN_MouseMove (mode 2: drain + poll in Pre; Post: consume, mode 1 poll+consume).

## OverrideHooks
- **GetCursorPos** (user32.dll)
  - `getcursorpos_override_callback()` - When enabled, returns virtual cursor position `(window_center + delta_x/y)` instead of the real OS cursor position. On the first call (or after a reset), seeds `window_center` from the real cursor position via one call to the original `GetCursorPos`. When disabled, passes through to the original.
- **DispatchMessageA** (user32.dll)
  - When enabled: registration/clipping on window events. Mode 1: for WM_INPUT calls ProcessRawInputMessage (GetRawInputData(lParam) + raw_mouse_poll(); on Wine we skip raw_mouse_poll() to avoid double-count). Mode 2: WM_INPUT never reaches Dispatch (filtered by Peek/GetMessage).
- **PeekMessageA** (user32.dll)
  - Mode 2 only: two-range peek (0..WM_INPUT-1, WM_INPUT+1..0xFFFF) so when only WM_INPUT is queued the loop sees "no message" and skips GetMessage/Dispatch. Mode 1: passthrough.
- **GetMessageA** (user32.dll)
  - Mode 2 only: when the real GetMessageA returns WM_INPUT we call `raw_mouse_accumulate_from_handle(lParam)` and loop (never return WM_INPUT). So mixed queues are handled without dispatching WM_INPUT. Mode 1: passthrough.

## CustomDetours
- **SetCursorPos** (user32.dll, via binary patch + GetProcAddress)
  - `hkSetCursorPos()` - When enabled: updates internal `window_center` to the coordinates the game wants to recenter to, and returns TRUE **without** calling the real `SetCursorPos` (the OS cursor is never warped by the game). When disabled: calls the real `SetCursorPos` normally.
  - Patched at exe addresses: `0x2004A0B2` (IN_Frame), `0x2004A410` (IN_MouseMove), `0x2004A579` (IN_MenuMouse)

## Technical Details

### Modes and relation to PH3

[PH3’s article](https://ph3at.github.io/posts/Windows-Input/) describes the problem: with high-polling mice, dispatching every WM_INPUT in the message loop kills performance. Their approach: **peek all messages except WM_INPUT** (two-range peek), so the loop never runs for raw input; then get all raw input in one go via **GetRawInputBuffer** at frame start. They keep legacy input enabled so window dragging etc. still work.

We have three modes:

- **0** – Off. No raw input; GetCursorPos/SetCursorPos unchanged.
- **1 (4.4-style)** – WM_INPUT is dispatched. DispatchMessageA processes each WM_INPUT (GetRawInputData(lParam) then raw_mouse_poll()). IN_MouseMove/IN_MenuMouse Post: poll + consume. Smooth on native Windows; with 4k/8k mice the loop can do many dispatches per frame. **On Wine** we skip raw_mouse_poll() inside ProcessRawInputMessage so the same event isn’t counted from both the message and the buffer (avoids inconsistent cursor speed).
- **2 (PH3-style)** – We avoid dispatching WM_INPUT like PH3: PeekMessageA uses a two-range peek so when **only** WM_INPUT is queued the game’s loop sees “no message” and goes straight to the frame (no GetMessage/Dispatch for those). On Windows, **GetRawInputBuffer does not get the data** when WM_INPUT are never consumed, so we can’t use “buffer only” like PH3. Instead we **drain** WM_INPUT at frame start: in IN_MouseMove/IN_MenuMouse **Pre** we call `raw_mouse_drain_wm_input()` (loop with PeekMessage(WM_INPUT, WM_INPUT, PM_REMOVE), accumulate from each handle via GetRawInputData), then raw_mouse_poll(). GetMessageA still runs when the queue has a mix (e.g. timer + WM_INPUT): we accumulate from each WM_INPUT and never return it, so we never dispatch WM_INPUT. Result: no dispatch flood, no dropped input, one batch of work per frame in PRE.

So mode 2 matches PH3’s idea (don’t run the message loop for WM_INPUT; get raw input in a batch before the game uses it) but gets the batch by draining queued WM_INPUT and reading from message handles, not from GetRawInputBuffer.

### How It Works
The game's original mouse pipeline is: `SetCursorPos` (center cursor) → game logic → `GetCursorPos` (read cursor) → compute delta → apply to view. This feature intercepts that pipeline so the game's own mouse code continues to work unchanged, but the underlying input comes from raw hardware deltas instead of OS cursor position:

1. **CVar Creation** (RefDllLoaded callback):
   - `create_raw_mouse_cvars()` registers `_sofbuddy_rawmouse` with a change callback (`raw_mouse_on_change`)
   - If the archived value is already non-zero (user previously enabled it), the change callback fires immediately

2. **Registration** (cvar change callback):
   - When the cvar is set to non-zero, `raw_mouse_on_change()` calls `raw_mouse_register_input()`
   - This registers for raw mouse input using `RegisterRawInputDevices()` with `hwndTarget` set to the active window
   - Deltas are reset and `window_center` is invalidated so the next `GetCursorPos` call seeds it fresh

3. **Message loop** (mode-dependent):
   - **Mode 1:** PeekMessageA/GetMessageA passthrough. Every message including WM_INPUT is seen; DispatchMessageA processes WM_INPUT (accumulate from lParam + poll buffer; on Wine we skip the poll).
   - **Mode 2:** PeekMessageA two-range peek so “only WM_INPUT” → no message, loop exits. GetMessageA: on WM_INPUT we accumulate and loop (never return WM_INPUT). WM_INPUT that were “skipped” by the peek are drained in IN_MouseMove/IN_MenuMouse Pre via raw_mouse_drain_wm_input(). DispatchMessageA never sees WM_INPUT.
   - DispatchMessageA always handles registration/clipping on window events.

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

7. **Poll and consume** (IN_MouseMove / IN_MenuMouse Pre and Post):
   - **Mode 1:** Pre does nothing; Post: raw_mouse_poll() then raw_mouse_consume_deltas().
   - **Mode 2:** Pre: raw_mouse_drain_wm_input() (drain queued WM_INPUT into deltas) then raw_mouse_poll(). Post: raw_mouse_consume_deltas().
   - Consume resets deltas so the next frame starts from center until new input arrives.

### Disabled Path (Feature Compiled In, CVar Off)
When `_sofbuddy_rawmouse` is 0 (the default), all hooks are pure passthroughs:
- **GetCursorPos** → calls original
- **SetCursorPos** → calls original `oSetCursorPos`
- **DispatchMessageA / PeekMessageA / GetMessageA** → call original with no pre-processing
- **IN_MouseMove / IN_MenuMouse** → return immediately

No cursor clipping, delta accumulation, or registration occurs. The game behaves identically to an unhooked state.
If the Windows Raw Input API is unavailable for the current build or registration fails, enabling `_sofbuddy_rawmouse` logs an error and the game stays on this legacy cursor path; behavior remains identical to the disabled state.

### Jitter & high polling rate
- **No cursor warp on clip refresh.** We do not call `SetCursorPos` to snap the OS cursor inside the clip rect; only `ClipCursor` is applied. Warping would add synthetic events and jitter.
- **Zero-delta packets skipped.** Relative packets with `lLastX == 0 && lLastY == 0` are not accumulated.
- **Mode 2 (PH3-style).** Two-range PeekMessageA + drain in PRE avoids dispatch flood while capturing all WM_INPUT via handles. No per-message GetMessage in the hot path when only WM_INPUT is queued.
- **Legacy input left enabled.** We register with `dwFlags = 0` (no `RIDEV_NOLEGACY`) so window dragging and system mouse behavior keep working (same as PH3).
- **Windows 11.** KB5028185 and 24H2 help with high-polling mice; no code change required.

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
  - **0** – Off. Legacy cursor (GetCursorPos/SetCursorPos).
  - **1** – 4.4-style: WM_INPUT dispatched; DispatchMessageA processes each (GetRawInputData + raw_mouse_poll; on Wine poll is skipped to avoid double-count). IN_MouseMove/IN_MenuMouse Post: poll + consume. Smooth; high-polling mice can cost CPU.
  - **2** – PH3-style: two-range PeekMessageA so the loop doesn’t run for “only WM_INPUT”; WM_INPUT drained in IN_MouseMove/IN_MenuMouse Pre (raw_mouse_drain_wm_input + raw_mouse_poll). GetMessageA still consumes WM_INPUT when the queue has other message types. No dispatch flood; recommended for 4k/8k mice or if mode 1 drops FPS. On Wine, mode 1 is often sufficient after the Wine fix; mode 2 is an alternative.
  - Changes take effect immediately via the cvar change callback.

## Benefits
- Smoother, more direct mouse response (no Windows acceleration/smoothing)
- Better support for high DPI mice
- Elimination of cursor warping issues
- Prevents accidental focus loss from cursor hitting desktop/taskbar edges
- Preserves all original SoF mouse cvars
