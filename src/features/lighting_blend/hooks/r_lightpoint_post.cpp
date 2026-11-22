#include "feature_config.h"

#if FEATURE_LIGHTING_BLEND

#include "../shared.h"

/*
	R_LightPoint samples the raw static lightmap and returns a float colour.
	The world lightmaps are built through the tone-curve/normalization LUT, so
	entities, particles, and Ghoul models would otherwise keep the unnormalized
	raw light.  Apply the same transform to the sampled colour when map
	normalization is active.
*/
float* r_lightpoint_post_callback(float* result, float* point, float* color, int style) {
	(void)point;
	(void)style;
	lighting_apply_entity_light_lut(color);
	return result;
}

#endif // FEATURE_LIGHTING_BLEND
