#include "feature_config.h"

#if FEATURE_LIGHTING_BLEND

#include "../shared.h"

/*
	R_BeginRegistration loads the BSP, builds its static lightmaps, and returns
	the new world model.  Clear the old normalization range before the build,
	then scan the new map's samples and refresh the curve LUT afterwards.
*/
void r_beginregistration_pre_callback(char*& map) {
	(void)map;
	lighting_on_map_load_begin();
}

char* r_beginregistration_post_callback(char* result, char* map) {
	(void)map;
	void* model = result ? static_cast<void*>(result)
	                     : (r_worldmodel_ptr ? *r_worldmodel_ptr : nullptr);
	lighting_on_map_loaded(model);
	return result;
}

#endif // FEATURE_LIGHTING_BLEND
