#include "feature_config.h"

#if FEATURE_HTTP_MAPS

#include "../shared.h"

void http_maps_reconnect_f_pre_callback(void)
{
	http_maps_reset_state();
}

#endif
