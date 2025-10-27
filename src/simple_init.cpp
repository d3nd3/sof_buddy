// Lifecycle callback system for feature initialization
// #include "../hdr/hook_manager.h"
#include "shared_hook_manager.h"
#include "feature_macro.h"
#include "util.h"
#include "sof_buddy.h"
#include <windows.h>

#include "simple_hook_init.h"
#include "callsite_classifier.h"
#include "parent_recorder.h"
#include "version.h"

// Original function pointers for non-lifecycle hooks
cvar_t *(*orig_Cvar_Get)(const char * name, const char * value, int flags, cvarcommand_t command) = reinterpret_cast<cvar_t*(*)(const char*,const char*,int,cvarcommand_t)>(0x20021AE0);
cvar_t *(*orig_Cvar_Set2) (char *var_name, char *value, qboolean force) = reinterpret_cast<cvar_t*(*)(char*,char*,qboolean)>(0x20021D70);
void (*orig_Cvar_SetInternal)(bool active) = reinterpret_cast<void(*)(bool)>(0x200216C0);
void ( *orig_Com_Printf)(const char * msg, ...) = NULL;
void (*orig_Qcommon_Frame) (int msec) = reinterpret_cast<void(*)(int)>(0x2001F720);
void (*orig_Qcommon_Init) (int argc, char **argv) = reinterpret_cast<void(*)(int,char**)>(0x2001F390);

// Command system function pointer - hardcoded address like other SoF.exe functions
typedef void (*xcommand_t)(void);
void (*orig_Cmd_AddCommand)(char *cmd_name, xcommand_t function) = reinterpret_cast<void(*)(char*,xcommand_t)>(0x20019130);

// Hook FS_InitFilesystem for pre-cvar initialization phase
REGISTER_HOOK_VOID(FS_InitFilesystem, 0x20026980, void, __cdecl);

// Hook Cbuf_AddLateCommands for post-cvar initialization phase  
REGISTER_HOOK(Cbuf_AddLateCommands, 0x20018740, qboolean, __cdecl);

// Lifecycle hook implementations
void hkFS_InitFilesystem(void) {
    oFS_InitFilesystem(); //processes cddir user etc setting dirs

    PrintOut(PRINT_LOG, "=== Lifecycle: Pre-CVar Init Phase ===\n");
    
    //Allow PrintOut to use Com_Printf now.
    orig_Com_Printf = reinterpret_cast<void(*)(const char*,...)>(0x2001C6E0);

    // Dispatch to all features registered for pre-cvar initialization
    DISPATCH_SHARED_HOOK(PreCvarInit, Post);
    
    PrintOut(PRINT_LOG, "=== Pre-CVar Init Phase Complete ===\n");
}


