#include "feature_config.h"

#if FEATURE_MORE_CROSSHAIRS

#include "../shared.h"

void more_crosshairs_qcommon_frame_post_callback(int msec) {
    (void)msec;
    more_crosshairs_pump_deferred_menu_refresh();
}

#endif
