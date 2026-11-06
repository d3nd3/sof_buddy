#include "feature_config.h"

#if FEATURE_INTERNAL_MENUS

#include "../shared.h"
#include "sof_compat.h"
#include "util.h"
#include <cstring>

bool g_internal_menus_main_menu_seen = false;
#if FEATURE_MORE_CROSSHAIRS
#include "features/more_crosshairs/shared.h"
#endif

void internal_menus_M_PushMenu_post(char const* menu_file, char const* parentFrame, bool lock_input) {
    (void)parentFrame;
    (void)lock_input;
    internal_menus_feature_prompt_note_menu(menu_file);
    internal_menus_remember_menu_page(menu_file);
    if (menu_file && std::strstr(menu_file, "sof_buddy")) {
        auto* ms = *reinterpret_cast<void**>(rvaToAbsExe(reinterpret_cast<void*>(0x00406554)));
        auto* menu = ms ? *reinterpret_cast<void**>(static_cast<char*>(ms) + 4) : nullptr;
        if (menu) {
            *reinterpret_cast<int*>(static_cast<char*>(menu) + 0x94) = 0;
            *reinterpret_cast<int*>(static_cast<char*>(menu) + 0x98) = 0;
            *reinterpret_cast<int*>(static_cast<char*>(menu) + 0x9C) = 0;
            *reinterpret_cast<int*>(static_cast<char*>(menu) + 0xA0) = 0;
        }
    }
#if FEATURE_MORE_CROSSHAIRS
    more_crosshairs_set_active_buddy_page(menu_file);
#endif
}

#endif
