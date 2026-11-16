/*
    Scaled Scope

    Keeps the sniper scope overlay in its authored 4:3 shape on widescreen
    resolutions, centres it with black side bars, and scales the magnification
    readout with the scope instead of leaving it at stock pixel size.
*/

#include "feature_config.h"

#if FEATURE_SCALED_SCOPE

#include "sof_compat.h"
#include "util.h"
#include "generated_detours.h"
#include "runtime_features.h"
#include "../scaled_ui_base/shared.h"
#include "shared.h"

#include <cstring>

namespace {

struct ScopeFixState {
    bool active = false;
    bool drew = false;
    int width = 0;
    int height = 0;
    int virtualWidth = 0;
    int virtualHeight = 0;
    int originX = 0;
    int originY = 0;
    float textScale = 1.0f;
    int vinettaQuad = 0;
};

ScopeFixState g_scopeFix;

// cScope fields patched while its Draw runs: text offsets, centre and the
// proportionally-sized overlay extents (0x04, 0x08, 0x30..0x58).
const int kScopeFields[] = {1, 2, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22};
int g_savedScopeFields[sizeof(kScopeFields) / sizeof(kScopeFields[0])];

int scale_int(int value, double ratio) {
    return static_cast<int>(static_cast<double>(value) * ratio);
}

void draw_black_bars(void) {
    if (!detour_Draw_FillRGB::oDraw_FillRGB) return;

    const int leftBar = (g_scopeFix.width - g_scopeFix.virtualWidth) / 2;
    const int rightBar = g_scopeFix.width - g_scopeFix.virtualWidth - leftBar;
    const int topBar = (g_scopeFix.height - g_scopeFix.virtualHeight) / 2;
    const int bottomBar = g_scopeFix.height - g_scopeFix.virtualHeight - topBar;

    paletteRGBA_t black = {};
    black.a = 255;

    if (leftBar > 0) {
        detour_Draw_FillRGB::oDraw_FillRGB(0, 0, leftBar, g_scopeFix.height, &black);
    }
    if (rightBar > 0) {
        detour_Draw_FillRGB::oDraw_FillRGB(
            g_scopeFix.width - rightBar, 0, rightBar, g_scopeFix.height, &black);
    }
    if (topBar > 0) {
        detour_Draw_FillRGB::oDraw_FillRGB(0, 0, g_scopeFix.width, topBar, &black);
    }
    if (bottomBar > 0) {
        detour_Draw_FillRGB::oDraw_FillRGB(
            0, g_scopeFix.height - bottomBar, g_scopeFix.width, bottomBar, &black);
    }
}

} // namespace

bool scaled_scope_active(void) {
    return g_scopeFix.active;
}

float scaled_scope_text_scale(void) {
    return (g_scopeFix.active && g_scopeFix.drew) ? g_scopeFix.textScale : 1.0f;
}

void scaled_scope_note_stretch_pic(int& x, int& y, int& w, int& h, const char* name) {
    if (!g_scopeFix.active) return;
    g_scopeFix.drew = true;

    // The four vinetta quads are the black overlay around the scope tube.  The
    // engine sizes them from the full viddef; redirect them to the centred 4:3
    // frame so the vignette does not stretch with the aspect ratio.
    if (!name || !std::strstr(name, "vinette")) return;

    const int leftWidth = g_scopeFix.virtualWidth / 2;
    const int rightWidth = g_scopeFix.virtualWidth - leftWidth;
    const int topHeight = g_scopeFix.virtualHeight / 2;
    const int bottomHeight = g_scopeFix.virtualHeight - topHeight;
    const int left = g_scopeFix.originX;
    const int top = g_scopeFix.originY;
    const int right = left + g_scopeFix.virtualWidth;
    const int bottom = top + g_scopeFix.virtualHeight;

    switch (g_scopeFix.vinettaQuad++ & 3) {
        case 0:
            x = left;
            y = top;
            w = leftWidth;
            h = topHeight;
            break;
        case 1:
            x = right;
            y = top;
            w = -rightWidth;
            h = topHeight;
            break;
        case 2:
            x = left;
            y = bottom;
            w = leftWidth;
            h = -bottomHeight;
            break;
        default:
            x = right;
            y = bottom;
            w = -rightWidth;
            h = -bottomHeight;
            break;
    }
}

