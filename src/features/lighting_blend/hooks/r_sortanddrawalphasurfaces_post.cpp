#include "feature_config.h"

#if FEATURE_LIGHTING_BLEND

#include "../shared.h"

/*
	Runs after the 3D world has been submitted.  This is a safe main-thread
	point to refresh lightmap atlases without interleaving uploads with the
	world pass, and it is reached with both multitexture modes.
*/
void r_sortanddrawalphasurfaces_post_callback(void) {
	lighting_frame_tick();
}

#endif // FEATURE_LIGHTING_BLEND
