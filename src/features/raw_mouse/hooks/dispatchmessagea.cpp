#include "feature_config.h"

#if FEATURE_RAW_MOUSE

#include "../shared.h"
#include "generated_detours.h"
#include "sof_compat.h"
#include "util.h"
#include <vector>

using detour_DispatchMessageA::tDispatchMessageA;

#if SOFBUDDY_RAWINPUT_API_AVAILABLE
static std::vector<BYTE> g_dispatchBuffer;

static bool ReadRawInputBufferInternal(HRAWINPUT input_handle, std::vector<BYTE>& buffer) {
    UINT dwSize = 0;
    if (GetRawInputData(input_handle, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER)) == (UINT)-1) {
        return false;
    }
    
    if (dwSize < sizeof(RAWINPUTHEADER)) {
        return false;
    }

    if (buffer.size() < dwSize) {
        buffer.resize(dwSize);
    }

    if (GetRawInputData(input_handle, RID_INPUT, buffer.data(), &dwSize, sizeof(RAWINPUTHEADER)) != dwSize) {
        return false;
    }
    
    return true;
}

static void ProcessRawInputMessage(LPARAM lParam) {
    // Process the specific message that triggered WM_INPUT
    if (!ReadRawInputBufferInternal(reinterpret_cast<HRAWINPUT>(lParam), g_dispatchBuffer)) {
        return;
    }

    RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(g_dispatchBuffer.data());
    if (raw->header.dwType != RIM_TYPEMOUSE) {
        return;
    }

    const RAWMOUSE &mouse = raw->data.mouse;
    if ((mouse.usFlags & MOUSE_MOVE_ABSOLUTE) != 0) {
        return;
    }
    
    if (mouse.lLastX != 0 || mouse.lLastY != 0) {
        raw_mouse_accumulate_delta(mouse.lLastX, mouse.lLastY);
    }
    if (raw_mouse_mode() != 1 || !raw_mouse_is_wine())
        raw_mouse_poll();
}
#else
static void ProcessRawInputMessage(LPARAM lParam) { (void)lParam; }
#endif

static void EnsureRawMouseRegistered(HWND hwnd_hint) {
  if (!raw_mouse_is_enabled() || !raw_mouse_api_supported()) {
    return;
  }

  HWND hwnd = raw_mouse_hwnd_target;
  if (hwnd && !IsWindow(hwnd)) {
    raw_mouse_registered = false;
    raw_mouse_hwnd_target = nullptr;
    hwnd = nullptr;
  }

  if (!hwnd)
    hwnd = hwnd_hint;
  if (!hwnd)
    hwnd = GetActiveWindow();
  if (!hwnd)
    hwnd = GetForegroundWindow();
  if (!hwnd) {
    return;
  }

  if (!raw_mouse_registered || raw_mouse_hwnd_target != hwnd) {
    raw_mouse_register_input(hwnd, false);
  }
}

static bool msg_affects_cursor_clip(UINT msg) {
  switch (msg) {
  case WM_SIZE:
  case WM_MOVE:
  case WM_ACTIVATE:
#ifndef WM_ENTERSIZEMOVE
#define WM_ENTERSIZEMOVE 0x0231
#endif
#ifndef WM_EXITSIZEMOVE
#define WM_EXITSIZEMOVE 0x0232
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

LRESULT dispatchmessagea_override_callback(
    const MSG *msg, detour_DispatchMessageA::tDispatchMessageA original) {
  if (msg && raw_mouse_is_enabled() && raw_mouse_api_supported()) {
    if (msg_affects_cursor_clip(msg->message)) {
      EnsureRawMouseRegistered(msg->hwnd);
      raw_mouse_refresh_cursor_clip(msg->hwnd);
    }
    if (msg->message == WM_INPUT && raw_mouse_mode() == 1)
      ProcessRawInputMessage(msg->lParam);
  }
  SOFBUDDY_ASSERT(original != nullptr);
  return original(msg);
}

#endif // FEATURE_RAW_MOUSE