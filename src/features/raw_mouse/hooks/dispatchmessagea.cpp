#include "feature_config.h"

#if FEATURE_RAW_MOUSE

#include "../shared.h"
#include "generated_detours.h"
#include "util.h"

using detour_DispatchMessageA::tDispatchMessageA;

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
      raw_mouse_ensure_registered(msg->hwnd);
      raw_mouse_refresh_cursor_clip(msg->hwnd);
    }
  }
  SOFBUDDY_ASSERT(original != nullptr);
  return original(msg);
}

#endif // FEATURE_RAW_MOUSE