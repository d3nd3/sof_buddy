#include "../../../hdr/detour_manifest.h"

// Detour definitions for scaled_font feature (empty scaffold)

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
        "scaled_font"                  // feature_name
    }
};
*/

const DetourDefinition scaled_font_detour_manifest[] = {};

const int scaled_font_detour_count = sizeof(scaled_font_detour_manifest) / sizeof(DetourDefinition);

// Queue these definitions for import during detour_manifest_init().
extern void detour_manifest_queue_definitions(const DetourDefinition *defs, int count);
static int scaled_font_manifest_queued = (detour_manifest_queue_definitions(scaled_font_detour_manifest, scaled_font_detour_count), 0);
