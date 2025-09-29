#include "hooks.h"
#include "detour_manifest.h"
#include "../../hooks/hook_registry.h"
#include "../../feature_registration.h"

// Implement your feature callback(s) here.
void refgl_on_media_timers(void) {
    // TODO: implement feature behavior
}

void media_timers_register_feature_hooks(void) {
    // Example registration - adjust to the hooks your feature needs:
    // register_hook_VID_CheckChanges(refgl_on_media_timers);
}

// Minimal startup that registers this feature's hooks. This will be
// referenced by FEATURE_AUTO_REGISTER so the feature is queued during
// static initialization and picked up by the manifest subsystem.
void media_timers_startup(void) {
    media_timers_register_feature_hooks();
}

// Use new registration order: early_init, startup, late_init, detours
FEATURE_AUTO_REGISTER(media_timers, "media_timers", nullptr, &media_timers_startup, nullptr, &media_timers_register_detours);

// Minimal C-linkage implementation expected by core shims.
extern "C" int media_timers_on_Sys_Milliseconds(void) {
    // Default: no timers implemented yet; return 0.
    return 0;
}
