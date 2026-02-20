#include "feature_config.h"

#if FEATURE_RAW_MOUSE

#include "../shared.h"
#include "generated_detours.h"
#include "sof_compat.h"
#include "util.h"

using detour_GetCursorPos::tGetCursorPos;

BOOL getcursorpos_override_callback(
    LPPOINT lpPoint, detour_GetCursorPos::tGetCursorPos original) {
  if (!in_mouse_raw || in_mouse_raw->value == 0.0f) {
    // Raw mouse disabled - call original
    SOFBUDDY_ASSERT(original != nullptr);
    return original(lpPoint);
  }

  if (!lpPoint) {
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }

  if (!raw_mouse_center_valid) {
    SOFBUDDY_ASSERT(original != nullptr);
    if (!original(lpPoint))
      return FALSE;
    raw_mouse_update_center(lpPoint->x, lpPoint->y);
  }
  if (!raw_mouse_center_valid) {
    return FALSE;
  }

  // Raw mouse enabled - return virtual cursor position
  lpPoint->x = window_center.x + raw_mouse_delta_x;
  lpPoint->y = window_center.y + raw_mouse_delta_y;
  return TRUE;
}

#endif // FEATURE_RAW_MOUSE
