/*
 * Module Loaders - DLL Loading/Unloading Lifecycle
 * 
 * Detects DLL load/unload events for ref.dll, game.dll, player.dll.
 * 
 * Load: VID_LoadRefresh/Sys_GetGameApi trigger InitializeXHooks() and dispatch shared hooks
 * Unload: SV_ShutdownGameProgs and VID_LoadRefresh (when ref active) remove hooks before DLL unloads
 * 
 * Critical: Hooks must be removed BEFORE DLLs unload to prevent accessing freed memory.
 * RVAs resolved fresh on each load since DLLs reload at different addresses.
 */

#include "feature_macro.h"
#include "shared_hook_manager.h"
#include "util.h"
#include "sof_compat.h"
#include "hook_manager.h"

// Forward declarations
static qboolean hkVID_LoadRefresh(char *name);
static void* hkSys_GetGameApi(void *imports);
static void hkSV_ShutdownGameProgs(void);

// Hook VID_LoadRefresh to detect when ref.dll is loaded
REGISTER_HOOK(VID_LoadRefresh, (void*)0x00066E10, SofExe, qboolean, __cdecl, char *name);

// Hook Sys_GetGameApi to detect when game.dll is loaded  
REGISTER_HOOK(Sys_GetGameApi, (void*)0x00065F20, SofExe, void*, __cdecl, void *imports);

// Hook SV_ShutdownGameProgs to detect when gamex86.dll is about to be unloaded
REGISTER_HOOK_VOID(SV_ShutdownGameProgs, (void*)0x005CD70, SofExe, void, __cdecl);

/*
	VID_LoadRefresh Hook - Called when ref.dll is loaded
	
	This is where we dispatch RefDllLoaded callbacks so features can apply
	detours to functions in ref.dll (addresses starting with 0x5XXXXXXX)
	
	Also handles cleanup when ref.dll is being unloaded by checking if the
	ref library is still active.
*/
qboolean hkVID_LoadRefresh(char *name)
{
	// Check if ref library is still active (exe RVA 0x00403664)
	int* ref_active = (int*)rvaToAbsExe((void*)0x00403664);
	
	// If ref library is active (true), this is a reload/shutdown, cleanup hooks
	if (ref_active && *ref_active != 0) {
		PrintOut(PRINT_LOG, "=== Module Unloading: ref.dll (detected by VID_LoadRefresh) ===\n");
		HookManager::Instance().RemoveRefHooks();
		PrintOut(PRINT_LOG, "=== ref.dll Cleanup Complete ===\n");
	}
	
	PrintOut(PRINT_LOG, "=== Module Loading: ref.dll ===\n");
	
	// Call original function to load the ref.dll
	qboolean ret = oVID_LoadRefresh(name);
	
	if (ret) {
		PrintOut(PRINT_LOG, "ref.dll loaded successfully: %s\n", name ? name : "default");
		
		// Initialize hooks targeting ref.dll functions first
		PrintOut(PRINT_LOG, "=== Initializing ref.dll hooks ===\n");
		PrintOut(PRINT_LOG, "Found %zu ref.dll hooks to apply\n", HookManager::Instance().GetHookCount(HookModule::RefDll));
		HookManager::Instance().ApplyRefHooks();
		PrintOut(PRINT_LOG, "=== ref.dll hook initialization complete ===\n");
		
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
		PrintOut(PRINT_LOG, "=== Initializing game.dll hooks ===\n");
		PrintOut(PRINT_LOG, "Found %zu game.dll hooks to apply\n", HookManager::Instance().GetHookCount(HookModule::GameDll));
		HookManager::Instance().ApplyGameHooks();
		PrintOut(PRINT_LOG, "=== game.dll hook initialization complete ===\n");
		
		// Dispatch the GameDllLoaded 'event' to all features
		DISPATCH_SHARED_HOOK(GameDllLoaded, Post);
		
		PrintOut(PRINT_LOG, "=== game.dll Loading Complete ===\n");
	} else {
		PrintOut(PRINT_BAD, "Failed to load game.dll\n");
	}
	
	return ret;
}

/*
	SV_ShutdownGameProgs Hook - Called when gamex86.dll is about to be unloaded
	
	This is where we clean up hooks before the DLL is unloaded to prevent crashes
*/
void hkSV_ShutdownGameProgs(void)
{
	PrintOut(PRINT_LOG, "=== Module Unloading: gamex86.dll ===\n");
	
	HookManager& manager = HookManager::Instance();
	
	
	
	manager.RemoveGameHooks();

	if (oSV_ShutdownGameProgs) {
		oSV_ShutdownGameProgs();
	}
	
	PrintOut(PRINT_LOG, "=== gamex86.dll Cleanup Complete ===\n");
}

