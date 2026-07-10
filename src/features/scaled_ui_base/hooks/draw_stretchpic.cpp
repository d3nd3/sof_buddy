#include "feature_config.h"

#if FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE

#include "sof_compat.h"
#include "util.h"
#include "generated_detours.h"
#include "../../scaled_ui_base/shared.h"
#include "debug/hook_callsite.h"

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
        g_currentStretchPicCaller == StretchPicCaller::CON_DrawConsole
        #if FEATURE_SCALED_CON
        || g_activeRenderType == uiRenderType::Console
        #endif
        ;
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
        SOFBUDDY_ASSERT(orig_SRC_AddDirtyPoint != nullptr);
        if (consoleHeight > 0) {
            orig_SRC_AddDirtyPoint(0, 0);
            orig_SRC_AddDirtyPoint(picW - 1, consoleHeight - 1);
        }
        g_currentStretchPicCaller = StretchPicCaller::Unknown;
        g_activeDrawCall = DrawRoutineType::None;
        resetGlVertexQuadState();
        return;
        #else
        // FEATURE_SCALED_CON not enabled, just call original
        original(x, y, w, h, palette, name, flags);
        g_currentStretchPicCaller = StretchPicCaller::Unknown;
        g_activeDrawCall = DrawRoutineType::None;
        resetGlVertexQuadState();
        return;
        #endif
    }
    
    if (g_activeRenderType == uiRenderType::HudCtfFlag) {
        SOFBUDDY_ASSERT(hudScale > 0.0f);
        w = w * hudScale;
        h = h * hudScale;
    }

    original(x, y, w, h, palette, name, flags);
    

    g_currentStretchPicCaller = StretchPicCaller::Unknown;
    g_activeDrawCall = DrawRoutineType::None;
    resetGlVertexQuadState();

    // PrintOut(PRINT_LOG, "hkDraw_StretchPic End: caller=%d\n", caller);
}
// #pragma GCC pop_options

#endif // FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE

