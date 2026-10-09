#include "feature_config.h"

#if FEATURE_SCALED_HUD || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE

#include "util.h"
#include "generated_detours.h"
#include "../shared.h"
#include "runtime_features.h"

void hkSCR_DrawCinemaScope(detour_SCR_DrawCinemaScope::tSCR_DrawCinemaScope original) {
    SOFBUDDY_ASSERT(original != nullptr);
    if (!RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_SCALED_HUD)) {
        original();
        return;
    }

    resetGlVertexQuadState();
    g_currentPicCaller = PicCaller::SCR_DrawCinemaScope;
    original();
    g_currentPicCaller = PicCaller::Unknown;
    resetGlVertexQuadState();
}

#endif // FEATURE_SCALED_HUD || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE
