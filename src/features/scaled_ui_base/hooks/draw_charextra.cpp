#include "feature_config.h"

#if FEATURE_SCALED_HUD || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE

#include "util.h"
#include "generated_detours.h"
#include "../shared.h"

void hkDraw_CharExtra(float x, float y, float scale, void* palette, int ch,
    detour_Draw_CharExtra::tDraw_CharExtra original) {
    SOFBUDDY_ASSERT(original != nullptr);
    // TYPEAMATIC / SP_FLAG_TYPEAMATIC -> SCR_DrawCinematicString -> Draw_CharExtra.
    // ref_gl ignores scale for quad size (fixed +8px); scale char positions instead.
    // Anchor Y to the last line. Preserve vanilla bottom margin as a 480p percentage
    // so scaled text stays on-screen at any resolution.
    if (g_cinematicDrawDepth > 0 && fontScale != 1.0f) {
        if (g_cineTextBaseY < 0.0f) {
            g_cineTextBaseY = y;
            const int lines = (g_cineTextLineCount > 0) ? g_cineTextLineCount : 1;
            computeTextBottomAnchor(g_cineTextBaseY, lines, 8.0f,
                g_cineTextBottomY, g_cineTextTargetBottomY);
        }
        applyBottomAnchoredScale(x, y, g_cineTextBottomY, g_cineTextTargetBottomY, fontScale, false);
    }
    original(x, y, scale, palette, ch);
}

#endif // FEATURE_SCALED_HUD || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE
