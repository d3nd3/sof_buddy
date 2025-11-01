/*
	VSync Toggle Fix
	
	This feature ensures that the gl_swapinterval cvar (vsync) is properly 
	applied when the video renderer changes by triggering its modified flag.
*/

#include "feature_config.h"

#if FEATURE_VSYNC_TOGGLE

#include "shared_hook_manager.h"
#include "sof_compat.h"
#include "util.h"

// Engine CVars are defined in cvars.cpp
extern cvar_t * vid_ref;
extern cvar_t * gl_swapinterval;
extern void create_vsync_cvars(void);

// Forward declarations
static void vsync_on_postcvarinit();
static void vsync_pre_vid_checkchanges();

// Hook registrations placed after function declarations for visibility
REGISTER_SHARED_HOOK_CALLBACK(RefDllLoaded, vsync_toggle, vsync_on_postcvarinit, 50, Post);
REGISTER_SHARED_HOOK_CALLBACK(VID_CheckChanges, vsync_toggle, vsync_pre_vid_checkchanges, 50, Pre);

// Initialize cvars when the renderer DLL has loaded
static void vsync_on_postcvarinit()
{
	/*
		vid_ref (VID_Init) and 
		gl_swapinterval
		  (VID_Init->VID_CheckChanges->VID_LoadRefresh->R_Init(After LoadLibrary)->R_Register) 
		are both created with CVAR_ARCHIVE flag.

	*/
    create_vsync_cvars();
}

// Pre-dispatch callback: ensure vsync will be applied during renderer switch
static void vsync_pre_vid_checkchanges()
{
	/*
		Cbuf_Execute is called by:
			QCommon_Init and CL_Init
			CL_Frame -> CL_ReadPackets -> CL_ParseServerMessage -> case svc_serverdata
			CL_Frame -> void CL_SendCommand (void)

		VID_CheckChanges is called by:
			CL_Frame
			VID_Init
		This bug is only noticeable when the user changes gl_swapinterval _AND_ the change
		 is processed by R_BeginFrame, then sometime in the future a vid_restart or gl_mode
		  change is performed.

		GL_UpdateSwapInterval() in q2 is called in 2 places:
		- R_Init() -> GL_SetDefaultState()
		- SCR_UpdateScreen->R_BeginFrame
		When you change gl_swapinterval, modified becomes false
		then vid_restart doesn't re-apply it on next vid change.

		The understood order:
			Cbuf_Execute(ReadPacketsOrSendCommand) (gl_swapinterval->modified=true)
			VID_CheckChanges(CL_Frame) (Has vid_ref->modified == true)
			SCR_UpdateScreen(GL_UpdateSwapInterval) (Apply swap, gl_swapinterval->modified=false)

		Because the VID_CheckChanges is between the Cbuf_Execute and the SCR_UpdateScreen, if the
		vid_ref->modified is applied in the same frame, it works.
	*/
	// Trigger gl_swapinterval->modified for R_Init() -> GL_SetDefaultState()
    if (vid_ref && vid_ref->modified && gl_swapinterval) {
		gl_swapinterval->modified = true;
		PrintOut(PRINT_LOG, "vsync_toggle: Triggered gl_swapinterval refresh on vid_ref change\n");
	}
}


#endif // FEATURE_VSYNC_TOGGLE