void hkcScope_Draw(void* self, detour_cScope_Draw::tcScope_Draw original) {
    SOFBUDDY_ASSERT(self != nullptr);
    SOFBUDDY_ASSERT(original != nullptr);

    if (!RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_SCALED_SCOPE)) {
        original(self);
        return;
    }

    int width = current_vid_w;
    int height = current_vid_h;
    if ((width <= 0 || height <= 0) && viddef_width && viddef_height) {
        width = *viddef_width;
        height = *viddef_height;
    }
    if (width <= 0 || height <= 0 || g_scopeFix.active) {
        original(self);
        return;
    }

    int virtualWidth = width;
    int virtualHeight = height;
    if (static_cast<long long>(width) * 3 >= static_cast<long long>(height) * 4) {
        // Wider than 4:3: height fills the screen, bars go on the sides.
        virtualWidth = static_cast<int>(static_cast<long long>(height) * 4 / 3);
    } else {
        // Narrower than 4:3: width fills the screen, bars go top/bottom.
        virtualHeight = static_cast<int>(static_cast<long long>(width) * 3 / 4);
    }
    if (virtualWidth <= 0 || virtualHeight <= 0) {
        original(self);
        return;
    }

    int* scope = static_cast<int*>(self);
    for (size_t i = 0; i < sizeof(kScopeFields) / sizeof(kScopeFields[0]); ++i)
        g_savedScopeFields[i] = scope[kScopeFields[i]];

    g_scopeFix.active = true;
    g_scopeFix.drew = false;
    g_scopeFix.width = width;
    g_scopeFix.height = height;
    g_scopeFix.virtualWidth = virtualWidth;
    g_scopeFix.virtualHeight = virtualHeight;
    g_scopeFix.originX = (width - virtualWidth) / 2;
    g_scopeFix.originY = (height - virtualHeight) / 2;
    g_scopeFix.textScale = static_cast<float>(virtualHeight) / 480.0f;
    g_scopeFix.vinettaQuad = 0;

    // Keep the overlay centred on the real screen while sizing its parts from
    // the 4:3 design frame.  Text uses this[1]/this[2] as screen offsets, so
    // shifting them also moves the magnification readout with the scope.
    scope[1] += scale_int(width, 300.0 / 640.0) -
                scale_int(virtualWidth, 300.0 / 640.0);
    scope[2] += scale_int(height, 70.0 / 480.0) -
                scale_int(virtualHeight, 70.0 / 480.0);
    scope[12] = width / 2;
    scope[13] = height / 2;
    scope[14] = scale_int(virtualWidth, 0.625);
    scope[15] = scale_int(virtualHeight, 2.0 / 3.0);
    scope[16] = scale_int(virtualWidth, 0.2);
    scope[17] = scale_int(virtualHeight, 4.0 / 15.0);
    scope[18] = scale_int(virtualWidth, 0.1953125);
    scope[19] = scale_int(virtualHeight, 0.2604166666666667);
    scope[20] = scale_int(virtualWidth, 0.003125);
    scope[21] = scale_int(virtualHeight, 0.004166666666666667);
    scope[22] = scale_int(virtualHeight, 0.008333333333333333);

    original(self);

    if (g_scopeFix.drew) draw_black_bars();

    for (size_t i = 0; i < sizeof(kScopeFields) / sizeof(kScopeFields[0]); ++i)
        scope[kScopeFields[i]] = g_savedScopeFields[i];

    g_scopeFix.active = false;
    g_scopeFix.drew = false;
    g_scopeFix.vinettaQuad = 0;
}

#endif // FEATURE_SCALED_SCOPE
