#include "feature_config.h"

#if FEATURE_SCALED_HUD

#include "util.h"
#include "generated_detours.h"
#include "../../scaled_ui_base/shared.h"
#include "../../scaled_ui_base/ref_gl_state.h"

void hkSCR_ExecuteLayoutString(char * text, detour_SCR_ExecuteLayoutString::tSCR_ExecuteLayoutString original) {
    SOFBUDDY_ASSERT(original != nullptr);

    resetGlVertexQuadState();
    ref_gl_cache_disable_blend();
    ref_gl_cache_sync();
    g_activeRenderType = uiRenderType::Scoreboard;
    original(text);
    g_activeRenderType = uiRenderType::None;
    resetGlVertexQuadState();
}

#endif // FEATURE_SCALED_HUD
