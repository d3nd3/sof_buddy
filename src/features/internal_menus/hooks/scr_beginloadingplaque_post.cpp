#include "feature_config.h"

#if FEATURE_INTERNAL_MENUS

#include "generated_detours.h"
#include "sof_compat.h"
#include "../shared.h"
#if FEATURE_HTTP_MAPS
#include "../../http_maps/shared.h"
#endif

void internal_menus_SCR_BeginLoadingPlaque_post(qboolean noPlaque) {
    if (noPlaque) return;
    const bool lock_input = internal_menus_should_lock_loading_input();
#if FEATURE_HTTP_MAPS
    // In unlock mode we always re-push loading UI after plaque; engine plaque path can force
    // menus off, so an early skip here leaves no loading menu visible.
    if (lock_input && http_maps_should_skip_loading_plaque_menu()) return;
#endif
    if (!detour_M_PushMenu::oM_PushMenu) return;
    // In unlock mode, still show loading UI but avoid killmenu churn.
    if (lock_input && detour_Cmd_ExecuteString::oCmd_ExecuteString) {
        char killmenu_cmd[] = "killmenu";
        detour_Cmd_ExecuteString::oCmd_ExecuteString(killmenu_cmd);
    }
    loading_reset_current_map_unknown();
    detour_M_PushMenu::oM_PushMenu(internal_menus_loading_menu_name(), "", lock_input);
    internal_menus_call_SCR_UpdateScreen(true);
}

#endif
