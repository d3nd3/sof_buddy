#include "feature_config.h"

#if FEATURE_HTTP_MAPS

#include "util.h"
#include "../shared.h"

void http_maps_PostCvarInit(void)
{
	PrintOut(PRINT_LOG, "http_maps: Registering CVars\n");
	create_http_maps_cvars();
}

#endif // FEATURE_HTTP_MAPS
