/*
	VSync Toggle - CVars

	This file contains cvar declarations and registration for the vsync_toggle feature.
*/

#include "feature_config.h"

#if FEATURE_VSYNC_TOGGLE

#include "sof_buddy.h"
#include "sof_compat.h"
#include "util.h"
#include "shared.h"
#include "generated_detours.h"

// Engine CVars resolved when the renderer is loaded
cvar_t *vid_ref = NULL;
cvar_t *gl_swapinterval = NULL;

/*
	Create and register required cvars for vsync behavior
	Called after ref.dll is loaded to ensure engine CVars are available
*/
void create_vsync_cvars(void) {
	SOFBUDDY_ASSERT(detour_Cvar_Get::oCvar_Get != nullptr);
	
	gl_swapinterval = detour_Cvar_Get::oCvar_Get("gl_swapinterval", "0", 0, NULL);
	vid_ref = detour_Cvar_Get::oCvar_Get("vid_ref", "gl", 0, NULL);
	SOFBUDDY_ASSERT(gl_swapinterval != nullptr);
	SOFBUDDY_ASSERT(vid_ref != nullptr);
}

#endif // FEATURE_VSYNC_TOGGLE


