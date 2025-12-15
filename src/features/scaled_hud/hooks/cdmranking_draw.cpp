#include "feature_config.h"

#if FEATURE_SCALED_HUD

#include "sof_compat.h"
#include "util.h"
#include "generated_detours.h"
#include "../../scaled_ui_base/shared.h"

void __thiscall hkcDMRanking_Draw(void * self, detour_cDMRanking_Draw::tcDMRanking_Draw original) {
    // Top right of screen, your score and rank
    // So relative to top and right edge

    g_activeRenderType = uiRenderType::HudDmRanking;
	original(self);
    g_activeRenderType = uiRenderType::None;
    hudDmRanking_wasImage = false;
}

#endif // FEATURE_SCALED_HUD

