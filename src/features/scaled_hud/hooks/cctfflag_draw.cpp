#include "feature_config.h"

#if FEATURE_SCALED_HUD

#include "util.h"
#include "generated_detours.h"
#include "../../scaled_ui_base/shared.h"

void hkcCtfFlag_Draw(void * self, detour_cCtfFlag_Draw::tcCtfFlag_Draw original) {
    SOFBUDDY_ASSERT(self != nullptr);
    SOFBUDDY_ASSERT(original != nullptr);

    g_activeRenderType = uiRenderType::HudCtfFlag;
	original(self);
    g_activeRenderType = uiRenderType::None;
}

#endif // FEATURE_SCALED_HUD

