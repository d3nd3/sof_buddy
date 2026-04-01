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
#if FEATURE_HTTP_MAPS
    if (http_maps_should_skip_loading_plaque_menu()) return;
#endif
    if (!detour_M_PushMenu::oM_PushMenu) return;
    const bool lock_input = internal_menus_should_lock_loading_input();
    // In unlock mode, still show loading UI but avoid killmenu churn.
    if (lock_input && detour_Cmd_ExecuteString::oCmd_ExecuteString)
        detour_Cmd_ExecuteString::oCmd_ExecuteString("killmenu");
    detour_M_PushMenu::oM_PushMenu(internal_menus_loading_menu_name(), "", lock_input);
    internal_menus_call_SCR_UpdateScreen(true);
}

#endif
