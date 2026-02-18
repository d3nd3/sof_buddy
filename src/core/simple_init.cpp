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
#if !defined(NDEBUG) && defined(SOFBUDDY_ENABLE_CALLSITE_LOGGER)
#include "debug/parent_recorder.h"
#endif

// Original function pointers for non-lifecycle hooks
cvar_t *(*orig_Cvar_Get)(const char * name, const char * value, int flags, cvarcommand_t command) = reinterpret_cast<cvar_t*(*)(const char*,const char*,int,cvarcommand_t)>(0x20021AE0);
cvar_t *(*orig_Cvar_Set2) (char *var_name, char *value, qboolean force) = reinterpret_cast<cvar_t*(*)(char*,char*,qboolean)>(0x20021D70);
void (*orig_Cvar_SetInternal)(bool active) = reinterpret_cast<void(*)(bool)>(0x200216C0);
void ( *orig_Com_Printf)(const char * msg, ...) = NULL;
void (*orig_Com_DPrintf)(const char *fmt, ...) = NULL;
void (*orig_Qcommon_Frame) (int msec) = reinterpret_cast<void(*)(int)>(0x2001F720);
void (*orig_Qcommon_Init) (int argc, char **argv) = reinterpret_cast<void(*)(int,char**)>(0x2001F390);

typedef void (*xcommand_t)(void);
void (*orig_Cmd_AddCommand)(char *cmd_name, xcommand_t function) = reinterpret_cast<void(*)(char*,xcommand_t)>(0x20019130);
// Cmd_Argc/Cmd_Argv are in the tokenizer block, not next to Cmd_AddCommand.
int (*orig_Cmd_Argc)(void) = reinterpret_cast<int(*)(void)>(0x20018D20);
char* (*orig_Cmd_Argv)(int i) = reinterpret_cast<char*(*)(int)>(0x20018D30);

void Cmd_SoFBuddy_ListFeatures_f(void);

