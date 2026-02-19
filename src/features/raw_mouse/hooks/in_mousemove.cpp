#include "feature_config.h"

#if FEATURE_RAW_MOUSE

#include "../shared.h"

void in_mousemove_callback(void* cmd)
{
    (void)cmd;

    if (!raw_mouse_is_enabled()) {
        return;
    }

    raw_mouse_poll();
    raw_mouse_consume_deltas();
}

#endif // FEATURE_RAW_MOUSE