// Feature list command implementation
void Cmd_SoFBuddy_ListFeatures_f(void) {
    if (!orig_Com_Printf) return;
    
    orig_Com_Printf("=== SoF Buddy Compiled Features ===\n");
    
    // Include the feature config to check which features are enabled
    #include "feature_config.h"
    
    int feature_count = 0;
    int total_features = 0;
    
    #if FEATURE_MEDIA_TIMERS
    orig_Com_Printf(P_GREEN "[ON] " P_WHITE "media_timers\n");
    feature_count++;
    #else
    orig_Com_Printf(P_RED "[OFF] " P_WHITE "media_timers\n");
    #endif
    total_features++;
    
    #if FEATURE_TEXTURE_MAPPING_MIN_MAG
    orig_Com_Printf(P_GREEN "[ON] " P_WHITE "texture_mapping_min_mag\n");
    feature_count++;
    #else
    orig_Com_Printf(P_RED "[OFF] " P_WHITE "texture_mapping_min_mag\n");
    #endif
    total_features++;
    
    #if FEATURE_UI_SCALING
    orig_Com_Printf(P_GREEN "[ON] " P_WHITE "ui_scaling\n");
    feature_count++;
    #else
    orig_Com_Printf(P_RED "[OFF] " P_WHITE "ui_scaling\n");
    #endif
    total_features++;
    
    #if FEATURE_HD_TEXTURES
    orig_Com_Printf(P_GREEN "[ON] " P_WHITE "hd_textures\n");
    feature_count++;
    #else
    orig_Com_Printf(P_RED "[OFF] " P_WHITE "hd_textures\n");
    #endif
    total_features++;
    
    #if FEATURE_VSYNC_TOGGLE
    orig_Com_Printf(P_GREEN "[ON] " P_WHITE "vsync_toggle\n");
    feature_count++;
    #else
    orig_Com_Printf(P_RED "[OFF] " P_WHITE "vsync_toggle\n");
    #endif
    total_features++;
    
    #if FEATURE_LIGHTING_BLEND
    orig_Com_Printf(P_GREEN "[ON] " P_WHITE "lighting_blend\n");
    feature_count++;
    #else
    orig_Com_Printf(P_RED "[OFF] " P_WHITE "lighting_blend\n");
    #endif
    total_features++;
    
    #if FEATURE_TEAMICONS_OFFSET
    orig_Com_Printf(P_GREEN "[ON] " P_WHITE "teamicons_offset\n");
    feature_count++;
    #else
    orig_Com_Printf(P_RED "[OFF] " P_WHITE "teamicons_offset\n");
    #endif
    total_features++;
    
    #if FEATURE_NEW_SYSTEM_BUG
    orig_Com_Printf(P_GREEN "[ON] " P_WHITE "new_system_bug\n");
    feature_count++;
    #else
    orig_Com_Printf(P_RED "[OFF] " P_WHITE "new_system_bug\n");
    #endif
    total_features++;
    
    #if FEATURE_CONSOLE_PROTECTION
    orig_Com_Printf(P_GREEN "[ON] " P_WHITE "console_protection\n");
    feature_count++;
    #else
    orig_Com_Printf(P_RED "[OFF] " P_WHITE "console_protection\n");
    #endif
    total_features++;
    
    #if FEATURE_CL_MAXFPS_SINGLEPLAYER
    orig_Com_Printf(P_GREEN "[ON] " P_WHITE "cl_maxfps_singleplayer\n");
    feature_count++;
    #else
    orig_Com_Printf(P_RED "[OFF] " P_WHITE "cl_maxfps_singleplayer\n");
    #endif
    total_features++;
    
    
    orig_Com_Printf("Total: " P_GREEN "%d" P_WHITE " active, " P_RED "%d" P_WHITE " disabled (%d total)\n", 
                    feature_count, total_features - feature_count, total_features);
    orig_Com_Printf("===============================\n");
}


qboolean hkCbuf_AddLateCommands(void)
{
    qboolean ret = oCbuf_AddLateCommands();

    PrintOut(PRINT_LOG, "=== Lifecycle: Post-CVar Init Phase ===\n");
    
    // Register SoF Buddy console commands after command system is initialized
    PrintOut(PRINT_LOG, "Attempting to register sofbuddy_list_features command...\n");
    orig_Cmd_AddCommand(const_cast<char*>("sofbuddy_list_features"), Cmd_SoFBuddy_ListFeatures_f);
    
    PrintOut(PRINT_LOG, "Registered sofbuddy_list_features command\n");
    
    // Register SoF Buddy version cvar
    PrintOut(PRINT_LOG, "Registering _sofbuddy_version cvar...\n");
    orig_Cvar_Get("_sofbuddy_version", SOFBUDDY_VERSION, CVAR_ARCHIVE | CVAR_NOSET, NULL);
    PrintOut(PRINT_LOG, "Registered _sofbuddy_version cvar with value: %s\n", SOFBUDDY_VERSION);
    
    // Note: If command already exists or conflicts with a cvar, 
    // Cmd_AddCommand will print an error message to the console
    
    // Dispatch to all features registered for post-cvar initialization
    // Each feature will register its own CVars via PostCvarInit callbacks
    DISPATCH_SHARED_HOOK(PostCvarInit, Post);

    
    PrintOut(PRINT_LOG, "=== Post-CVar Init Phase Complete ===\n");

    Cmd_SoFBuddy_ListFeatures_f();
    return ret;
}

// Earliest initialization function called from DllMain
void lifecycle_EarlyStartup(void)
{
    PrintOut(PRINT_LOG, "=== Lifecycle: Early Startup Phase ===\n");
    
    // Initialize system DLL hooks
    InitializeSystemHooks();
    
    // Initialize hooks targeting SoF.exe (0x200xxxxx addresses) only
    InitializeExeHooks();
    
    // Cmd_AddCommand is now hardcoded at 0x20019130
    PrintOut(PRINT_LOG, "Cmd_AddCommand hardcoded at 0x%p\n", orig_Cmd_AddCommand);
    
    // Dispatch to all features registered for early startup
    DISPATCH_SHARED_HOOK(EarlyStartup, Post);

    // Initialize caller classification maps (prefer sof_buddy/funcmaps, legacy fallback is internal)
    CallsiteClassifier::initialize("sof_buddy/funcmaps");

    #ifndef NDEBUG
    // Initialize ParentRecorder output directory (sof_buddy/func_parents) - debug builds only
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
