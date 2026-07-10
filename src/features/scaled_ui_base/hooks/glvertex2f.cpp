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

static int g_cineVertexIndex = 1;
static float g_cineAnchorX;
static float g_cineAnchorY;

extern int g_croppedPicVertexIndex;

void resetGlVertexQuadState() {
    g_quadVertexIndex = 1;
    g_tiledBgVertexIndex = 1;
    g_cineVertexIndex = 1;
#if FEATURE_SCALED_HUD
    g_croppedPicVertexIndex = 1;
#endif
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

#if FEATURE_SCALED_HUD
static inline bool tryScoreboardVertex(float& x, float& y) {
    if (g_activeRenderType == uiRenderType::Scoreboard) {
        scaleVertexFromScreenCenter(x, y, hudScale);
        return true;
    }
    return false;
}
#endif

#if FEATURE_SCALED_HUD || FEATURE_SCALED_MENU
static inline void scaleCinematicVertex(float x, float y) {
    if (fontScale == 1.0f) {
        orig_glVertex2f(x, y);
        return;
    }
    if (g_cineVertexIndex == 1) {
        g_cineAnchorX = x;
        g_cineAnchorY = y;
    }
    orig_glVertex2f(
        g_cineAnchorX + (x - g_cineAnchorX) * fontScale,
        g_cineAnchorY + (y - g_cineAnchorY) * fontScale);
    if (++g_cineVertexIndex > 4) g_cineVertexIndex = 1;
}
#endif

#if FEATURE_SCALED_CON
static void handleDrawCharVertex(float x, float y) {
    SOFBUDDY_ASSERT(orig_glVertex2f != nullptr);
#if FEATURE_SCALED_HUD
    if (tryScoreboardVertex(x, y)) return;
#endif
    if (g_activeRenderType == uiRenderType::Console &&
        g_activeDrawCall != DrawRoutineType::StretchPic) {
        SOFBUDDY_ASSERT(fontScale > 0.0f);
        orig_glVertex2f(x * fontScale, y * fontScale);
        return;
    }
    orig_glVertex2f(x, y);
}

void __stdcall my_glVertex2f_DrawChar_1(float x, float y) { handleDrawCharVertex(x, y); }
void __stdcall my_glVertex2f_DrawChar_2(float x, float y) { handleDrawCharVertex(x, y); }
void __stdcall my_glVertex2f_DrawChar_3(float x, float y) { handleDrawCharVertex(x, y); }
void __stdcall my_glVertex2f_DrawChar_4(float x, float y) { handleDrawCharVertex(x, y); }
#endif

#if FEATURE_SCALED_HUD || FEATURE_SCALED_MENU
static void handleStretchPicVertex(float x, float y) {
    SOFBUDDY_ASSERT(orig_glVertex2f != nullptr);
#if FEATURE_SCALED_HUD
    if (g_activeRenderType == uiRenderType::Scoreboard) {
        scaleVertexFromScreenCenter(x, y, hudScale);
        return;
    }
#endif
    if (g_activeRenderType == uiRenderType::Cinematic) {
        scaleCinematicVertex(x, y);
        return;
    }
    orig_glVertex2f(x, y);
}

void __stdcall my_glVertex2f_StretchPic_1(float x, float y) { handleStretchPicVertex(x, y); }
void __stdcall my_glVertex2f_StretchPic_2(float x, float y) { handleStretchPicVertex(x, y); }
void __stdcall my_glVertex2f_StretchPic_3(float x, float y) { handleStretchPicVertex(x, y); }
void __stdcall my_glVertex2f_StretchPic_4(float x, float y) { handleStretchPicVertex(x, y); }

static void handleDrawPicVertex(float x, float y) {
    SOFBUDDY_ASSERT(orig_glVertex2f != nullptr);
#if FEATURE_SCALED_HUD
    if (tryScoreboardVertex(x, y)) return;
#endif
    if (g_activeRenderType == uiRenderType::Cinematic) {
        scaleCinematicVertex(x, y);
        return;
    }
    switch (g_currentPicCaller) {
#if FEATURE_SCALED_HUD
        case PicCaller::SCR_DrawCrosshair:
            scaleVertexFromScreenCenter(x, y, crosshairScale);
            return;
        case PicCaller::SCR_DrawCinemaScope: {
            const float s = effective_auto_scale(screen_y_scale);
            if (g_scaleCinematicPics && s != 1.0f && DrawPicWidth > 0 && DrawPicHeight > 0) {
                if (g_quadVertexIndex == 1) {
                    g_quadXFirst = x;
                    g_quadYFirst = y;
                    const float freeW = current_vid_w - (float)DrawPicWidth;
                    const float freeH = current_vid_h - (float)DrawPicHeight;
                    const float fx = freeW > 0.0f ? x / freeW : 0.5f;
                    const float fy = freeH > 0.0f ? y / freeH : 0.5f;
                    g_quadXMidOffset = fx * (current_vid_w - DrawPicWidth * s);
                    g_quadYMidOffset = fy * (current_vid_h - DrawPicHeight * s);
                }
                orig_glVertex2f(
                    g_quadXMidOffset + (x - g_quadXFirst) * s,
                    g_quadYMidOffset + (y - g_quadYFirst) * s);
                if (++g_quadVertexIndex > 4) g_quadVertexIndex = 1;
                return;
            }
            break;
        }
#endif
        default:
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
    orig_glVertex2f(x, y);
}

void __stdcall my_glVertex2f_DrawPic_1(float x, float y) { handleDrawPicVertex(x, y); }
void __stdcall my_glVertex2f_DrawPic_2(float x, float y) { handleDrawPicVertex(x, y); }
void __stdcall my_glVertex2f_DrawPic_3(float x, float y) { handleDrawPicVertex(x, y); }
void __stdcall my_glVertex2f_DrawPic_4(float x, float y) { handleDrawPicVertex(x, y); }
#endif

#if FEATURE_SCALED_HUD
static void handleDrawPicOptionsVertex(float x, float y) {
    SOFBUDDY_ASSERT(orig_glVertex2f != nullptr);
    if (tryScoreboardVertex(x, y)) return;
    if (g_currentPicOptionsCaller == PicOptionsCaller::SCR_DrawCrosshair) {
        scaleVertexFromScreenCenter(x, y, crosshairScale);
        return;
    }
    orig_glVertex2f(x, y);
}

void __stdcall my_glVertex2f_PicOptions_1(float x, float y) { handleDrawPicOptionsVertex(x, y); }
void __stdcall my_glVertex2f_PicOptions_2(float x, float y) { handleDrawPicOptionsVertex(x, y); }
void __stdcall my_glVertex2f_PicOptions_3(float x, float y) { handleDrawPicOptionsVertex(x, y); }
void __stdcall my_glVertex2f_PicOptions_4(float x, float y) { handleDrawPicOptionsVertex(x, y); }
#endif

#endif // FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE
