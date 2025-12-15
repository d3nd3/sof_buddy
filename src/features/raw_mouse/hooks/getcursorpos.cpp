#include "feature_config.h"

#if FEATURE_RAW_MOUSE

#include "sof_compat.h"
#include "util.h"
#include "generated_detours.h"
#include "../shared.h"

using detour_GetCursorPos::tGetCursorPos;

BOOL getcursorpos_override_callback(LPPOINT lpPoint, detour_GetCursorPos::tGetCursorPos original) {
    if (!in_mouse_raw || in_mouse_raw->value == 0) {
        // Raw mouse disabled - call original
        return original ? original(lpPoint) : FALSE;
    }
    
    // Raw mouse enabled - return virtual cursor position
    if (lpPoint) {
        lpPoint->x = window_center.x + raw_mouse_delta_x;
        lpPoint->y = window_center.y + raw_mouse_delta_y;
    }
    
    return TRUE;
}

#endif // FEATURE_RAW_MOUSE

