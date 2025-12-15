#include "feature_config.h"

#if FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_TEXT || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE

#include "sof_compat.h"
#include "util.h"
#include "../../scaled_ui_base/shared.h"

void* gl_findimage_post_callback(void* result, char *filename, int imagetype, char mimap, char allowPicmip) {
    if (result) {
        #ifdef UI_MENU
        if (g_currentPicCaller == PicCaller::Crosshair || g_currentPicCaller == PicCaller::ExecuteLayoutString ) {
            DrawPicWidth = *(short*)((char*)result + 0x44);
            DrawPicHeight = *(short*)((char*)result + 0x46);
        } else 
        #endif
        if ( g_activeDrawCall == DrawRoutineType::Pic) {
            if (mainMenuBgTiled) {
                DrawPicWidth = *(short*)((char*)result + 0x44) * screen_y_scale;
                DrawPicHeight = *(short*)((char*)result + 0x46) * screen_y_scale;
            } else {
                DrawPicWidth = *(short*)((char*)result + 0x44);
                DrawPicHeight = *(short*)((char*)result + 0x46);
            }
        }
    }
    
    return result;
}

#endif // FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_TEXT || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE

