/*
	cl_maxfps cvar to work in singleplayer (makes _sp_cl_frame_delay obsolete)
	
	Uses detour/hook to sp_Sys_Mil to set _sp_cl_frame_delay to 0 before calling original.
	Also fixes the black loading screen issue after cinematics by hooking CinematicFreeze
	in game.dll when it loads/reloads.
*/

#include "../../../hdr/feature_config.h"

#if FEATURE_CL_MAXFPS_SINGLEPLAYER

#include <windows.h>
#include "../../../hdr/feature_macro.h"
#include "../../../hdr/shared_hook_manager.h"
#include "../../../hdr/util.h"
#include "../../../hdr/sof_buddy.h"

// Forward declarations
static void cl_maxfps_EarlyStartup(void);

// Register for EarlyStartup to apply early patches
REGISTER_SHARED_HOOK_CALLBACK(EarlyStartup, cl_maxfps_singleplayer, cl_maxfps_EarlyStartup, 70, Post);


// Hook CinematicFreeze in game.dll - automatically applied when game.dll loads
REGISTER_HOOK(CinematicFreeze, 0x50075190, void, __cdecl, bool bEnable);

// Hook sp_Sys_Mil in sofplus.dll - automatically applied when sofplus loads
REGISTER_HOOK(sp_Sys_Mil, 0xFA60, int, __cdecl);

// sp_Sys_Mil hook variables (kept for compatibility)
static int (*sp_Sys_Mil)(void) = NULL;
static int (*orig_sp_Sys_Mil)(void) = NULL;

/*
	sp_Sys_Mil hook implementation
	Sets _sp_cl_frame_delay to 0 before calling the original function
*/
int hksp_Sys_Mil(void)
{
	// Set _sp_cl_frame_delay to 0 before calling original
	// Only write to memory if sofplus is fully initialized
	if (o_sofplus) {
		*(int*)((char*)o_sofplus + 0x331BC) = 0x00;
	}

	return osp_Sys_Mil();
}

/*
	Early startup callback - Apply memory patches to exe
*/
static void cl_maxfps_EarlyStartup(void)
{
    PrintOut(PRINT_LOG, "cl_maxfps_singleplayer: Applying early patches...\n");
    
    // Patch CL_Frame() to allow client frame limiting in singleplayer mode
    // This fixes the issue where cl_maxfps didn't work in singleplayer
    WriteByte((void*)0x2000D973, 0x90);
    WriteByte((void*)0x2000D974, 0x90);

	
    // sp_Sys_Mil hook is now automatically registered via REGISTER_HOOK macro
    PrintOut(PRINT_LOG, "cl_maxfps_singleplayer: sp_Sys_Mil hook registered automatically\n");
    
    PrintOut(PRINT_LOG, "cl_maxfps_singleplayer: Early patches applied\n");
}


/*
	CinematicFreeze Hook Implementation
	
	Fixes black loading screen after cinematics by synchronizing freeze state
	between game.dll and the main executable.
	
	This hook is automatically applied when game.dll loads via InitializeGameHooks().
*/
void hkCinematicFreeze(bool bEnable)
{
	// Get freeze state before calling original
	bool before = *(char*)0x5015D8D5 == 1 ? true : false;
	
	// Call original CinematicFreeze function
	oCinematicFreeze(bEnable);
	
	// Get freeze state after calling original
	bool after = *(char*)0x5015D8D5 == 1 ? true : false;

	// If freeze state changed, synchronize it with the exe
	if (before != after) {
		*(char*)0x201E7F5B = after ? 0x1 : 0x00;
		PrintOut(PRINT_LOG, "cl_maxfps_singleplayer: CinematicFreeze state synchronized: %s\n", after ? "enabled" : "disabled");
	}
}

#endif // FEATURE_CL_MAXFPS_SINGLEPLAYER