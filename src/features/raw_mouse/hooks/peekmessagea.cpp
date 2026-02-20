#include "feature_config.h"

#if FEATURE_RAW_MOUSE

#include "../shared.h"
#include "generated_detours.h"

using detour_PeekMessageA::tPeekMessageA;

BOOL peekmessagea_override_callback(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin,
    UINT wMsgFilterMax, UINT wRemoveMsg, tPeekMessageA original) {
  if (!raw_mouse_is_enabled() || !raw_mouse_api_supported() || raw_mouse_mode() != 2)
    return original(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, wRemoveMsg);
  raw_mouse_set_peekmessage_original(reinterpret_cast<void*>(original));
  if (original(lpMsg, hWnd, 0, WM_INPUT - 1, wRemoveMsg)) return TRUE;
  return original(lpMsg, hWnd, WM_INPUT + 1, 0xFFFF, wRemoveMsg);
}

#endif
