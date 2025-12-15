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

#include "detours.h"
#include "shared_hook_manager.h"
#include "util.h"
#include "sof_compat.h"
#include "debug/callsite_classifier.h"

#include "generated_detours.h"

// Override callback for VID_LoadRefresh (RefDllLoaded lifecycle)
qboolean vid_loadrefresh_override_callback(char const* name, detour_VID_LoadRefresh::tVID_LoadRefresh original) {
    int* ref_active = (int*)rvaToAbsExe((void*)0x00403664);
    
    if (ref_active && *ref_active != 0) {
        PrintOut(PRINT_LOG, "=== Module Unloading: ref.dll (detected by VID_LoadRefresh) ===\n");
        DetourSystem::Instance().RemoveRefDetours();
        CallsiteClassifier::invalidateModuleCache(Module::RefDll);
        PrintOut(PRINT_LOG, "=== ref.dll Cleanup Complete ===\n");
    }
    
    PrintOut(PRINT_LOG, "=== Module Loading: ref.dll ===\n");
    
    qboolean result = qboolean(0);
    if (original) {
        result = original(name);
    }
    
    if (result) {
        PrintOut(PRINT_LOG, "ref.dll loaded successfully: %s\n", name ? name : "default");
        
        CallsiteClassifier::cacheModuleLoaded(Module::RefDll);
        
        PrintOut(PRINT_LOG, "=== Initializing ref.dll detours ===\n");
        PrintOut(PRINT_LOG, "Found %zu ref.dll detours to apply\n", DetourSystem::Instance().GetDetourCount(DetourModule::RefDll));
        DetourSystem::Instance().ApplyRefDetours();
        PrintOut(PRINT_LOG, "=== ref.dll detour initialization complete ===\n");
        
        DISPATCH_SHARED_HOOK_ARGS(RefDllLoaded, Post, name);
        
        PrintOut(PRINT_LOG, "=== ref.dll Loading Complete ===\n");
    } else {
        PrintOut(PRINT_BAD, "Failed to load ref.dll: %s\n", name ? name : "default");
    }
    
    return result;
}

// Override callback for Sys_GetGameApi (GameDllLoaded lifecycle)
void* sys_getgameapi_override_callback(void* imports, detour_Sys_GetGameApi::tSys_GetGameApi original) {
    PrintOut(PRINT_LOG, "=== Module Loading: game.dll ===\n");
    
    void* result = nullptr;
    if (original) {
        result = original(imports);
    }
    
    if (result) {
        PrintOut(PRINT_LOG, "game.dll loaded successfully (API returned: %p)\n", result);
        
        CallsiteClassifier::cacheModuleLoaded(Module::GameDll);
        
        PrintOut(PRINT_LOG, "=== Initializing game.dll detours ===\n");
        PrintOut(PRINT_LOG, "Found %zu game.dll detours to apply\n", DetourSystem::Instance().GetDetourCount(DetourModule::GameDll));
        DetourSystem::Instance().ApplyGameDetours();
        PrintOut(PRINT_LOG, "=== game.dll detour initialization complete ===\n");
        
        DISPATCH_SHARED_HOOK_ARGS(GameDllLoaded, Post, imports);
        
        PrintOut(PRINT_LOG, "=== game.dll Loading Complete ===\n");
    } else {
        PrintOut(PRINT_BAD, "Failed to load game.dll\n");
    }
    
    return result;
}

void sv_shutdowngameprogs_callback(void) {
    PrintOut(PRINT_LOG, "=== Module Unloading: gamex86.dll ===\n");
    
    DetourSystem::Instance().RemoveGameDetours();
    CallsiteClassifier::invalidateModuleCache(Module::GameDll);
    
    PrintOut(PRINT_LOG, "=== gamex86.dll Cleanup Complete ===\n");
}

// Core hooks are now registered via hooks.json and generated code


