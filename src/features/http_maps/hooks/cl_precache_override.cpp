#include "feature_config.h"

#if FEATURE_HTTP_MAPS

#include "generated_detours.h"
#include "util.h"
#include "../shared.h"

void cl_precache_http_maps_override_callback(detour_CL_Precache_f::tCL_Precache_f original)
{
	PrintOut(PRINT_LOG, "http_maps: cl_precache_override_callback entered\n");
	http_maps_try_begin_precache(original);
}

#endif // FEATURE_HTTP_MAPS
