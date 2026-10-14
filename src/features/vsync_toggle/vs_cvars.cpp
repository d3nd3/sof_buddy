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
cvar_t *vid_fullscreen = NULL;
cvar_t *gl_displayrefresh = NULL;

// cl_maxfps resolved locally (not via media_timers) so this feature works with any feature
// mix; the name is prefixed to avoid colliding with media_timers' global of the same name.
cvar_t *vs_cl_maxfps = NULL;

// Display readout state shown on F12 -> CPU (see vs_display.cpp).
cvar_t *_sb_internal_display_info = NULL;
// "1" while the cap sits below the display with VSync on (RMF <cinclude> flag).
cvar_t *_sb_internal_fps_cap_mismatch = NULL;
// Generated refresh-request picker row (see vs_display.cpp).
cvar_t *_sb_internal_display_modes_rmf = NULL;

/*
	Create and register required cvars for vsync behavior
	Called after ref.dll is loaded to ensure engine CVars are available
*/
void create_vsync_cvars(void) {
	SOFBUDDY_ASSERT(detour_Cvar_Get::oCvar_Get != nullptr);
	
	gl_swapinterval = detour_Cvar_Get::oCvar_Get("gl_swapinterval", "0", 0, NULL);
	vid_ref = detour_Cvar_Get::oCvar_Get("vid_ref", "gl", 0, NULL);
	// Same spec as exe VID_Init ("1", archive).
	vid_fullscreen = detour_Cvar_Get::oCvar_Get("vid_fullscreen", "1", 1, NULL);
	// Same spec as ref_gl R_Register ("0", archive), so resolving early is harmless.
	gl_displayrefresh = detour_Cvar_Get::oCvar_Get("gl_displayrefresh", "0", 1, NULL);
	// Same spec as exe CL_InitLocal. No callback: media_timers owns the tracking one.
	vs_cl_maxfps = detour_Cvar_Get::oCvar_Get("cl_maxfps", "30", 0, NULL);
	_sb_internal_display_info = detour_Cvar_Get::oCvar_Get("_sb_internal_display_info", "unknown", 0, NULL);
	_sb_internal_fps_cap_mismatch = detour_Cvar_Get::oCvar_Get("_sb_internal_fps_cap_mismatch", "0", 0, NULL);
	SOFBUDDY_ASSERT(gl_swapinterval != nullptr);
	SOFBUDDY_ASSERT(vid_ref != nullptr);
	SOFBUDDY_ASSERT(gl_displayrefresh != nullptr);
	SOFBUDDY_ASSERT(vs_cl_maxfps != nullptr);

	static bool display_refresh_command_registered = false;
	if (!display_refresh_command_registered && detour_Cmd_AddCommand::oCmd_AddCommand) {
		detour_Cmd_AddCommand::oCmd_AddCommand(
			const_cast<char*>("sofbuddy_display_refresh"), Cmd_SoFBuddy_Display_Refresh_f);
		display_refresh_command_registered = true;
	}

	sofbuddy_refresh_display_info();
}

#endif // FEATURE_VSYNC_TOGGLE


