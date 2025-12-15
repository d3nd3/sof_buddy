#include "feature_config.h"

#if FEATURE_LIGHTING_BLEND

#include "util.h"
#include "../shared.h"

#define GL_DST_COLOR                      0x0306
#define GL_SRC_COLOR                      0x0300

void r_blendlightmaps_pre_callback(void) {
	if ( _sofbuddy_lighting_overbright->value == 1.0f ) {
		PrintOut(PRINT_LOG, "Using overbright lighting\n");
		*lightblend_target_src = GL_DST_COLOR;
		*lightblend_target_dst = GL_SRC_COLOR;
	} else {
		*lightblend_target_src = lightblend_src;
		*lightblend_target_dst = lightblend_dst;	
	}
	
	is_blending = true;
}

#endif // FEATURE_LIGHTING_BLEND

