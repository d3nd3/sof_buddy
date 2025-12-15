#include "feature_config.h"

#if FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_TEXT || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE

#include "sof_compat.h"
#include "util.h"
#include "generated_detours.h"
#include "../../scaled_ui_base/shared.h"
#include "debug/hook_callsite.h"

// #pragma GCC push_options
// #pragma GCC optimize ("O0")
void hkDraw_StretchPic(int x, int y, int w, int h, int palette, char * name, int flags, detour_Draw_StretchPic::tDraw_StretchPic original) {
    g_activeDrawCall = DrawRoutineType::StretchPic;
    StretchPicCaller detectedCaller = StretchPicCaller::Unknown;

    uint32_t fnStart = HookCallsite::recordAndGetFnStartExternal("Draw_StretchPic");
    if (fnStart) {
        detectedCaller = getStretchPicCallerFromRva(fnStart);
    }
    
    if (g_currentStretchPicCaller == StretchPicCaller::Unknown && detectedCaller != StretchPicCaller::Unknown) {
        g_currentStretchPicCaller = detectedCaller;
    }
    
    StretchPicCaller caller = g_currentStretchPicCaller;
    
    if (caller == StretchPicCaller::CON_DrawConsole) {
        //Draw the console background with new height
        extern float draw_con_frac;
        int consoleHeight = draw_con_frac * current_vid_h;
        y = -1 * current_vid_h + consoleHeight;
        original(x, y, w, h, palette, name, flags);
        extern void( * orig_SRC_AddDirtyPoint)(int x, int y);
        orig_SRC_AddDirtyPoint(0, 0);
        orig_SRC_AddDirtyPoint(w - 1, consoleHeight - 1);
        g_currentStretchPicCaller = StretchPicCaller::Unknown;
        g_activeDrawCall = DrawRoutineType::None;
        return;
    }
    
    if (g_activeRenderType == uiRenderType::HudCtfFlag) {
        w = w * hudScale;
        h = h * hudScale;
    }

    original(x, y, w, h, palette, name, flags);
    

    g_currentStretchPicCaller = StretchPicCaller::Unknown;
    g_activeDrawCall = DrawRoutineType::None;
}
// #pragma GCC pop_options

#endif // FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_TEXT || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE

