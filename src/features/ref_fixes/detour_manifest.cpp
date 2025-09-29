#include "../../../hdr/detour_manifest.h"

// Detour definitions for ref_fixes feature (empty scaffold)

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
        "ref_fixes"                  // feature_name
    }
};
*/

const DetourDefinition ref_fixes_detour_manifest[] = {};

const int ref_fixes_detour_count = sizeof(ref_fixes_detour_manifest) / sizeof(DetourDefinition);

// Queue these definitions for import during detour_manifest_init().
extern void detour_manifest_queue_definitions(const DetourDefinition *defs, int count);
static int ref_fixes_manifest_queued = (detour_manifest_queue_definitions(ref_fixes_detour_manifest, ref_fixes_detour_count), 0);


// No detours defined in this manifest; no registration lines required.
