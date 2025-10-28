#include "feature_config.h"

#if FEATURE_RAW_MOUSE

#include <stdint.h>
#include "sof_compat.h"
#include "util.h"

cvar_t * in_mouse_raw = NULL;

// Forward declaration of change callback (defined in hooks.cpp)
extern void raw_mouse_on_change(cvar_t *cvar);

void create_raw_mouse_cvars(void) {
    in_mouse_raw = orig_Cvar_Get("_sofbuddy_rawmouse", "1", CVAR_ARCHIVE, &raw_mouse_on_change);
    
    PrintOut(PRINT_LOG, "raw_mouse: Registered _sofbuddy_rawmouse cvar with change callback\n");
}

#endif

