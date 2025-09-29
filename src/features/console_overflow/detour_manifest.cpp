#include "../../../hdr/detour_manifest.h"

// Detour definitions for console_overflow feature (empty scaffold)

/* Example detour (commented out - does not apply)
const DetourDefinition example_detour_manifest[] = {
    {
        "V_DrawScreen",               // name
        DETOUR_TYPE_PTR,               // type
        NULL,                          // target_module (NULL for PTR)
        "0x13371337",                 // target_proc / address expression
        "exe",                         // wrapper_prefix (exe/ref/game)
        DETOUR_PATCH_JMP,              // patch_type
        5,                             // detour_len (number of patched bytes)
        "void(void)",                 // signature (compact prototype)
        "console_overflow"                  // feature_name
    }
};
*/

const DetourDefinition console_overflow_detour_manifest[] = {};

const int console_overflow_detour_count = sizeof(console_overflow_detour_manifest) / sizeof(DetourDefinition);

// Queue these definitions for import during detour_manifest_init().
extern void detour_manifest_queue_definitions(const DetourDefinition *defs, int count);
static int console_overflow_manifest_queued = (detour_manifest_queue_definitions(console_overflow_detour_manifest, console_overflow_detour_count), 0);


extern void detour_manifest_register_detour_function(const char *name, void *func_ptr);
static int _reg_console_overflow_V_DrawScreen = (detour_manifest_register_detour_function("V_DrawScreen", (void*)&exe_V_DrawScreen), 0);
