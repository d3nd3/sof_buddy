#include "feature_config.h"

#if FEATURE_INTERNAL_MENUS

#include "sof_compat.h"
#include "../shared.h"
#include <cstring>

extern bool g_sofbuddy_menu_calling;

void internal_menus_M_PushMenu_pre(char const*& menu_file, char const*& parentFrame, bool& lock_input) {
    (void)menu_file;
    (void)parentFrame;
    (void)lock_input;


    if (g_sofbuddy_menu_calling) return;
    if (!_sofbuddy_menu_internal || !orig_Cvar_Set2) return;

    // The engine's loading push should use the embedded loading menu assets.
    // Everything else stays in normal (external) menu mode.
    const bool is_loading_push = (menu_file && std::strcmp(menu_file, "loading") == 0);
    g_internal_menus_external_loading_push = is_loading_push;
    orig_Cvar_Set2(const_cast<char*>("sofbuddy_menu_internal"), const_cast<char*>(is_loading_push ? "1" : "0"), true);
}

#endif
