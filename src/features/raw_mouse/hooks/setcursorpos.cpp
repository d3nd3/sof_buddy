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
    
    if (!in_mouse_raw || in_mouse_raw->value == 0) {
        return oSetCursorPos(X, Y);
    }

    raw_mouse_delta_x = 0;
    raw_mouse_delta_y = 0;
    
    window_center.x = X;
    window_center.y = Y;
    return oSetCursorPos(X, Y);
}

#endif // FEATURE_RAW_MOUSE

