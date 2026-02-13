#include "feature_config.h"

#if FEATURE_INTERNAL_MENUS

#include "sof_compat.h"
#include "../shared.h"

extern bool g_sofbuddy_menu_calling;

void internal_menus_M_PushMenu_post(char const* menu_file, char const* parentFrame, bool lock_input) {
    (void)menu_file;
    (void)parentFrame;
    (void)lock_input;


    if (g_sofbuddy_menu_calling) return;
    if (!g_internal_menus_external_loading_push) return;
    if (!orig_Cvar_Set2 || !_sofbuddy_menu_internal) return;

    // Restore normal mode right after the engine's loading push call.
    g_internal_menus_external_loading_push = false;
    orig_Cvar_Set2(const_cast<char*>("sofbuddy_menu_internal"), const_cast<char*>("0"), true);
}

#endif
