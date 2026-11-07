#include "feature_config.h"

#if FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE

#include "sof_compat.h"
#include "util.h"
#include "generated_detours.h"
#include "../../scaled_ui_base/shared.h"
#include "debug/hook_callsite.h"
#include "runtime_features.h"
#if FEATURE_MORE_CROSSHAIRS
#include "features/more_crosshairs/shared.h"
#endif

// #pragma GCC push_options
// #pragma GCC optimize ("O0")
void hkDraw_StretchPic(int x, int y, int w, int h, int palette, char * name, int flags, detour_Draw_StretchPic::tDraw_StretchPic original) {

    // PrintOut(PRINT_LOG, "hkDraw_StretchPic: x=%d, y=%d, w=%d, h=%d, palette=%d, name=%s, flags=%d\n", x, y, w, h, palette, name, flags);

    SOFBUDDY_ASSERT(original != nullptr);
    SOFBUDDY_ASSERT(w > 0);
    SOFBUDDY_ASSERT(h > 0);
    SOFBUDDY_ASSERT(current_vid_h > 0);
    resetGlVertexQuadState();
    g_activeDrawCall = DrawRoutineType::StretchPic;
    if (g_currentStretchPicCaller == StretchPicCaller::Unknown) {
        uint32_t fnStart = HookCallsite::recordAndGetFnStartExternal("Draw_StretchPic");
        if (fnStart) g_currentStretchPicCaller = getStretchPicCallerFromRva(fnStart);
    }
    const bool consoleBackground =
#if FEATURE_SCALED_CON
        RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_SCALED_CON) &&
        (g_currentStretchPicCaller == StretchPicCaller::CON_DrawConsole ||
         g_activeRenderType == uiRenderType::Console);
#else
        false;
#endif
    if (consoleBackground) {
        #if FEATURE_SCALED_CON
        extern float draw_con_frac;
        SOFBUDDY_ASSERT(draw_con_frac >= 0.0f && draw_con_frac <= 1.0f);
        const int consoleHeight = (int)(draw_con_frac * current_vid_h);
        const int picH = current_vid_h > 0 ? current_vid_h : h;
        const int picW = current_vid_w > 0 ? current_vid_w : w;
        // viddef_width is narrowed during Con_DrawConsole for text layout; background must stay full width
        y = -picH + (consoleHeight > 0 ? consoleHeight : 1);
        original(0, y, picW, picH, palette, name, flags);
        extern void( * orig_SRC_AddDirtyPoint)(int x, int y);
        if (orig_SRC_AddDirtyPoint && consoleHeight > 0) {
            orig_SRC_AddDirtyPoint(0, 0);
            orig_SRC_AddDirtyPoint(picW - 1, consoleHeight - 1);
        }
        g_currentStretchPicCaller = StretchPicCaller::Unknown;
        g_activeDrawCall = DrawRoutineType::None;
        resetGlVertexQuadState();
        return;
        #else
        // FEATURE_SCALED_CON not enabled, just call original
    #if FEATURE_SCALED_HUD
    void* image = nullptr;
    int* image_texnum = nullptr;
    int saved_texnum = 0;
    unsigned int proxy_texnum = 0;
    float proxy_scale = 0.0f;
    if (g_activeRenderType == uiRenderType::Scoreboard || g_activeRenderType == uiRenderType::HudCtfFlag)
        proxy_scale = hudScale;
    else if (g_currentStretchPicCaller == StretchPicCaller::TeamIconDraw && g_iconsAutoscale)
        proxy_scale = hudScale;
    if (proxy_scale > 0.0f && name) {
        image = scaled_ui_find_image(name);
        if (image) {
            proxy_texnum = scaled_ui_image_proxy_texnum(image, proxy_scale);
            if (proxy_texnum) {
                image_texnum = reinterpret_cast<int*>(static_cast<char*>(image) + 80);
                saved_texnum = *image_texnum;
                *image_texnum = static_cast<int>(proxy_texnum);
            }
        }
    }
#endif

    original(x, y, w, h, palette, name, flags);
#if FEATURE_SCALED_HUD
    if (proxy_texnum && image_texnum)
        *image_texnum = saved_texnum;
#endif
        g_currentStretchPicCaller = StretchPicCaller::Unknown;
        g_activeDrawCall = DrawRoutineType::None;
        resetGlVertexQuadState();
        return;
        #endif
    }
    
#if FEATURE_SCALED_HUD
    extern bool g_iconsAutoscale;
    if (RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_SCALED_HUD)) {
        if (g_activeRenderType == uiRenderType::HudCtfFlag) {
            SOFBUDDY_ASSERT(hudScale > 0.0f);
            w = w * hudScale;
            h = h * hudScale;
        }

        if (g_currentStretchPicCaller == StretchPicCaller::TeamIconDraw) {
            // Team icon pic drawn by ref_gl Draw_PlayerInfo above a player. Scale with the
            // HUD scale and recenter on the original quad so it stays over the player.
            extern bool g_iconsAutoscale;
            if (g_iconsAutoscale && hudScale > 0.0f && hudScale != 1.0f) {
                const int nw = (w * hudScale >= 1.0f) ? (int)(w * hudScale) : 1;
                const int nh = (h * hudScale >= 1.0f) ? (int)(h * hudScale) : 1;
                x += (w - nw) / 2;
                y += (h - nh) / 2;
                w = nw;
                h = nh;
            }
        }
    }
#endif

#if FEATURE_MORE_CROSSHAIRS
    if (name) {
        const char* live = more_crosshairs_menu_preview_pic_name(name);
        if (live) name = const_cast<char*>(live);
    }
#endif
    original(x, y, w, h, palette, name, flags);

    g_currentStretchPicCaller = StretchPicCaller::Unknown;
    g_activeDrawCall = DrawRoutineType::None;
    resetGlVertexQuadState();

    // PrintOut(PRINT_LOG, "hkDraw_StretchPic End: caller=%d\n", caller);
}
// #pragma GCC pop_options

#endif // FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE

