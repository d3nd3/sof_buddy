#include "feature_config.h"

#if FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_TEXT || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE

#include "sof_compat.h"
#include "util.h"
#include "generated_detours.h"
#include "../../scaled_ui_base/shared.h"
#include "debug/hook_callsite.h"

void hkDraw_Pic(int x, int y, char const * imgname, int palette, detour_Draw_Pic::tDraw_Pic original) {
    SOFBUDDY_ASSERT(original != nullptr);
    
    g_activeDrawCall = DrawRoutineType::Pic;
    PicCaller detectedCaller = PicCaller::Unknown;
    uint32_t fnStart = HookCallsite::recordAndGetFnStartExternal("Draw_Pic");
    if (fnStart) {
        detectedCaller = getPicCallerFromRva(fnStart);
    }
    
    if (g_currentPicCaller == PicCaller::Unknown && detectedCaller != PicCaller::Unknown) {
        g_currentPicCaller = detectedCaller;
    }
    
    if (imgname != nullptr) {
        const char* p = imgname;
        int slashes = 0;
        while (*p && slashes < 2) {
            if (*p++ == '/') slashes++;
        }
        if (slashes == 2 && *p) {
            if (p[0] == 'b' && p[1] == 'a' && p[2] == 'c' && p[3] == 'k' &&
                p[4] == 'd' && p[5] == 'o' && p[6] == 'p' && p[7] == 'e' &&
                p[8] == '/') {
                p += 9;
                if (p[0] == 'w' && p[1] == 'e' && p[2] == 'b' && p[3] == '_' &&
                    p[4] == 'b' && p[5] == 'g' && (p[6] == '.' || p[6] == '\0' || p[6] == '/')) {
                    mainMenuBgTiled = true;
                    original(x, y, imgname, palette);
                    mainMenuBgTiled = false;
                    g_currentPicCaller = PicCaller::Unknown;
                    g_activeDrawCall = DrawRoutineType::None;
                    return;
                }
            }
        }
    }

    original(x, y, imgname, palette);
    g_currentPicCaller = PicCaller::Unknown;
    g_activeDrawCall = DrawRoutineType::None;
}

#endif // FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_TEXT || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE

