#include "feature_config.h"

#if FEATURE_LIGHTING_BLEND

#include "util.h"
#include "../shared.h"

void r_blendlightmaps_pre_callback(void) {
	if (!lightblend_target_src || !lightblend_target_dst) {
		is_blending = false;
		return;
	}

	// lightblend_src/dst already contain the effective blend for the selected
	// preset (Vanilla and the curve presets use the stock ZERO / SRC_COLOR).
	*lightblend_target_src = lightblend_src;
	*lightblend_target_dst = lightblend_dst;

	is_blending = true;
}

#endif // FEATURE_LIGHTING_BLEND
