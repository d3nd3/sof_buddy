#include "feature_config.h"

#if FEATURE_INTERNAL_MENUS

#include "../shared.h"
#if FEATURE_LIGHTING_BLEND
#include "../../lighting_blend/shared.h"
#endif

void internal_menus_M_ForceMenuOff_post(void) {
    internal_menus_note_menu_page(nullptr, nullptr);
    internal_menus_feature_prompt_note_closed();
#if FEATURE_LIGHTING_BLEND
    lighting_gl_modulate_menu_closed();
#endif
}

#endif
