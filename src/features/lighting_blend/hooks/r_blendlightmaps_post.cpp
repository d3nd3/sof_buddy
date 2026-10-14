#include "feature_config.h"

#if FEATURE_LIGHTING_BLEND

#include "util.h"
#include "../shared.h"

void r_blendlightmaps_post_callback(void) {
	is_blending = false;
}

#endif // FEATURE_LIGHTING_BLEND

