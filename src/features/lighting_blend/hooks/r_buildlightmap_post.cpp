#include "feature_config.h"

#if FEATURE_LIGHTING_BLEND

#include "../shared.h"

/*
	R_BuildLightMap writes the finished 8-bit RGBA lightmap.  In the preserve
	modes the inline LUT is left at the original gamma table, and this post-hook
	applies the curve to luminance or the brightest channel and scales RGB
	together.  The rebuild path calls the original directly and applies the
	same helper itself.
*/
int r_buildlightmap_post_callback(int result, int surf, void* dest, int pitch) {
	lighting_apply_preserve_color_to_buffer(surf, dest, pitch);
	return result;
}

#endif // FEATURE_LIGHTING_BLEND
