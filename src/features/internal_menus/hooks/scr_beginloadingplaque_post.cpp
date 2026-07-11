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

    static int* s_cls_state = (int*)rvaToAbsExe((void*)0x001C1F00);
    if (s_cls_state) internal_menus_update_connect_flow(*s_cls_state);

    // SP local load: vanilla loading.rmf from pak. Avoid killmenu so blankscreen/outfit interstitials
    // are not cleared early.
    if (internal_menus_use_vanilla_loading_menu()) {
        internal_menus_sync_loading_network_ui();
        detour_M_PushMenu::oM_PushMenu("loading", "", true);
        internal_menus_call_SCR_UpdateScreen(true);
        return;
    }

    const bool lock_input = internal_menus_should_lock_loading_input();
#if FEATURE_HTTP_MAPS
    // After precache, a second SCR_BeginLoadingPlaque often runs; killmenu+push flashes the loading
    // UI and can destabilize the client right before spawn.
    if (lock_input && http_maps_should_skip_loading_plaque_menu()) return;
#endif
    internal_menus_sync_loading_network_ui();
    if (internal_menus_should_killmenu_before_loading() && lock_input &&
        detour_Cmd_ExecuteString::oCmd_ExecuteString) {
        char killmenu_cmd[] = "killmenu";
        detour_Cmd_ExecuteString::oCmd_ExecuteString(killmenu_cmd);
    }
    loading_reset_current_map_unknown();
    detour_M_PushMenu::oM_PushMenu(internal_menus_loading_menu_name(), "", lock_input);
    internal_menus_call_SCR_UpdateScreen(true);
}

#endif
