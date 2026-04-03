#include "feature_config.h"

#if FEATURE_RAW_MOUSE

#include <stdint.h>
#include "sof_compat.h"
#include "util.h"
#include <windows.h>
#include "shared.h"
#include "generated_detours.h"

cvar_t * in_mouse_raw = NULL;

void raw_mouse_on_change(cvar_t *cvar)
{
    if (!cvar) return;
    /* Cvar_Get may invoke this callback before returning; in_mouse_raw is not
     * assigned yet, so raw_mouse_is_enabled() would read a null pointer. */
    in_mouse_raw = cvar;

    raw_mouse_reset_deltas();
    raw_mouse_center_valid = false;

    if (cvar->value != 0.0f) {
        if (!raw_mouse_api_supported()) {
            PrintOut(PRINT_BAD, "raw_mouse: Raw Input API not available; keeping legacy cursor mode\n");
            return;
        }
        raw_mouse_ensure_registered(nullptr, true);
        if (raw_mouse_registered) {
            PrintOut(PRINT_DEV, "raw_mouse: Raw input is now ENABLED\n");
        } else {
            PrintOut(PRINT_BAD,
                     "raw_mouse: Raw input was requested but registration failed\n");
        }
    } else {
        raw_mouse_unregister_input(true);
        PrintOut(PRINT_DEV, "raw_mouse: Raw input is now DISABLED (using legacy cursor mode)\n");
    }
}

void create_raw_mouse_cvars(void) {
    in_mouse_raw = detour_Cvar_Get::oCvar_Get("_sofbuddy_rawmouse", "0", CVAR_SOFBUDDY_ARCHIVE, &raw_mouse_on_change);

    PrintOut(PRINT_DEV, "raw_mouse: Registered _sofbuddy_rawmouse cvar with change callback\n");
}

#endif
