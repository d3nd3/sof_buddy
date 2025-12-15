#include "feature_config.h"

#if FEATURE_VSYNC_TOGGLE

#include "sof_compat.h"
#include "util.h"
#include "../shared.h"

void vsync_pre_vid_checkchanges()
{
	// Trigger gl_swapinterval->modified for R_Init() -> GL_SetDefaultState()
    if (vid_ref && vid_ref->modified && gl_swapinterval) {
		gl_swapinterval->modified = true;
		PrintOut(PRINT_LOG, "vsync_toggle: Triggered gl_swapinterval refresh on vid_ref change\n");
	}
}

#endif // FEATURE_VSYNC_TOGGLE

