#include "feature_config.h"

#if FEATURE_HTTP_MAPS

#include "util.h"
#include "../shared.h"

void http_maps_qcommon_frame_post_callback(int msec)
{
	(void)msec;
	http_maps_pump();
	http_maps_run_deferred_continue_if_pending();
}

#endif // FEATURE_HTTP_MAPS
