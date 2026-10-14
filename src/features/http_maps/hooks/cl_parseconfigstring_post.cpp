#include "feature_config.h"

#if FEATURE_HTTP_MAPS

#include "../shared.h"

void http_maps_cl_parseconfigstring_post_callback(void)
{
	if (!http_maps_should_run_on_configstring_post()) return;
	http_maps_on_parse_configstring_post();
}

#endif // FEATURE_HTTP_MAPS
