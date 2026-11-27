#include "feature_config.h"

#if FEATURE_SCALED_MENU

#ifdef UI_MENU

#include "sof_compat.h"
#include "util.h"
#include "generated_detours.h"
#include "../shared.h"
#if FEATURE_SCALED_MENU && FEATURE_INTRO_SCALER
#include "../../intro_scaler/shared.h"
#endif

void scaledMenu_EarlyStartup(void)
{
    PrintOut(PRINT_LOG, "scaled_menu: installing menu hooks\n");
    // Do NOT resolve menuScale here: this runs from DllMain, before the engine has
    // initialised vid_w/vid_h or registered cvars. Touching them here crashes the loader.
    // menuScale is resolved on the first VID_CheckChanges instead.
    if (!scaled_menu_install_hooks())
        PrintOut(PRINT_BAD, "scaled_menu: menu hook install failed\n");
#if FEATURE_SCALED_MENU && FEATURE_INTRO_SCALER
    intro_scaler_set_clip_extend(scaled_menu_backdrop_clip_extend);
#endif
}

void scaledMenu_PostCvarInit(void)
{
    if (detour_Cmd_AddCommand::oCmd_AddCommand) {
        detour_Cmd_AddCommand::oCmd_AddCommand(
            const_cast<char*>("menu_native"), Cmd_MenuNative_f);
        // Legacy alias: the 4:3->wide correction is now the default.
        detour_Cmd_AddCommand::oCmd_AddCommand(
            const_cast<char*>("menu_wide"), Cmd_MenuWide_f);
    }
}

#endif // UI_MENU

#endif // FEATURE_SCALED_MENU
