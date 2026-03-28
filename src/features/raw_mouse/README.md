# Raw Mouse Input

## Purpose
Implements raw mouse input for Soldier of Fortune using the Windows Raw Input API. Uses a [PH3-style](https://ph3at.github.io/posts/Windows-Input/) buffered approach: one `GetRawInputBuffer` read per frame, two-range PeekMessage so WM_INPUT is never dispatched, and no per-message `GetRawInputData`. Keeps legacy input enabled (dwFlags=0) for UI/alt-tab. Preserves all original SoF mouse cvars. Only relative mouse motion is handled (gaming mice); absolute (e.g. RDP) is not.

## How it works (overview)
- **Registration:** Mouse raw input is registered once in focus-following mode (`hwndTarget = NULL`, `dwFlags = 0`). That means Win32 routes `WM_INPUT` to the focused app window without us needing to hand `RegisterRawInputDevices` a specific HWND. `raw_mouse_hwnd_target` is still maintained, but only for cursor clip/focus decisions and logging.
- **Window resolution:** When we do need a concrete game window, we prefer the engine’s `cl_hwnd`, then cached/hinted windows, then GUI-thread / active / foreground / thread-enumerated fallbacks. Candidates are normalized to root HWNDs and scored so likely helper/tool windows lose to the real game window.
- **Buffered read:** Persistent `std::vector<BYTE>` buffer, grown when required. Query size, then `GetRawInputBuffer`; on ERROR_INSUFFICIENT_BUFFER retry with larger buffer (up to 4 retries, 256KB cap). Iterate with NEXTRAWINPUTBLOCK; for each RIM_TYPEMOUSE packet accumulate `lLastX`/`lLastY` into `raw_mouse_delta_x`/`raw_mouse_delta_y`. Drain is skipped when `ActiveApp` says the game is inactive or when our resolved target is not foreground.
- **Virtual cursor:** GetCursorPos returns `(window_center + raw_mouse_delta_x, window_center + raw_mouse_delta_y)`; center is seeded from the game’s SetCursorPos (patched call sites pass center). SetCursorPos (when raw on) only updates `window_center` and does not move the OS cursor.
- **Message pump:** Sys_SendKeyEvents and winmain loop are replaced with capped two-range pumps: PeekMessage in two ranges (0..WM_INPUT-1, WM_INPUT+1..0xFFFFFFFF), so WM_INPUT is never peeked and never delivered. DispatchMessageA is not special-cased for WM_INPUT; on window events (WM_SIZE, WM_MOVE, WM_ACTIVATE, etc.) we call `raw_mouse_ensure_registered(msg->hwnd)` and `raw_mouse_refresh_cursor_clip(msg->hwnd)`.
- **Per-frame flow:** The Sys_SendKeyEvents override resets accumulated deltas, then runs `Sys_SendKeyEvents_Replacement()` (capped pump + frame time). Raw-buffer draining happens from the `GetCursorPos` path so the synthetic cursor always sees the freshest available hardware deltas.

## WOW64 (32-bit): what finally made it work

**Critical:** On WOW64 (32-bit app on 64-bit Windows), `GetRawInputBuffer` uses a different layout and size convention. Without this, `lLastX`/`lLastY` stay zero and raw mouse does nothing.

1. **Buffer size**  
   The size returned by the query (or by the API on ERROR_INSUFFICIENT_BUFFER) is **not** in bytes. Use **size × 8** for the actual buffer size in bytes.  
   - After the initial query: `if (!defined(_WIN64)) size *= 8;`  
   - In the retry path when growing: same scaling for the required size before resize.

2. **Mouse data offset**  
   The RAWINPUT data union (e.g. RAWMOUSE) is **not** at offset 16 after the header. It is at **offset 24** (16-byte header + 8-byte alignment padding).  
   - Do **not** use `current->data.mouse` on 32-bit; that reads the wrong bytes.  
   - Read RAWMOUSE from `(const char*)current + 24`:  
     `const RAWMOUSE* m = reinterpret_cast<const RAWMOUSE*>(reinterpret_cast<const char*>(current) + 24);`  
   - 64-bit keeps using `&current->data.mouse`.

References: MSDN [GetRawInputBuffer](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getrawinputbuffer) (WOW64 note), RAWINPUT layout with `FieldOffset(16+8)` for the data union.

## Callbacks
- **EarlyStartup** (Post, 70) – Patches three SetCursorPos call sites (E8 + NOP) to `hkSetCursorPos`; installs winmain detour via `raw_mouse_install_winmain_peek_detour()` (WinMainPeekLoop + capped pump); resolves `oSetCursorPos` via GetProcAddress(user32, "SetCursorPos").
- **RefDllLoaded** (Post, 70) – Creates `_sofbuddy_rawmouse` cvar (CVAR_SOFBUDDY_ARCHIVE) with `raw_mouse_on_change`; if archived value is non-zero, calls `raw_mouse_on_change` immediately (resets deltas and center_valid, then attempts normal `raw_mouse_ensure_registered(nullptr)` registration).

## Override hooks (from hooks.json)
- **GetCursorPos** – When enabled: if center not valid, seed from `original(lpPoint)` and `raw_mouse_update_center`; return virtual position `(window_center + raw_mouse_delta_x, window_center + raw_mouse_delta_y)`. Returns FALSE for null lpPoint (ERROR_INVALID_PARAMETER) or if center remains invalid. When disabled: passthrough.
- **DispatchMessageA** – When enabled: on window events (WM_SIZE, WM_MOVE, WM_ACTIVATE, WM_ENTERSIZEMOVE, WM_EXITSIZEMOVE, WM_DISPLAYCHANGE) call `raw_mouse_ensure_registered(msg->hwnd)` and `raw_mouse_refresh_cursor_clip(msg->hwnd)`. When disabled: passthrough.
- **Sys_SendKeyEvents** – Replaced entirely: `raw_mouse_consume_deltas()`, then `Sys_SendKeyEvents_Replacement()` (capped pump + frame time). Raw-buffer draining now lives in the `GetCursorPos` path.

## Custom detours
- **SetCursorPos** (patch + GetProcAddress) – When enabled: treat as recenter; set `window_center`, return TRUE without moving OS cursor. Patched at IN_Frame, IN_MouseMove, IN_MenuMouse. When disabled: real SetCursorPos.

## Technical details
- **Registration:** `raw_mouse_ensure_registered()` clears stale cached targets, resolves a usable game HWND for clip/focus tracking, and separately ensures the process-level raw mouse registration exists. The Win32 registration path always uses `hwndTarget = NULL` for the normal focused-app case, which removes the usual “bad HWND” source of error 87.
- **Focus:** `RawMouseHasForeground(hwnd)` compares root of hwnd with root of `GetForegroundWindow()`. Raw drain also checks the game’s `ActiveApp` flag before reading buffered input. Clip uses `RawMouseResolveTargetHwnd(hwnd_hint)` and strongly prefers the engine’s `cl_hwnd`.
- **Buffer:** Persistent `raw_read_buf` (std::vector<BYTE>). Size aligned to sizeof(RAWINPUTHEADER). On 32-bit (!_WIN64), query and retry required size scaled by 8. Retry loop on ERROR_INSUFFICIENT_BUFFER: grow (or use readSize×8 on 32-bit), align, cap 256KB, then retry; up to 4 retries.
- **Clip:** `raw_mouse_refresh_cursor_clip()` uses client rect (inset RAW_MOUSE_CLIP_INSET 64px); ClipCursor to that rect when we have foreground. Released when not focused, when disabled, when GetClientRect/ClientToScreen fails, when rect is degenerate after inset, or when ClipCursor fails. No clip change if rect unchanged.
- **Diagnostics:** On registration failures, especially error 87, we now log the candidate windows we considered plus the current `GetRegisteredRawInputDevices` mouse registration. This helps distinguish “bad HWND selection” from “another in-process component already touched raw input”.
- **Capped pump:** `PumpWindowMessagesCapped()` processes up to MAX_MSG_PER_FRAME (10) messages in each of two PeekMessage ranges (0..WM_INPUT-1, WM_INPUT+1..0xFFFFFFFF), then TranslateMessage + DispatchMessageA. WM_INPUT is never peeked, so never delivered.

## Configuration
- **_sofbuddy_rawmouse** (default: 0, archived) – 0 = off (legacy cursor). Non-zero = on (buffered raw mouse). Change applies immediately.

## Benefits
- One buffer read per frame; no per-message cost at high polling rates.
- No GetRawInputData; no drain-from-handle.
- Legacy input kept for window drag / alt-tab.
- Correct behavior on 32-bit (WOW64) via size×8 and data offset 24.
- Smoother response, no Windows acceleration; preserves SoF mouse cvars.
