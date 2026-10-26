#include "feature_config.h"

#if FEATURE_INTERNAL_MENUS

#include "../shared.h"
#if FEATURE_MORE_CROSSHAIRS
#include "features/more_crosshairs/shared.h"
#endif

void internal_menus_M_PushMenu_post(char const* menu_file, char const* parentFrame, bool lock_input) {
    (void)parentFrame;
    (void)lock_input;
    internal_menus_remember_menu_page(menu_file);
#if FEATURE_MORE_CROSSHAIRS
    more_crosshairs_set_active_buddy_page(menu_file);
#endif
}

#endif
