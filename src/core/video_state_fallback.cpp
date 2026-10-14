#include "feature_config.h"

#if !(FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE)
int current_vid_w = 0;
int current_vid_h = 0;
float screen_y_scale = 1.0f;
int* viddef_width = nullptr;
int* viddef_height = nullptr;
#endif
