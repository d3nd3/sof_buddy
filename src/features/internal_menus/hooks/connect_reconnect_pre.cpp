#include "feature_config.h"

#if FEATURE_INTERNAL_MENUS

#include "generated_detours.h"
#include "sof_compat.h"
#include "../shared.h"

namespace {

void push_loading_override_menu() {
    if (!detour_M_PushMenu::oM_PushMenu) return;

    // Replace top page so ESC behavior stays predictable.
    if (orig_Cmd_ExecuteString)
        orig_Cmd_ExecuteString("killmenu");

    loading_seed_current_from_engine_mapname();

    const bool lock_input = internal_menus_should_lock_loading_input();
    detour_M_PushMenu::oM_PushMenu("loading/loading", "", lock_input);
}

} // namespace

void internal_menus_Reconnect_f_pre() {
    push_loading_override_menu();
}

#endif
