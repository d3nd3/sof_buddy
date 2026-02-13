#include "feature_config.h"

#if FEATURE_RAW_MOUSE

#include "sof_compat.h"
#include "util.h"
#include "generated_detours.h"
#include "../shared.h"

using detour_DispatchMessageA::tDispatchMessageA;

#if SOFBUDDY_RAWINPUT_API_AVAILABLE
static bool ReadRawInputBuffer(HRAWINPUT input_handle, BYTE* buffer, UINT buffer_size)
{
    UINT size = buffer_size;
    UINT copied = GetRawInputData(input_handle, RID_INPUT, buffer, &size, sizeof(RAWINPUTHEADER));
    if (copied == static_cast<UINT>(-1)) {
        return false;
    }
    return copied == size && size >= sizeof(RAWINPUTHEADER);
}

static void ProcessRawInput(LPARAM lParam)
{
    UINT dwSize = 0;
    if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER)) == static_cast<UINT>(-1)) {
        return;
    }
    if (dwSize < sizeof(RAWINPUTHEADER)) {
        return;
    }

    BYTE stackBuffer[256];
    RAWINPUT* raw = nullptr;

    if (dwSize <= sizeof(stackBuffer)) {
        if (!ReadRawInputBuffer(reinterpret_cast<HRAWINPUT>(lParam), stackBuffer, dwSize)) {
            return;
        }
        raw = reinterpret_cast<RAWINPUT*>(stackBuffer);
    } else {
        g_heapBuffer.resize(dwSize);
        if (!ReadRawInputBuffer(reinterpret_cast<HRAWINPUT>(lParam), g_heapBuffer.data(), dwSize)) {
            return;
        }
        raw = reinterpret_cast<RAWINPUT*>(g_heapBuffer.data());
    }

    if (!raw || raw->header.dwType != RIM_TYPEMOUSE) {
        return;
    }

    const RAWMOUSE& mouse = raw->data.mouse;
    if ((mouse.usFlags & MOUSE_MOVE_ABSOLUTE) != 0) {
        return;
    }

    if (mouse.lLastX == 0 && mouse.lLastY == 0) {
        return;
    }

    raw_mouse_accumulate_delta(mouse.lLastX, mouse.lLastY);
}
#else
static void ProcessRawInput(LPARAM lParam)
{
    (void)lParam;
}
#endif

static void EnsureRawMouseRegistered(HWND hwnd_hint)
{
    if (!raw_mouse_is_enabled() || !raw_mouse_api_supported()) {
        return;
    }

    HWND hwnd = raw_mouse_hwnd_target;
    if (hwnd && !IsWindow(hwnd)) {
        raw_mouse_registered = false;
        raw_mouse_hwnd_target = nullptr;
        hwnd = nullptr;
    }

    if (!hwnd) hwnd = hwnd_hint;
    if (!hwnd) hwnd = GetActiveWindow();
    if (!hwnd) hwnd = GetForegroundWindow();
    if (!hwnd) {
        return;
    }

    if (!raw_mouse_registered || raw_mouse_hwnd_target != hwnd) {
        raw_mouse_register_input(hwnd, false);
    }
}

static bool msg_affects_cursor_clip(UINT msg) {
    switch (msg) {
    case WM_INPUT:
    case WM_SIZE:
    case WM_MOVE:
    case WM_ACTIVATE:
#ifndef WM_ENTERSIZEMOVE
#define WM_ENTERSIZEMOVE 0x0231
#endif
#ifndef WM_EXITSIZEMOVE
#define WM_EXITSIZEMOVE  0x0232
#endif
    case WM_ENTERSIZEMOVE:
    case WM_EXITSIZEMOVE:
#ifndef WM_DISPLAYCHANGE
#define WM_DISPLAYCHANGE 0x007E
#endif
    case WM_DISPLAYCHANGE:
        return true;
    default:
        return false;
    }
}

LRESULT dispatchmessagea_override_callback(const MSG* msg, detour_DispatchMessageA::tDispatchMessageA original) {
    if (msg && raw_mouse_is_enabled() && raw_mouse_api_supported()) {
        if (msg_affects_cursor_clip(msg->message))
            EnsureRawMouseRegistered(msg->hwnd);
        if (msg->message == WM_INPUT)
            ProcessRawInput(msg->lParam);
    }
    if (msg && msg_affects_cursor_clip(msg->message))
        raw_mouse_refresh_cursor_clip(msg->hwnd);
    return original ? original(msg) : 0;
}

#endif // FEATURE_RAW_MOUSE
