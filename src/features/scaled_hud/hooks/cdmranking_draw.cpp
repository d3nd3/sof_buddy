#include "feature_config.h"

#if FEATURE_SCALED_HUD

#include "util.h"
#include "generated_detours.h"
#include "../../scaled_ui_base/shared.h"

void hkcDMRanking_Draw(void * self, detour_cDMRanking_Draw::tcDMRanking_Draw original) {
    SOFBUDDY_ASSERT(self != nullptr);
    SOFBUDDY_ASSERT(original != nullptr);

    g_activeRenderType = uiRenderType::HudDmRanking;
	original(self);
    g_activeRenderType = uiRenderType::None;
    hudDmRanking_wasImage = false;
}

#endif // FEATURE_SCALED_HUD

