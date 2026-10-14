#include "feature_config.h"

#if FEATURE_INTERNAL_MENUS

#include "../shared.h"

void internal_menus_M_PushMenu_post(char const* menu_file, char const* parentFrame, bool lock_input) {
    (void)parentFrame;
    (void)lock_input;
    internal_menus_remember_menu_page(menu_file);
}

#endif
