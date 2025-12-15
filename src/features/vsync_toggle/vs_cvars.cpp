/*
	VSync Toggle - CVars

	This file contains cvar declarations and registration for the vsync_toggle feature.
*/

#include "feature_config.h"

#if FEATURE_VSYNC_TOGGLE

#include "sof_buddy.h"
#include "sof_compat.h"
#include "shared.h"

// Engine CVars resolved when the renderer is loaded
cvar_t *vid_ref = NULL;
cvar_t *gl_swapinterval = NULL;

/*
	Create and register required cvars for vsync behavior
	Called after ref.dll is loaded to ensure engine CVars are available
*/
void create_vsync_cvars(void) {
	gl_swapinterval = orig_Cvar_Get("gl_swapinterval", "0", 0, NULL);
	vid_ref = orig_Cvar_Get("vid_ref", "gl", 0, NULL);
}

#endif // FEATURE_VSYNC_TOGGLE


