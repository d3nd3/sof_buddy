/*
    Scaled Scope

    Keeps the sniper scope overlay in its authored 4:3 shape on widescreen
    resolutions, centres it with black side bars, and scales the magnification
    readout with the scope instead of leaving it at stock pixel size.

    cScope::Draw positions every overlay element from viddef_width/viddef_height
    (full screen). The fix remaps each draw into a centred 4:3 virtual frame:
    coordinates are scaled about the screen centre, sizes are scaled by the same
    ratio, and the magnification text is remapped and scaled with the run.
*/

#include "feature_config.h"

#if FEATURE_SCALED_SCOPE

#include "sof_compat.h"
#include "util.h"
#include "generated_detours.h"
#include "runtime_features.h"
#include "../scaled_ui_base/shared.h"
#include "shared.h"

#include <cmath>
#include <cstring>

namespace {

struct ScopeFixState {
    bool active = false;
    bool drew = false;
    int width = 0;
    int height = 0;
    int virtualWidth = 0;
    int virtualHeight = 0;
    float textScale = 1.0f;
};

ScopeFixState g_scopeFix;

int remap_coord(int value, int center, int virtualSize, int size) {
    if (size <= 0) return value;
    const double s = static_cast<double>(virtualSize) / static_cast<double>(size);
    return center + static_cast<int>(std::lround((value - center) * s));
}

int remap_size(int value, int virtualSize, int size) {
    if (size <= 0) return value;
    const double s = static_cast<double>(virtualSize) / static_cast<double>(size);
    return static_cast<int>(std::lround(static_cast<double>(value) * s));
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
    return g_scopeFix.active ? g_scopeFix.textScale : 1.0f;
}

void scaled_scope_note_stretch_pic(int& x, int& y, int& w, int& h, const char* name) {
    if (!g_scopeFix.active) return;
    g_scopeFix.drew = true;

    const int cx = g_scopeFix.width / 2;
    const int cy = g_scopeFix.height / 2;

    // The four crosshair bars are the only overlay quads with hard-coded design
    // sizes: the engine draws them 16 px thick with an 8 px centre gap, which
    // does not scale with the resolution. Their long axes are viddef-based, so
    // remap those normally; scale the fixed thickness/gap by the vertical design
    // scale instead.
    if (name && std::strstr(name, "retic")) {
        const double s = static_cast<double>(g_scopeFix.textScale);
        const double sx = static_cast<double>(g_scopeFix.virtualWidth) / g_scopeFix.width;
        const double sy = static_cast<double>(g_scopeFix.virtualHeight) / g_scopeFix.height;
        const int aw = w < 0 ? -w : w;
        const int ah = h < 0 ? -h : h;
        if (aw > ah) {
            x = remap_coord(x, cx, g_scopeFix.virtualWidth, g_scopeFix.width);
            w = remap_size(w, g_scopeFix.virtualWidth, g_scopeFix.width);
            y = remap_coord(y, cy, g_scopeFix.virtualHeight, g_scopeFix.height) -
                static_cast<int>(std::lround(8.0 * (s - sy)));
            h = static_cast<int>(std::lround(static_cast<double>(h) * s));
        } else {
            y = remap_coord(y, cy, g_scopeFix.virtualHeight, g_scopeFix.height);
            h = remap_size(h, g_scopeFix.virtualHeight, g_scopeFix.height);
            x = remap_coord(x, cx, g_scopeFix.virtualWidth, g_scopeFix.width) -
                static_cast<int>(std::lround(8.0 * (s - sx)));
            w = static_cast<int>(std::lround(static_cast<double>(w) * s));
        }
        return;
    }

    x = remap_coord(x, cx, g_scopeFix.virtualWidth, g_scopeFix.width);
    y = remap_coord(y, cy, g_scopeFix.virtualHeight, g_scopeFix.height);
    w = remap_size(w, g_scopeFix.virtualWidth, g_scopeFix.width);
    h = remap_size(h, g_scopeFix.virtualHeight, g_scopeFix.height);
}

void scaled_scope_remap_text(int& x, int& y) {
    if (!g_scopeFix.active) return;
    g_scopeFix.drew = true;

    const float sy = static_cast<float>(g_scopeFix.virtualHeight) /
                     static_cast<float>(g_scopeFix.height);
    x = remap_coord(x, g_scopeFix.width / 2, g_scopeFix.virtualWidth, g_scopeFix.width);
    y = remap_coord(y, g_scopeFix.height / 2, g_scopeFix.virtualHeight, g_scopeFix.height);
    // cScope::Draw places the readout 8 px above the gauge with a fixed pixel
    // offset; scale that offset with the readout so its spacing matches.
    y -= static_cast<int>(std::lround(
        8.0 * (static_cast<double>(g_scopeFix.textScale) - sy)));
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

    g_scopeFix.active = true;
    g_scopeFix.drew = false;
    g_scopeFix.width = width;
    g_scopeFix.height = height;
    g_scopeFix.virtualWidth = virtualWidth;
    g_scopeFix.virtualHeight = virtualHeight;
    g_scopeFix.textScale = static_cast<float>(virtualHeight) / 480.0f;

    // cScope::Draw reads only viddef_* and its own fields; the Draw_StretchPic,
    // R_DrawFont and FillRGB hooks remap what it emits into the 4:3 frame while
    // this flag is set.
    original(self);

    // cScope::Draw returns early when the scope is not zoomed. Only frame the
    // screen once it actually emitted overlay geometry; otherwise the black
    // bars would shrink the normal widescreen view with the scope inactive.
    if (g_scopeFix.drew) draw_black_bars();

    g_scopeFix.active = false;
    g_scopeFix.drew = false;
}

#endif // FEATURE_SCALED_SCOPE
