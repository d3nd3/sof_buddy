#include "feature_config.h"

#if FEATURE_INTERNAL_MENUS

#include "generated_detours.h"
#include "sof_compat.h"
#include "../shared.h"

void internal_menus_SCR_BeginLoadingPlaque_post(qboolean noPlaque) {
    if (noPlaque) return;
    if (!detour_M_PushMenu::oM_PushMenu) return;
    if (orig_Cmd_ExecuteString)
        orig_Cmd_ExecuteString("killmenu");
    const bool lock_input = internal_menus_should_lock_loading_input();
    detour_M_PushMenu::oM_PushMenu("loading/loading", "", lock_input);
    internal_menus_call_SCR_UpdateScreen(true);
}

#endif
