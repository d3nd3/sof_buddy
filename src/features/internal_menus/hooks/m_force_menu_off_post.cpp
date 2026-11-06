#include "feature_config.h"

#if FEATURE_INTERNAL_MENUS

#include "../shared.h"

void internal_menus_M_ForceMenuOff_post(void) {
    internal_menus_feature_prompt_note_closed();
}

#endif
