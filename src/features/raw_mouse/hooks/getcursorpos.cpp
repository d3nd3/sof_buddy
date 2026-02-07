#include "feature_config.h"

#if FEATURE_RAW_MOUSE

#include "sof_compat.h"
#include "util.h"
#include "generated_detours.h"
#include "../shared.h"

using detour_GetCursorPos::tGetCursorPos;

BOOL getcursorpos_override_callback(LPPOINT lpPoint, detour_GetCursorPos::tGetCursorPos original) {
    SOFBUDDY_ASSERT(in_mouse_raw != nullptr);
    
    if (!in_mouse_raw || in_mouse_raw->value == 0.0f) {
        // Raw mouse disabled - call original
        SOFBUDDY_ASSERT(original != nullptr);
        return original ? original(lpPoint) : FALSE;
    }

    if (!lpPoint) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (!raw_mouse_center_valid && original) {
        if (original(lpPoint)) {
            raw_mouse_update_center(lpPoint->x, lpPoint->y);
        } else {
            return FALSE;
        }
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
