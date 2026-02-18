#include "feature_config.h"

#if FEATURE_RAW_MOUSE

#include <stdint.h>
#include "sof_compat.h"
#include "util.h"
#include <windows.h>
#include "shared.h"

cvar_t * in_mouse_raw = NULL;

void raw_mouse_on_change(cvar_t *cvar)
{
    if (!cvar) return;

    raw_mouse_reset_deltas();
    raw_mouse_center_valid = false;

    if (cvar->value != 0.0f) {
        if (!raw_mouse_api_supported()) {
            PrintOut(PRINT_BAD, "raw_mouse: Raw Input API not available; keeping legacy cursor mode\n");
            return;
        }
        HWND hwnd = GetActiveWindow();
        if (!hwnd) hwnd = GetForegroundWindow();
        if (raw_mouse_register_input(hwnd, true)) {
            PrintOut(PRINT_DEV, "raw_mouse: Raw input is now ENABLED\n");
        } else {
            PrintOut(PRINT_BAD, "raw_mouse: Raw input was requested but registration failed\n");
        }
    } else {
        raw_mouse_unregister_input(true);
        PrintOut(PRINT_DEV, "raw_mouse: Raw input is now DISABLED (using legacy cursor mode)\n");
    }
}

void create_raw_mouse_cvars(void) {
    in_mouse_raw = orig_Cvar_Get("_sofbuddy_rawmouse", "0", CVAR_SOFBUDDY_ARCHIVE, &raw_mouse_on_change);

    // Apply archived state immediately in case callback is not triggered on load.
    if (in_mouse_raw && in_mouse_raw->value != 0.0f) {
        raw_mouse_on_change(in_mouse_raw);
    }

    PrintOut(PRINT_DEV, "raw_mouse: Registered _sofbuddy_rawmouse cvar with change callback\n");
}

#endif
