#include "../../../hdr/detour_manifest.h"

// Detour definitions for media_timers feature (empty scaffold)

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
        "media_timers"                  // feature_name
    }
};
*/

const DetourDefinition media_timers_detour_manifest[] = {};

const int media_timers_detour_count = sizeof(media_timers_detour_manifest) / sizeof(DetourDefinition);

// Queue these definitions for import during detour_manifest_init().
extern void detour_manifest_queue_definitions(const DetourDefinition *defs, int count);
static int media_timers_manifest_queued = (detour_manifest_queue_definitions(media_timers_detour_manifest, media_timers_detour_count), 0);


// No detours defined in this manifest; no registration lines required.
