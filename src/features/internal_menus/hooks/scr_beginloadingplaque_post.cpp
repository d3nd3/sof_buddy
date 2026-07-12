#include "feature_config.h"

#if FEATURE_INTERNAL_MENUS

#include "generated_detours.h"
#include "sof_compat.h"
#include "util.h"
#include "../shared.h"
#if FEATURE_HTTP_MAPS
#include "../../http_maps/shared.h"
#endif

void internal_menus_SCR_BeginLoadingPlaque_post(qboolean noPlaque) {
    if (noPlaque) return;
    if (!detour_M_PushMenu::oM_PushMenu) return;

    if (internal_menus_use_vanilla_loading_menu()) {
        internal_menus_sync_loading_network_ui();
        detour_M_PushMenu::oM_PushMenu("loading", "", true);
        internal_menus_call_SCR_UpdateScreen(true);
        return;
    }

    const bool lock_input = internal_menus_should_lock_loading_input();
#if FEATURE_HTTP_MAPS
    // Vanilla re-evaluates <exinclude deathmatch> on every menu push; only skip a redundant
    // second vanilla re-push. Never skip when switching to the custom MP menu (deathmatch set
    // after the first plaque, or map/http_maps finished before CL_Changing).
    if (lock_input && internal_menus_use_vanilla_loading_menu() &&
        http_maps_should_skip_loading_plaque_menu()) {
        internal_menus_sync_loading_network_ui();
        http_maps_refresh_loading_menu_labels();
        return;
    }
#endif
    internal_menus_sync_loading_network_ui();
    if (internal_menus_should_killmenu_before_loading() && lock_input &&
        detour_Cmd_ExecuteString::oCmd_ExecuteString) {
        char killmenu_cmd[] = "killmenu";
        detour_Cmd_ExecuteString::oCmd_ExecuteString(killmenu_cmd);
    }
#if FEATURE_HTTP_MAPS
    http_maps_refresh_loading_menu_labels();
#else
    loading_reset_current_map_unknown();
#endif
    detour_M_PushMenu::oM_PushMenu(internal_menus_loading_menu_name(), "", lock_input);
    internal_menus_call_SCR_UpdateScreen(true);
}

#endif
