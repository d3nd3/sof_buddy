#include "feature_config.h"

#if FEATURE_HTTP_MAPS

#include "../shared.h"

void http_maps_qcommon_frame_pre_callback(int& msec)
{
	(void)msec;
	http_maps_run_deferred_continue_if_pending();
}

#endif // FEATURE_HTTP_MAPS
