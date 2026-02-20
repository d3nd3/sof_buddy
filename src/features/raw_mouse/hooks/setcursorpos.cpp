#include "feature_config.h"

#if FEATURE_RAW_MOUSE

#include "../shared.h"
#include "sof_compat.h"
#include "util.h"

tSetCursorPos oSetCursorPos = nullptr;

BOOL __stdcall hkSetCursorPos(int X, int Y) {
  SOFBUDDY_ASSERT(oSetCursorPos != nullptr);

  if (!raw_mouse_is_enabled() || !raw_mouse_api_supported()) {
    return oSetCursorPos(X, Y);
  }

  raw_mouse_update_center(X, Y);
  return oSetCursorPos(X, Y);
}

#endif // FEATURE_RAW_MOUSE
