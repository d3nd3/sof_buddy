/*
	cl_maxfps cvar to work in singleplayer (makes _sp_cl_frame_delay obsolete)
	
	Uses detour/hook to sp_Sys_Mil to set _sp_cl_frame_delay to 0 before calling original.
	Also fixes the black loading screen issue after cinematics by hooking CinematicFreeze
	in game.dll when it loads/reloads.
*/

#include "feature_config.h"

#if FEATURE_CL_MAXFPS_SINGLEPLAYER

#include <windows.h>
#include "feature_macro.h"
#include "shared_hook_manager.h"
#include "util.h"
#include "sof_buddy.h"
#include "DetourXS/detourxs.h"

static void cl_maxfps_EarlyStartup(void);

REGISTER_SHARED_HOOK_CALLBACK(EarlyStartup, cl_maxfps_singleplayer, cl_maxfps_EarlyStartup, 70, Post);
REGISTER_HOOK(CinematicFreeze, (void*)0x75190, GameDll, void, __cdecl, bool bEnable);


// sp_Sys_Mil hook variables (kept for compatibility)
static int (*sp_Sys_Mil)(void) = NULL;
static int (*orig_sp_Sys_Mil)(void) = NULL;

/*
	sp_Sys_Mil hook implementation
	Sets _sp_cl_frame_delay to 0 before calling the original function
*/
int hksp_Sys_Mil(void)
{
	if (o_sofplus) {
		*(int*)rvaToAbsSoFPlus((void*)0x331BC) = 0x00;
	}

    if (orig_sp_Sys_Mil) return orig_sp_Sys_Mil();
    return 0;
}

/*
	Early startup callback - Apply memory patches to exe

	We nopped the jnz instruction, so the singleplayer
	does not enter the client frame code without limits.
	This causes
*/
static void cl_maxfps_EarlyStartup(void)
{
    PrintOut(PRINT_LOG, "cl_maxfps_singleplayer: Applying early patches...\n");
    
    WriteByte(rvaToAbsExe((void*)0x0000D973), 0x90);
    WriteByte(rvaToAbsExe((void*)0x0000D974), 0x90);

    PrintOut(PRINT_LOG, "cl_maxfps_singleplayer: sp_Sys_Mil hook registered automatically\n");
	//ensure _sp_cl_frame_delay is set to 0
    if (o_sofplus && orig_sp_Sys_Mil == NULL) {
        void* target = rvaToAbsSoFPlus((void*)0xFA60);
        void* tramp = DetourCreate(target, (LPVOID)hksp_Sys_Mil, DETOUR_TYPE_JMP, DETOUR_LEN_AUTO);
        if (tramp) {
            orig_sp_Sys_Mil = (int(*)(void))tramp;
            PrintOut(PRINT_LOG, "cl_maxfps_singleplayer: Direct detour applied: sp_Sys_Mil -> %p\n", tramp);
        } else {
            PrintOut(PRINT_BAD, "cl_maxfps_singleplayer: Failed to detour sp_Sys_Mil at %p\n", target);
        }
    }
    
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

	#if 0
	char * game_cinematicfreeze = (char*)rvaToAbsGame((void*)0x0015D8D5);
	char before = *game_cinematicfreeze;

	oCinematicFreeze(bEnable);
	
	char after = *game_cinematicfreeze;
	//Did cinematicFreeze change?
	if (before != after) {
		//Ensure cl_ps_cinematicFreeze is set instantly.
		char * cl_ps_cinematicfreeze = (char*)rvaToAbsExe((void*)0x001E7F5B);
		*cl_ps_cinematicfreeze = after;
		PrintOut(PRINT_LOG, "cl_maxfps_singleplayer: CinematicFreeze state synchronized: %s\n", after ? "enabled" : "disabled");
	}
	#else
	oCinematicFreeze(bEnable);
	char * cl_ps_cinematicfreeze = (char*)rvaToAbsExe((void*)0x001E7F5B);
	*cl_ps_cinematicfreeze = bEnable;
	#endif
}

#endif // FEATURE_CL_MAXFPS_SINGLEPLAYER