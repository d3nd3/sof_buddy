#include "feature_config.h"

#if FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE

#include "sof_compat.h"
#include "util.h"
#include "../shared.h"
#include "runtime_features.h"
#if FEATURE_INTERNAL_MENUS
#include "features/internal_menus/shared.h"
#endif
#if FEATURE_SCALED_MENU
#include "features/scaled_menu/shared.h"
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
        screen_x_scale = (current_vid_w > 0) ? (float)current_vid_w / 640.0f : 1.0f;
#if FEATURE_SCALED_CON
        if (RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_SCALED_CON) && _sofbuddy_font_scale)
            fontscale_change(_sofbuddy_font_scale);
#endif
#if FEATURE_SCALED_HUD
        if (RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_SCALED_HUD)) {
            if (_sofbuddy_hud_scale) hudscale_change(_sofbuddy_hud_scale);
            apply_auto_crosshair_scale();
        }
#endif
#if FEATURE_SCALED_MENU
        if (RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_SCALED_MENU) && _sofbuddy_menu_scale)
            menuscale_change(_sofbuddy_menu_scale);
#endif
#if FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_MENU
        reapply_scale_percentages();
#endif
    }
}

void vid_checkchanges_post(void) {
    scaled_ui_refresh_vid_dimensions_from_engine();
#if FEATURE_INTERNAL_MENUS
    if (RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_INTERNAL_MENUS))
        internal_menus_OnVidChanged();
#endif
#if FEATURE_SCALED_MENU
    if (RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_SCALED_MENU))
        scaled_menu_on_vid_layout_changed();
#endif
}

#endif // FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE
