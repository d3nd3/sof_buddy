#include "feature_config.h"

#if FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE

#include "sof_compat.h"
#include "util.h"
#include "../shared.h"
#if FEATURE_INTERNAL_MENUS
#include "features/internal_menus/shared.h"
#endif

extern int* viddef_width;
extern int* viddef_height;

void scaled_ui_refresh_vid_dimensions_from_engine(void) {
    if (!viddef_width) viddef_width = (int*)rvaToAbsExe((void*)0x0040365C);
    if (!viddef_height) viddef_height = (int*)rvaToAbsExe((void*)0x00403660);
    if (viddef_width && viddef_height) {
        current_vid_w = *viddef_width;
        current_vid_h = *viddef_height;
        screen_y_scale = (float)current_vid_h / 480.0f;
    }
}

void vid_checkchanges_post(void) {
    scaled_ui_refresh_vid_dimensions_from_engine();
#if FEATURE_INTERNAL_MENUS
    internal_menus_OnVidChanged();
#endif
}

#endif // FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE
