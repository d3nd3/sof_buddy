/*
	Module Loading Lifecycle System
	
	Core infrastructure that handles DLL loading events and dispatches to features 
	that need to apply detours to specific modules (game.dll, ref.dll, player.dll)
	
	This is NOT a feature - it's structural code that enables the module loading
	lifecycle system for all features.
*/

#include "feature_macro.h"
#include "shared_hook_manager.h"
#include "util.h"
#include "sof_compat.h"
#include "simple_hook_init.h"

// Forward declarations
static qboolean hkVID_LoadRefresh(char *name);
static void* hkSys_GetGameApi(void *imports);

// Hook VID_LoadRefresh to detect when ref.dll is loaded
REGISTER_HOOK(VID_LoadRefresh, 0x20066E10, qboolean, __cdecl, char *name);

// Hook Sys_GetGameApi to detect when game.dll is loaded  
REGISTER_HOOK(Sys_GetGameApi, 0x20065F20, void*, __cdecl, void *imports);

/*
	VID_LoadRefresh Hook - Called when ref.dll is loaded
	
	This is where we dispatch RefDllLoaded callbacks so features can apply
	detours to functions in ref.dll (addresses starting with 0x5XXXXXXX)
*/
qboolean hkVID_LoadRefresh(char *name)
{
	PrintOut(PRINT_LOG, "=== Module Loading: ref.dll ===\n");
	
	// Call original function to load the ref.dll
	qboolean ret = oVID_LoadRefresh(name);
	
	if (ret) {
		PrintOut(PRINT_LOG, "ref.dll loaded successfully: %s\n", name ? name : "default");
		
		// Initialize hooks targeting ref.dll functions first
		InitializeRefHooks();
		
		// Then dispatch to all features that need to respond to ref.dll loading
		DISPATCH_SHARED_HOOK(RefDllLoaded, Post);
		
		PrintOut(PRINT_LOG, "=== ref.dll Loading Complete ===\n");
	} else {
		PrintOut(PRINT_BAD, "Failed to load ref.dll: %s\n", name ? name : "default");
	}
	
	return ret;
}

/*
	Sys_GetGameApi Hook - Called when game.dll is loaded
	
	This is where we dispatch GameDllLoaded callbacks so features can apply
	detours to functions in game.dll
*/
void* hkSys_GetGameApi(void *imports)
{
	PrintOut(PRINT_LOG, "=== Module Loading: game.dll ===\n");
	
	// Call original function to load the game.dll
	void *ret = oSys_GetGameApi(imports);
	
	if (ret) {
		PrintOut(PRINT_LOG, "game.dll loaded successfully (API returned: %p)\n", ret);
		
		// Initialize hooks targeting game.dll functions first
		InitializeGameHooks();
		
		// Dispatch the GameDllLoaded 'event' to all features
		DISPATCH_SHARED_HOOK(GameDllLoaded, Post);
		
		PrintOut(PRINT_LOG, "=== game.dll Loading Complete ===\n");
	} else {
		PrintOut(PRINT_BAD, "Failed to load game.dll\n");
	}
	
	return ret;
}
