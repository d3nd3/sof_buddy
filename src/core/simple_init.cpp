/*
 * Lifecycle Management System
 * 
 * Coordinates feature initialization at specific SoF startup phases:
 * - EarlyStartup: After DllMain, initializes exe/system hooks
 * - PreCvarInit: After filesystem init, before CVars
 * - PostCvarInit: After CVars ready, registers Buddy commands/CVars
 * 
 * Ensures proper initialization order and prevents race conditions.
 */

#include "shared_hook_manager.h"
#include "detours.h"
#include "util.h"
#include "sof_buddy.h"
#include <windows.h>

#include "debug/callsite_classifier.h"
#include "version.h"
#include "update_command.h"
#include "sofbuddy_cfg.h"
#include "generated_detours.h"
#include "generated_registrations.h"
#include "feature_config.h"
#include "feature_list.inc"
#include "runtime_features.h"
#if !defined(NDEBUG) && defined(SOFBUDDY_ENABLE_CALLSITE_LOGGER)
#include "debug/parent_recorder.h"
#endif

void Cmd_SoFBuddy_ListFeatures_f(void);

// Override callback for FS_InitFilesystem (PreCvarInit lifecycle)
void fs_initfilesystem_override_callback(detour_FS_InitFilesystem::tFS_InitFilesystem original) {
    if (original) {
        original();
    }
    
    PrintOut(PRINT_LOG, "=== Lifecycle: Pre-CVar Init Phase ===\n");

    sofbuddy_cfg_exec_startup();
    
    DISPATCH_SHARED_HOOK(PreCvarInit, Post);
    
    PrintOut(PRINT_LOG, "=== Pre-CVar Init Phase Complete ===\n");
    PrintOut(PRINT_LOG, "\n");
    PrintOut(PRINT_LOG, "\n");
}

// Override callback for Cbuf_AddLateCommands (PostCvarInit lifecycle)
qboolean cbuf_addlatecommands_override_callback(detour_Cbuf_AddLateCommands::tCbuf_AddLateCommands original) {
    qboolean ret = qboolean(0);
    
    if (original) {
        ret = original();
    }
    
    PrintOut(PRINT_LOG, "=== Lifecycle: Post-CVar Init Phase ===\n");
    
    PrintOut(PRINT_DEV, "Attempting to register sofbuddy_list_features command...\n");
    detour_Cmd_AddCommand::oCmd_AddCommand(const_cast<char*>("sofbuddy_list_features"), Cmd_SoFBuddy_ListFeatures_f);
    PrintOut(PRINT_DEV, "Registered sofbuddy_list_features command\n");

    PrintOut(PRINT_DEV, "Attempting to register sofbuddy_update command...\n");
    detour_Cmd_AddCommand::oCmd_AddCommand(const_cast<char*>("sofbuddy_update"), Cmd_SoFBuddy_Update_f);
    detour_Cmd_AddCommand::oCmd_AddCommand(const_cast<char*>("sofbuddy_update_install"), Cmd_SoFBuddy_UpdateInstall_f);
    detour_Cmd_AddCommand::oCmd_AddCommand(const_cast<char*>("sofbuddy_openurl"), Cmd_SoFBuddy_OpenUrl_f);
    PrintOut(PRINT_DEV, "Registered sofbuddy_update command\n");
    
    PrintOut(PRINT_DEV, "Registering _sb_internal_version cvar...\n");
    cvar_t* version_cvar = detour_Cvar_Get::oCvar_Get("_sb_internal_version", SOFBUDDY_VERSION, CVAR_NOSET, NULL);
    if (version_cvar) {
        detour_Cvar_Set2::oCvar_Set2(const_cast<char*>("_sb_internal_version"), const_cast<char*>(SOFBUDDY_VERSION), true);
    }
    PrintOut(PRINT_DEV, "Registered _sb_internal_version cvar with value: %s\n", SOFBUDDY_VERSION);

    PrintOut(PRINT_DEV, "Registering updater state cvars...\n");
    sofbuddy_update_init();
    sofbuddy_update_maybe_check_startup();
    PrintOut(PRINT_DEV, "Updater state cvars ready\n");
    
    DISPATCH_SHARED_HOOK(PostCvarInit, Post);
    sofbuddy_cfg_save_now();
    
    PrintOut(PRINT_LOG, "=== Post-CVar Init Phase Complete ===\n");
    PrintOut(PRINT_LOG, "\n");
    PrintOut(PRINT_LOG, "\n");
    Cmd_SoFBuddy_ListFeatures_f();

    return ret;
}

