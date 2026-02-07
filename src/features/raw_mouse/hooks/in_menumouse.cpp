#include "feature_config.h"

#if FEATURE_RAW_MOUSE

#include "../shared.h"

void in_menumouse_callback(void* cvar1, void* cvar2)
{
    (void)cvar1;
    (void)cvar2;

    if (!raw_mouse_is_enabled()) {
        return;
    }

    raw_mouse_consume_deltas();
}

#endif // FEATURE_RAW_MOUSE
