#include "feature_config.h"

#if FEATURE_RAW_MOUSE

#include "sof_compat.h"
#include "util.h"
#include "../shared.h"

tSetCursorPos oSetCursorPos = nullptr;

BOOL __stdcall hkSetCursorPos(int X, int Y)
{
    SOFBUDDY_ASSERT(in_mouse_raw != nullptr);
    SOFBUDDY_ASSERT(oSetCursorPos != nullptr);
    
    if (!raw_mouse_is_enabled() || !raw_mouse_api_supported()) {
        return oSetCursorPos(X, Y);
    }

    HWND hwnd = GetActiveWindow();
    if (!hwnd) hwnd = GetForegroundWindow();
    if (hwnd && (!raw_mouse_registered || raw_mouse_hwnd_target != hwnd)) {
        raw_mouse_register_input(hwnd, false);
    }

    raw_mouse_update_center(X, Y);
    return oSetCursorPos(X, Y);
}

#endif // FEATURE_RAW_MOUSE