// Override callback for FS_InitFilesystem (PreCvarInit lifecycle)
void fs_initfilesystem_override_callback(detour_FS_InitFilesystem::tFS_InitFilesystem original) {
    if (original) {
        original();
    }
    
    PrintOut(PRINT_LOG, "=== Lifecycle: Pre-CVar Init Phase ===\n");
    
    orig_Com_Printf = reinterpret_cast<void(*)(const char*,...)>(0x2001C6E0);
    orig_Com_DPrintf = reinterpret_cast<void(*)(const char*,...)>(0x2001C8E0);
    if (!orig_Cmd_ExecuteString) orig_Cmd_ExecuteString = (void(*)(const char*))rvaToAbsExe((void*)0x194F0);

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
    orig_Cmd_AddCommand(const_cast<char*>("sofbuddy_list_features"), Cmd_SoFBuddy_ListFeatures_f);
    PrintOut(PRINT_DEV, "Registered sofbuddy_list_features command\n");

    PrintOut(PRINT_DEV, "Attempting to register sofbuddy_update command...\n");
    orig_Cmd_AddCommand(const_cast<char*>("sofbuddy_update"), Cmd_SoFBuddy_Update_f);
    orig_Cmd_AddCommand(const_cast<char*>("sofbuddy_openurl"), Cmd_SoFBuddy_OpenUrl_f);
    PrintOut(PRINT_DEV, "Registered sofbuddy_update command\n");
    
    PrintOut(PRINT_DEV, "Registering _sofbuddy_version cvar...\n");
    cvar_t* version_cvar = orig_Cvar_Get("_sofbuddy_version", SOFBUDDY_VERSION, CVAR_NOSET, NULL);
    if (version_cvar) {
        orig_Cvar_Set2(const_cast<char*>("_sofbuddy_version"), const_cast<char*>(SOFBUDDY_VERSION), true);
    }
    PrintOut(PRINT_DEV, "Registered _sofbuddy_version cvar with value: %s\n", SOFBUDDY_VERSION);

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


// Feature list command implementation
// WARNING: When adding a new feature to features/FEATURES.txt, you MUST also
// add a corresponding entry below (between lines ~60-145). Search for similar
// entries and add your feature with the same pattern using FEATURE_XXX macro.
// TODO: Automate this using X-macro pattern to prevent manual sync issues.
void Cmd_SoFBuddy_ListFeatures_f(void) {
    if (!orig_Com_Printf) return;
    
    PrintOut(PRINT_DEV, "=== SoF Buddy Compiled Features ===\n");
    
    // Include the feature config to check which features are enabled
    #include "feature_config.h"
    
    int feature_count = 0;
    int total_features = 0;
    
    #if FEATURE_MEDIA_TIMERS
    PrintOut(PRINT_DEV, P_GREEN "[ON] " P_WHITE "media_timers\n");
    feature_count++;
    #else
    PrintOut(PRINT_DEV, P_RED "[OFF] " P_WHITE "media_timers\n");
    #endif
    total_features++;
    
    #if FEATURE_TEXTURE_MAPPING_MIN_MAG
    PrintOut(PRINT_DEV, P_GREEN "[ON] " P_WHITE "texture_mapping_min_mag\n");
    feature_count++;
    #else
    PrintOut(PRINT_DEV, P_RED "[OFF] " P_WHITE "texture_mapping_min_mag\n");
    #endif
    total_features++;
    
    #if FEATURE_SCALED_UI_BASE || FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_MENU
    PrintOut(PRINT_DEV, P_GREEN "[ON] " P_WHITE "scaled_ui (base/con/hud/menu)\n");
    feature_count++;
    #else
    PrintOut(PRINT_DEV, P_RED "[OFF] " P_WHITE "scaled_ui (base/con/hud/menu)\n");
    #endif
    total_features++;
    
    #if FEATURE_HD_TEXTURES
    PrintOut(PRINT_DEV, P_GREEN "[ON] " P_WHITE "hd_textures\n");
    feature_count++;
    #else
    PrintOut(PRINT_DEV, PRED "[OFF] " P_WHITE "hd_textures\n");
    #endif
    total_features++;
    
    #if FEATURE_VSYNC_TOGGLE
    PrintOut(PRINT_DEV, P_GREEN "[ON] " P_WHITE "vsync_toggle\n");
    feature_count++;
    #else
    PrintOut(PRINT_DEV, P_RED "[OFF] " P_WHITE "vsync_toggle\n");
    #endif
    total_features++;
    
    #if FEATURE_LIGHTING_BLEND
    PrintOut(PRINT_DEV, P_GREEN "[ON] " P_WHITE "lighting_blend\n");
    feature_count++;
    #else
    PrintOut(PRINT_DEV, P_RED "[OFF] " P_WHITE "lighting_blend\n");
    #endif
    total_features++;
    
    #if FEATURE_TEAMICONS_OFFSET
    PrintOut(PRINT_DEV, P_GREEN "[ON] " P_WHITE "teamicons_offset\n");
    feature_count++;
    #else
    PrintOut(PRINT_DEV, P_RED "[OFF] " P_WHITE "teamicons_offset\n");
    #endif
    total_features++;

    #if FEATURE_HTTP_MAPS
    PrintOut(PRINT_DEV, P_GREEN "[ON] " P_WHITE "http_maps\n");
    feature_count++;
    #else
    PrintOut(PRINT_DEV, P_RED "[OFF] " P_WHITE "http_maps\n");
    #endif
    total_features++;

    #if FEATURE_INTERNAL_MENUS
    PrintOut(PRINT_DEV, P_GREEN "[ON] " P_WHITE "internal_menus\n");
    feature_count++;
    #else
    PrintOut(PRINT_DEV, P_RED "[OFF] " PWHITE "internal_menus\n");
    #endif
    total_features++;
    
    #if FEATURE_NEW_SYSTEM_BUG
    PrintOut(PRINT_DEV, P_GREEN "[ON] " P_WHITE "new_system_bug\n");
    feature_count++;
    #else
    PrintOut(PRINT_DEV, P_RED "[OFF] " P_WHITE "new_system_bug\n");
    #endif
    total_features++;
    
    #if FEATURE_CONSOLE_PROTECTION
    PrintOut(PRINT_DEV, P_GREEN "[ON] " P_WHITE "console_protection\n");
    feature_count++;
    #else
    PrintOut(PRINT_DEV, P_RED "[OFF] " P_WHITE "console_protection\n");
    #endif
    total_features++;
    
    #if FEATURE_CL_MAXFPS_SINGLEPLAYER
    PrintOut(PRINT_DEV, P_GREEN "[ON] " P_WHITE "cl_maxfps_singleplayer\n");
    feature_count++;
    #else
    PrintOut(PRINT_DEV, P_RED "[OFF] " P_WHITE "cl_maxfps_singleplayer\n");
    #endif
    total_features++;

    #if FEATURE_CBUF_LIMIT_INCREASE
    PrintOut(PRINT_DEV, P_GREEN "[ON] " P_WHITE "cbuf_limit_increase\n");
    feature_count++;
    #else
    PrintOut(PRINT_DEV, P_RED "[OFF] " P_WHITE "cbuf_limit_increase\n");
    #endif
    total_features++;
    
    #if FEATURE_RAW_MOUSE
    PrintOut(PRINT_DEV, P_GREEN "[ON] " P_WHITE "raw_mouse\n");
    feature_count++;
    #else
    PrintOut(PRINT_DEV, P_RED "[OFF] " P_WHITE "raw_mouse\n");
    #endif
    total_features++;
    
    PrintOut(PRINT_DEV, "Total: " P_GREEN "%d" P_WHITE " active, " P_RED "%d" P_WHITE " disabled (%d total)\n",
             feature_count, total_features - feature_count, total_features);
    PrintOut(PRINT_DEV, "===============================\n");
}



// Earliest initialization function called from DllMain
void lifecycle_EarlyStartup(void)
{
    #ifdef GDB
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
    
    // Initialize detours targeting SoF.exe (0x200xxxxx addresses) only
    PrintOut(PRINT_LOG, "=== Initializing SoF.exe detours ===\n");
    PrintOut(PRINT_LOG, "Found %zu SoF.exe detours to apply\n", DetourSystem::Instance().GetDetourCount(DetourModule::SofExe));
    DetourSystem::Instance().ApplyExeDetours();
    PrintOut(PRINT_LOG, "=== SoF.exe detour initialization complete ===\n");
    PrintOut(PRINT_LOG, "\n");
    PrintOut(PRINT_LOG, "\n");
    
    // Cmd_AddCommand is now hardcoded at 0x20019130
    PrintOut(PRINT_LOG, "Cmd_AddCommand hardcoded at 0x%p\n", orig_Cmd_AddCommand);
    
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
