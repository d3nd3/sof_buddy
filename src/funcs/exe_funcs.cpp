#include "sof_buddy.h"
#include "util.h"
#include "detour_manifest.h"
#include "../hdr/detour_manifest.h"

// Centralized hook shims that delegate to feature-owned implementations.

// Media Timers
// Forward declare with C linkage to match definition
extern "C" int media_timers_on_Sys_Milliseconds(void);
int core_Sys_Milliseconds(void)
{
    return media_timers_on_Sys_Milliseconds();
}

// Cinematic Freeze
// Forward declare to avoid include path issues
void cinematic_freeze_on_CinematicFreeze(bool beforeState, bool afterState);
extern void (*orig_CinematicFreeze)(bool);
void core_CinematicFreeze(bool bEnable)
{
    bool before = *(char*)0x5015D8D5 == 1 ? true : false;
    orig_CinematicFreeze(bEnable);
    bool after = *(char*)0x5015D8D5 == 1 ? true : false;
    cinematic_freeze_on_CinematicFreeze(before, after);
}

// Forward declarations for core detour functions
void my_FS_InitFilesystem(void);
qboolean my_Cbuf_AddLateCommands(void);
// Sys_GetGameApi is implemented here (moved from sof_buddy.cpp)
extern void * (*orig_Sys_GetGameApi)(void * imports);
void * my_Sys_GetGameApi(void * imports);
qboolean my_VID_LoadRefresh(char *name);

// Original function storage pointers for core detours
extern void (*orig_FS_InitFilesystem)(void);
extern qboolean (*orig_Cbuf_AddLateCommands)(void);
extern qboolean (*orig_VID_LoadRefresh)(char*);

// Core detour definitions - infrastructure detours that all features depend on
static const DetourDefinition core_detour_manifest[] = {
    {
        "FS_InitFilesystem",
        DETOUR_TYPE_PTR,
        NULL,  // Not used for PTR type
        "0x20026980",  // Address expression for FS_InitFilesystem
        "exe",
        DETOUR_PATCH_JMP,
        6,
        "Core filesystem init hook",
        "core"
    },
    {
        "Cbuf_AddLateCommands",
        DETOUR_TYPE_PTR,
        NULL,  // Not used for PTR type
        "0x20018740",  // Address expression for Cbuf_AddLateCommands
        "exe",
        DETOUR_PATCH_JMP,
        5,
        "Post-cmdline init hook",
        "core"
    },
    {
        "Sys_GetGameApi",
        DETOUR_TYPE_PTR,
        NULL,  // Not used for PTR type
        "0x20065F20",  // Address expression for Sys_GetGameApi
        "exe",
        DETOUR_PATCH_JMP,
        5,
        "Game API hook",
        "core"
    },
    {
        "VID_LoadRefresh",
        DETOUR_TYPE_PTR,
        NULL,  // Not used for PTR type
        "0x20066E10",  // Address expression for VID_LoadRefresh
        "exe",
        DETOUR_PATCH_JMP,
        7,
        "Renderer reload hook",
        "core"
    }
};

static const int core_detour_count = sizeof(core_detour_manifest) / sizeof(DetourDefinition);

// Registration function for core detours
void core_register_detours(void)
{
    extern void detour_manifest_add_definition(const struct DetourDefinition *def);
    

    
    // Register core detour definitions
    for (int i = 0; i < core_detour_count; i++) {
        detour_manifest_add_definition(&core_detour_manifest[i]);
    }
    

    PrintOut(PRINT_LOG, "Core detours registered: %d definitions\n", core_detour_count);
}

// Core VID_LoadRefresh implementation - applies REF_GL detours when renderer loads
qboolean my_VID_LoadRefresh(char *name)
{
    PrintOut(PRINT_LOG, "=== my_VID_LoadRefresh ENTRY: loading %s ===\n", name ? name : "NULL");
    //vid_ref loaded.
    qboolean ret = orig_VID_LoadRefresh(name);

    // Apply ref_gl.dll detours using manifest system
    PrintOut(PRINT_LOG, "=== Applying ref_gl.dll detours ===\n");
    extern void detour_manifest_apply_all(DetourManifestTarget target, const char *phase);
    detour_manifest_apply_all(DETOUR_MANIFEST_REF_GL, "ref_init");
    PrintOut(PRINT_LOG, "=== ref_gl.dll detours applied ===\n");

    return ret;
}

// Sys_GetGameApi implementation - applies GAME_X86 detours when the game module
// is loaded. Moved from `src/sof_buddy.cpp` to centralize core shims.
void * my_Sys_GetGameApi(void * imports)
{
    PrintOut(PRINT_LOG, "=== my_Sys_GetGameApi ENTRY ===\n");

    void * pv_ret = 0;
    // calls LoadLibrary
    pv_ret = orig_Sys_GetGameApi(imports);

    PrintOut(PRINT_LOG, "=== Applying GAME_X86 detours ===\n");
    // Apply gamex86.dll detours using manifest system
    extern void detour_manifest_apply_all(DetourManifestTarget target, const char *phase);
    detour_manifest_apply_all(DETOUR_MANIFEST_GAME_X86, "game_init");
    PrintOut(PRINT_LOG, "=== GAME_X86 detours applied ===\n");

    return pv_ret;
}

// Auto-generated wrapper for detour: V_DrawScreen
void exe_V_DrawScreen(void) {
    // call registered hooks if present (call_hook_V_DrawScreen may be defined elsewhere)
    // call_hook_V_DrawScreen();
    void *orig_tr = detour_manifest_get_internal_trampoline("V_DrawScreen");
    (void)orig_tr;
}

