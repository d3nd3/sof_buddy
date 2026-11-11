#include "feature_config.h"

#if FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE

#include "sof_compat.h"
#include "util.h"
#include "generated_detours.h"
#include "../../scaled_ui_base/shared.h"
#include "../../scaled_ui_base/ref_gl_state.h"
#include "debug/hook_callsite.h"
#include "runtime_features.h"
#include <cmath>
#include <cstring>
#if FEATURE_MORE_CROSSHAIRS
#include "features/more_crosshairs/shared.h"
#endif

void hkDraw_Pic(int x, int y, char const * imgname, int palette, detour_Draw_Pic::tDraw_Pic original) {
    SOFBUDDY_ASSERT(original != nullptr);
    resetGlVertexQuadState();
    g_activeDrawCall = DrawRoutineType::Pic;
    if (g_currentPicCaller == PicCaller::Unknown) {
        uint32_t fnStart = HookCallsite::recordAndGetFnStartExternal("Draw_Pic");
        if (fnStart) g_currentPicCaller = getPicCallerFromRva(fnStart);
    }
#if FEATURE_MORE_CROSSHAIRS
    if (g_currentPicCaller == PicCaller::SCR_DrawCrosshair) more_crosshairs_before_draw_crosshair();
#endif
    if (imgname != nullptr &&
        RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_SCALED_MENU)) {
        const char* p = imgname;
        int slashes = 0;
        while (*p && slashes < 2) {
            if (*p++ == '/') slashes++;
        }
        if (slashes == 2 && *p) {
            constexpr char kBackdropPic[] = "backdrop/web_bg";
            if (std::strncmp(p, kBackdropPic, sizeof(kBackdropPic) - 1) == 0 &&
                (p[sizeof(kBackdropPic) - 1] == '\0' ||
                 p[sizeof(kBackdropPic) - 1] == '.' ||
                 p[sizeof(kBackdropPic) - 1] == '/')) {
                mainMenuBgTiled = true;
#if FEATURE_SCALED_HUD
    void* image = nullptr;
    int* image_texnum = nullptr;
    int saved_texnum = 0;
    unsigned int proxy_texnum = 0;
    float proxy_scale = 0.0f;
    if (g_activeRenderType == uiRenderType::Scoreboard || g_activeRenderType == uiRenderType::HudCtfFlag)
        proxy_scale = hudScale;
    else if (g_currentPicCaller == PicCaller::SCR_DrawCrosshair)
        proxy_scale = crosshairScale;
    if (proxy_scale > 0.0f) {
        image = scaled_ui_find_image(imgname);
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

    original(x, y, imgname, palette);
#if FEATURE_SCALED_HUD
    if (proxy_texnum && image_texnum)
        *image_texnum = saved_texnum;
#endif
                mainMenuBgTiled = false;
                g_currentPicCaller = PicCaller::Unknown;
                g_activeDrawCall = DrawRoutineType::None;
                resetGlVertexQuadState();
                return;
            }
        }
    }

#if FEATURE_MORE_CROSSHAIRS
    if (imgname) {
        const char* live = more_crosshairs_menu_preview_pic_name(imgname);
        if (live) imgname = live;
    }
#endif
#if FEATURE_SCALED_MENU
    // <selection>'s Draw() emits its inventory icons through Draw_Pic at the stock
    // 1:1 size. Render those as stretched quads instead so the images themselves
    // grow with the menu, not just the selection's container.
    if (g_scaledMenuSelectionDrawActive && imgname && detour_Draw_StretchPic::oDraw_StretchPic) {
        const float s = g_scaledMenuSelectionScale;
        if (s > 1.001f) {
            void* image = scaled_ui_find_image(imgname);
            if (image) {
                const int iw = *reinterpret_cast<short*>(static_cast<char*>(image) + 0x44);
                const int ih = *reinterpret_cast<short*>(static_cast<char*>(image) + 0x46);
                if (iw > 0 && ih > 0) {
                    const float ox = static_cast<float>(g_scaledMenuSelectionOriginX);
                    const float oy = static_cast<float>(g_scaledMenuSelectionOriginY);
                    const int sx = static_cast<int>(std::lround(
                        ox + (static_cast<float>(x) - ox) * s));
                    const int sy = static_cast<int>(std::lround(
                        oy + (static_cast<float>(y) - oy) * s));
                    const int sw = static_cast<int>(std::lround(static_cast<float>(iw) * s));
                    const int sh = static_cast<int>(std::lround(static_cast<float>(ih) * s));
                    int pal = palette;
                    using tDrawStretchPic = detour_Draw_StretchPic::tDraw_StretchPic;
                    reinterpret_cast<tDrawStretchPic>(detour_Draw_StretchPic::oDraw_StretchPic)(
                        sx, sy, sw, sh, static_cast<int>(reinterpret_cast<intptr_t>(&pal)),
                        const_cast<char*>(imgname), 0);
                    g_currentPicCaller = PicCaller::Unknown;
                    g_activeDrawCall = DrawRoutineType::None;
                    resetGlVertexQuadState();
                    return;
                }
            }
        }
    }
#endif
    original(x, y, imgname, palette);
#if FEATURE_SCALED_HUD
    // Draw_Pic enables GL_BLEND but never clears it (unlike Draw_StretchPic).
    // client_sb rows end with Draw_Pic; next row's Draw_String_Color/Draw_Char
    // must start with blend off or text chars render inconsistently soft.
    if (RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_SCALED_HUD) &&
        g_activeRenderType == uiRenderType::Scoreboard)
        ref_gl_cache_disable_blend();
#endif
    g_currentPicCaller = PicCaller::Unknown;
    g_activeDrawCall = DrawRoutineType::None;
    resetGlVertexQuadState();
}

#endif // FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE

