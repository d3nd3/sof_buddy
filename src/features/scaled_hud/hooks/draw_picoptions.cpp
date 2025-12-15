#include "feature_config.h"

#if FEATURE_SCALED_HUD

#include "sof_compat.h"
#include "util.h"
#include "generated_detours.h"
#include "../../scaled_ui_base/shared.h"
#include "debug/hook_callsite.h"

using detour_Draw_PicOptions::oDraw_PicOptions;

void hkDraw_PicOptions(int x, int y, float w_scale, float h_scale, int pal, char * name, detour_Draw_PicOptions::tDraw_PicOptions original) {
	if (!oDraw_PicOptions && original) oDraw_PicOptions = original;
    
    HookCallsite::recordAndGetFnStartExternal("Draw_PicOptions");
    g_activeDrawCall = DrawRoutineType::PicOptions;
    
    {
        uint32_t fnStart = HookCallsite::recordAndGetFnStartExternal("Draw_PicOptions");
        if (fnStart) {
            PicOptionsCaller detected = getPicOptionsCallerFromRva(fnStart);
            if (g_currentPicOptionsCaller == PicOptionsCaller::Unknown && detected != PicOptionsCaller::Unknown) {
                g_currentPicOptionsCaller = detected;
            }
        }
    }

    if (g_activeRenderType == uiRenderType::HudDmRanking) {
        float x_scale = static_cast<float>(current_vid_w) / 640.0f;
        int offsetEdge = 40; //36??
        if (hudDmRanking_wasImage) {
            //This is the 2nd image from the top right corner.
            //Below the eye image.

            //2nd image, means we in spec chasing someone. This image is a DM Logo.
            x = current_vid_w - 32 * hudScale - offsetEdge * x_scale;
            // 20px fixed border, 32px image above, 6px spacing to eye, 
            y = 20 * screen_y_scale + 32 * hudScale + 6 * screen_y_scale;

            w_scale = hudScale;
            h_scale = hudScale;
        } else {
            x = current_vid_w - 32 * hudScale - offsetEdge * x_scale;
            w_scale = hudScale;
            h_scale = hudScale;
        }
        // To help R_DrawFont know if teamIcon image was drawn.
        hudDmRanking_wasImage = true;

    } else if (g_activeRenderType == uiRenderType::HudInventory) {
        static bool secondDigit = false;

        float x_scale = static_cast < float > (current_vid_w) / 640.0f;

        x = static_cast < float > (current_vid_w) - 13.0f * x_scale - 63.0f * hudScale;
        if (!secondDigit) x += 18.0f * hudScale;

        y = static_cast < float > (current_vid_h) - 3.0f * screen_y_scale - 49.0f * hudScale;

        w_scale = hudScale;
        h_scale = hudScale;

        if (secondDigit) secondDigit = false;
        else secondDigit = true;
    }

    oDraw_PicOptions(x, y, w_scale, h_scale, pal, name);

    g_currentPicOptionsCaller = PicOptionsCaller::Unknown;
    g_activeDrawCall = DrawRoutineType::None;
}

#endif // FEATURE_SCALED_HUD

