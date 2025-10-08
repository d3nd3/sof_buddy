/*
	VSync Toggle Fix
	
	This feature ensures that the gl_swapinterval cvar (vsync) is properly 
	applied when the video renderer changes by triggering its modified flag.
*/

#include "../../../hdr/feature_config.h"

#if FEATURE_VSYNC_TOGGLE

#include "../../../hdr/shared_hook_manager.h"
#include "../../../hdr/sof_compat.h"
#include "../../../hdr/util.h"

// Engine CVars are defined in cvars.cpp
extern cvar_t * vid_ref;
extern cvar_t * gl_swapinterval;
extern void create_vsync_cvars(void);

// Forward declarations
static void vsync_on_postcvarinit();
static void vsync_pre_vid_checkchanges();

// Hook registrations placed after function declarations for visibility
REGISTER_SHARED_HOOK_CALLBACK(PreCvarInit, vsync_toggle, vsync_on_postcvarinit, 50, Post);
REGISTER_SHARED_HOOK_CALLBACK(VID_CheckChanges, vsync_toggle, vsync_pre_vid_checkchanges, 50, Pre);

// Initialize cvars when the renderer DLL has loaded
static void vsync_on_postcvarinit()
{
    create_vsync_cvars();
}

// Pre-dispatch callback: ensure vsync will be applied during renderer switch
static void vsync_pre_vid_checkchanges()
{
	// Trigger gl_swapinterval->modified for R_Init() -> GL_SetDefaultState()
    if (vid_ref && vid_ref->modified && gl_swapinterval) {
		gl_swapinterval->modified = true;
		PrintOut(PRINT_LOG, "vsync_toggle: Triggered gl_swapinterval refresh on vid_ref change\n");
	}
}


#endif // FEATURE_VSYNC_TOGGLE