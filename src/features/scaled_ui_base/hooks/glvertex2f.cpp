#include "feature_config.h"

#if FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_TEXT || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE

#include "sof_compat.h"
#include "util.h"
#include "../../scaled_ui_base/shared.h"
#include "debug/hook_callsite.h"

extern void(__stdcall * orig_glVertex2f)(float one, float two);

static inline void scaleVertexFromScreenCenter(float& x, float& y, float scale) {
    SOFBUDDY_ASSERT(orig_glVertex2f != nullptr);
    SOFBUDDY_ASSERT(scale > 0.0f);
    SOFBUDDY_ASSERT(current_vid_w > 0);
    SOFBUDDY_ASSERT(current_vid_h > 0);
    
    static int vertexCounter = 1;
    static float x_mid_offset;
    static float y_mid_offset;
    static float x_first_vertex;
    static float y_first_vertex;
    if (vertexCounter == 1) {
        x_mid_offset = current_vid_w * 0.5f - x;
        y_mid_offset = current_vid_h * 0.5f - y;
        x_first_vertex = x;
        y_first_vertex = y;
    }
    float final_x = x * scale - x_first_vertex * (scale - 1) - x_mid_offset * (scale - 1);
    float final_y = y * scale - y_first_vertex * (scale - 1) - y_mid_offset * (scale - 1);
    orig_glVertex2f(final_x, final_y);
    vertexCounter++;
    if (vertexCounter > 4) vertexCounter = 1;
}

static inline void scaleVertexFromCenter(float& x, float& y, float scale) {
    SOFBUDDY_ASSERT(orig_glVertex2f != nullptr);
    SOFBUDDY_ASSERT(scale > 0.0f);
    
    static int svfc_vertexCounter = 1;
    static int svfc_centerX;
    static int svfc_centerY;

    if (svfc_vertexCounter == 1) {
        SOFBUDDY_ASSERT(DrawPicWidth > 0);
        SOFBUDDY_ASSERT(DrawPicHeight > 0);
        
        svfc_centerX = x + DrawPicWidth * 0.5f;
        svfc_centerY = y + DrawPicHeight * 0.5f;

        if (DrawPicWidth == 0 || DrawPicHeight == 0) {
            PrintOut(PRINT_LOG, "[DEBUG] scaleVertexFromCenter: FATAL ERROR - DrawPicWidth or DrawPicHeight is 0!\n");
            extern void (*orig_Com_Printf)(const char* fmt, ...);
            if (orig_Com_Printf) orig_Com_Printf("FATAL ERROR: DrawPicWidth or DrawPicHeight is 0! This should not happen!\n");
            exit(1);
        }
    }

    x = svfc_centerX + (x - svfc_centerX) * scale;
    y = svfc_centerY + (y - svfc_centerY) * scale;

    orig_glVertex2f(x, y);

    svfc_vertexCounter++;
    if (svfc_vertexCounter > 4) {
        svfc_vertexCounter = 1;
        DrawPicWidth = 0;
        DrawPicHeight = 0;
    }
}

void __stdcall hkglVertex2f(float x, float y) {
    SOFBUDDY_ASSERT(orig_glVertex2f != nullptr);
    
    HookCallsite::recordAndGetFnStartExternal("glVertex2f");

    switch (g_activeRenderType) {
        case uiRenderType::Console:
            SOFBUDDY_ASSERT(fontScale > 0.0f);
            if (g_activeDrawCall != DrawRoutineType::StretchPic) {
                orig_glVertex2f(x * fontScale, y * fontScale);
                return;
            }
        break;
        case uiRenderType::Scoreboard: {
            scaleVertexFromScreenCenter(x, y, hudScale);
            return;
        }
        default:
            // Generic Draw calls
            switch (g_activeDrawCall) {
                case DrawRoutineType::PicOptions: {
                    switch (g_currentPicOptionsCaller) {
                        case PicOptionsCaller::SCR_DrawCrosshair: {
                            scaleVertexFromScreenCenter(x, y, crosshairScale);
                            return;
                        }
                    }
                break;
                }
                case DrawRoutineType::Pic: {
                    switch (g_currentPicCaller) {
                        case PicCaller::SCR_DrawCrosshair: {
                            scaleVertexFromScreenCenter(x, y, crosshairScale);
                            return;
                        }
                        default: {
                            if (mainMenuBgTiled) {
                                //BackdropDraw
                                static int vertexCounter = 1;
                                static int startX;
                                static int startY;
                                if (vertexCounter == 1) {
                                    startX = x;
                                    startY = y;
                                }
                                if (vertexCounter > 1 && vertexCounter < 4)
                                    x = startX + DrawPicWidth;
                                if (vertexCounter > 2)
                                    y = startY + DrawPicHeight;
                                orig_glVertex2f(x, y);
                                vertexCounter++;
                                if (vertexCounter > 4) { 
                                    vertexCounter = 1;
                                    DrawPicWidth = 0;
                                    DrawPicHeight = 0;
                                }
                                return;
                            } else {
                                // Scale from center of image
                                if (g_currentPicCaller != PicCaller::DrawPicWrapper) {
                                    break;
                                    scaleVertexFromCenter(x, y, hudScale);
                                    return;
                                }
                            }
                            break;
                        }
                    
                    }
                break;

            }
        }
    }
    orig_glVertex2f(x, y);
}

#endif // FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_TEXT || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE

