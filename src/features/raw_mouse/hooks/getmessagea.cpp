#include "feature_config.h"

#if FEATURE_RAW_MOUSE

#include "../shared.h"
#include "generated_detours.h"
#include "sof_compat.h"

using detour_GetMessageA::tGetMessageA;

BOOL getmessagea_override_callback(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin,
    UINT wMsgFilterMax, tGetMessageA original) {
  if (!raw_mouse_is_enabled() || !raw_mouse_api_supported() || raw_mouse_mode() != 2)
    return original(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax);
  for (;;) {
    BOOL ret = original(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax);
    if (!ret) return ret;
    if (lpMsg->message == WM_INPUT) {
      raw_mouse_accumulate_from_handle(reinterpret_cast<HRAWINPUT>(lpMsg->lParam));
      continue;
    }
    return ret;
  }
}

#endif
