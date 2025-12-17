#include "feature_config.h"

#if FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE

#include "sof_compat.h"
#include "util.h"
#include "../shared.h"

extern int* viddef_width;
extern int* viddef_height;

void vid_checkchanges_post(void) {
    if (!viddef_width) viddef_width = (int*)rvaToAbsExe((void*)0x0040365C);
    if (!viddef_height) viddef_height = (int*)rvaToAbsExe((void*)0x00403660);
    current_vid_w = *viddef_width;
    current_vid_h = *viddef_height;
    screen_y_scale = (float)current_vid_h / 480.0f;
}

#endif // FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE

