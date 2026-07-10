#include "feature_config.h"

#if FEATURE_SCALED_HUD || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE

#include "util.h"
#include "generated_detours.h"
#include "../shared.h"

void hkSCR_DrawCinematicString(int speed, int x, int y, detour_SCR_DrawCinematicString::tSCR_DrawCinematicString original) {
    SOFBUDDY_ASSERT(original != nullptr);

    resetGlVertexQuadState();
    g_activeRenderType = uiRenderType::Cinematic;
    original(speed, x, y);
    g_activeRenderType = uiRenderType::None;
    resetGlVertexQuadState();
}

#endif // FEATURE_SCALED_HUD || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE
