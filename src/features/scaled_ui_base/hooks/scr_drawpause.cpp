#include "feature_config.h"

#if FEATURE_SCALED_HUD || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE

#include "util.h"
#include "generated_detours.h"
#include "../shared.h"
#include "runtime_features.h"

void hkSCR_DrawPause(detour_SCR_DrawPause::tSCR_DrawPause original) {
    SOFBUDDY_ASSERT(original != nullptr);
    if (!RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_SCALED_HUD) &&
        !RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_SCALED_MENU)) {
        original();
        return;
    }

    resetGlVertexQuadState();
    g_currentFontCaller = FontCaller::SCRDrawPause;
    original();
    g_currentFontCaller = FontCaller::Unknown;
    resetGlVertexQuadState();
}

#endif // FEATURE_SCALED_HUD || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE
