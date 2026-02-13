#include "feature_config.h"

#if FEATURE_HTTP_MAPS

#include "util.h"
#include "../shared.h"

void http_maps_qcommon_frame_post_callback(int msec)
{
	(void)msec;
	static int seen = 0;
	if (++seen <= 2) return;
	http_maps_pump();
}

#endif // FEATURE_HTTP_MAPS