// Core hooks are now registered via hooks.json


void Cmd_SoFBuddy_ListFeatures_f(void) {
    if (!detour_Com_Printf::oCom_Printf) return;
    
    PrintOut(PRINT_DEV, "=== SoF Buddy Features ===\n");
    int feature_count = 0;
    int total_features = 0;
    #define PRINT_FEATURE(macro, name, label) do { \
        const bool enabled = RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_##macro); \
        PrintOut(PRINT_DEV, "%s[%s] " P_WHITE "%s\n", \
                 enabled ? P_GREEN : P_RED, enabled ? "ON" : "OFF", name); \
        feature_count += enabled ? 1 : 0; \
        total_features++; \
    } while (0);
    FEATURE_LIST(PRINT_FEATURE)
    #undef PRINT_FEATURE

    PrintOut(PRINT_DEV, "Total: " P_GREEN "%d" P_WHITE " active, " P_RED "%d" P_WHITE " disabled (%d total)\n",
             feature_count, total_features - feature_count, total_features);
    PrintOut(PRINT_DEV, "===============================\n");
}



// Earliest initialization function called from DllMain
void lifecycle_EarlyStartup(void)
{
    RuntimeFeatures::Load();

    #if defined(GDB) && !defined(NDEBUG)
    extern void sofbuddy_debug_breakpoint(void);
    sofbuddy_debug_breakpoint();
    #endif
    
    PrintOut(PRINT_LOG, "=== Lifecycle: Early Startup Phase ===\n");
    
    DetourSystem::Instance().ProcessDeferredRegistrations();
    PrintOut(PRINT_LOG, "=== Processed deferred detour registrations ===\n");
    PrintOut(PRINT_LOG, "\n");
    PrintOut(PRINT_LOG, "\n");
    
    RegisterAllFeatureHooks();
    PrintOut(PRINT_LOG, "=== Feature hook registrations complete ===\n");
    PrintOut(PRINT_LOG, "\n");
    PrintOut(PRINT_LOG, "\n");
    
    // Initialize system DLL hooks
    PrintOut(PRINT_LOG, "=== Initializing system DLL hooks ===\n");
    PrintOut(PRINT_LOG, "Found %zu system DLL detours to apply\n", DetourSystem::Instance().GetDetourCount(DetourModule::Unknown));
    DetourSystem::Instance().ApplySystemDetours();
    PrintOut(PRINT_LOG, "=== system DLL detour initialization complete ===\n");
    PrintOut(PRINT_LOG, "\n");
    PrintOut(PRINT_LOG, "\n");

    RegisterPointerOnlyFunctions_Unknown();
    
    // Initialize detours targeting SoF.exe (0x200xxxxx addresses) only
    PrintOut(PRINT_LOG, "=== Initializing SoF.exe detours ===\n");
    PrintOut(PRINT_LOG, "Found %zu SoF.exe detours to apply\n", DetourSystem::Instance().GetDetourCount(DetourModule::SofExe));
    DetourSystem::Instance().ApplyExeDetours();
    PrintOut(PRINT_LOG, "=== SoF.exe detour initialization complete ===\n");
    PrintOut(PRINT_LOG, "\n");
    PrintOut(PRINT_LOG, "\n");

    RegisterPointerOnlyFunctions_SofExe();

    // Dispatch to all features registered for early startup
    DISPATCH_SHARED_HOOK(EarlyStartup, Post);

    CallsiteClassifier::initialize("sof_buddy/funcmaps");
    PrintOut(PRINT_LOG, "=== Caller classification maps initialized ===\n");
    PrintOut(PRINT_LOG, "\n");
    PrintOut(PRINT_LOG, "\n");
    #if !defined(NDEBUG) && defined(SOFBUDDY_ENABLE_CALLSITE_LOGGER)
    ParentRecorder::Instance().initialize("sof_buddy/func_parents");
    #endif

    #ifdef NOP_SOFPLUS_INIT_FUNCTION
    extern void* o_sofplus;
    if ( o_sofplus ) {
        BOOL (*sofplusEntry)(void) = (BOOL(*)(void))((char*)o_sofplus + 0xF590);
        BOOL result = sofplusEntry();
    }
    #endif

    PrintOut(PRINT_LOG, "=== Early Startup Phase Complete ===\n");
}
