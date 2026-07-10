#include "feature_config.h"

#if FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE

#include "sof_compat.h"
#include "util.h"
#include "../../scaled_ui_base/shared.h"

extern void(__stdcall * orig_glVertex2f)(float one, float two);

static int g_quadVertexIndex = 1;
static float g_quadXMidOffset;
static float g_quadYMidOffset;
static float g_quadXFirst;
static float g_quadYFirst;

static int g_tiledBgVertexIndex = 1;
static int g_tiledBgStartX;
static int g_tiledBgStartY;

void resetGlVertexQuadState() {
    g_quadVertexIndex = 1;
    g_tiledBgVertexIndex = 1;
}

static inline void scaleVertexFromScreenCenter(float& x, float& y, float scale) {
    SOFBUDDY_ASSERT(orig_glVertex2f != nullptr);
    SOFBUDDY_ASSERT(scale > 0.0f);
    SOFBUDDY_ASSERT(current_vid_w > 0);
    SOFBUDDY_ASSERT(current_vid_h > 0);

    if (g_quadVertexIndex == 1) {
        g_quadXMidOffset = current_vid_w * 0.5f - x;
        g_quadYMidOffset = current_vid_h * 0.5f - y;
        g_quadXFirst = x;
        g_quadYFirst = y;
    }
    float final_x = x * scale - g_quadXFirst * (scale - 1) - g_quadXMidOffset * (scale - 1);
    float final_y = y * scale - g_quadYFirst * (scale - 1) - g_quadYMidOffset * (scale - 1);
    orig_glVertex2f(final_x, final_y);
    g_quadVertexIndex++;
    if (g_quadVertexIndex > 4) g_quadVertexIndex = 1;
}

void __stdcall hkglVertex2f(float x, float y) {
    SOFBUDDY_ASSERT(orig_glVertex2f != nullptr);
    switch (g_activeRenderType) {
#if FEATURE_SCALED_CON
        case uiRenderType::Console:
            SOFBUDDY_ASSERT(fontScale > 0.0f);
            if (g_activeDrawCall != DrawRoutineType::StretchPic) {
                orig_glVertex2f(x * fontScale, y * fontScale);
                return;
            }
        break;
#endif
        case uiRenderType::Scoreboard:
            scaleVertexFromScreenCenter(x, y, hudScale);
            return;
        default:
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
                                if (g_tiledBgVertexIndex == 1) {
                                    g_tiledBgStartX = x;
                                    g_tiledBgStartY = y;
                                }
                                if (g_tiledBgVertexIndex > 1 && g_tiledBgVertexIndex < 4)
                                    x = g_tiledBgStartX + DrawPicWidth;
                                if (g_tiledBgVertexIndex > 2)
                                    y = g_tiledBgStartY + DrawPicHeight;
                                orig_glVertex2f(x, y);
                                g_tiledBgVertexIndex++;
                                if (g_tiledBgVertexIndex > 4) {
                                    g_tiledBgVertexIndex = 1;
                                    DrawPicWidth = 0;
                                    DrawPicHeight = 0;
                                }
                                return;
                            }
                            break;
                        }
                    }
                break;
            }
        }
    }
    resetGlVertexQuadState();
    orig_glVertex2f(x, y);
}

#endif // FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE
