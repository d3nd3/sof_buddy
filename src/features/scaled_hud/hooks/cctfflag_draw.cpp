#include "feature_config.h"

#if FEATURE_SCALED_HUD

#include "sof_compat.h"
#include "util.h"
#include "generated_detours.h"
#include "../../scaled_ui_base/shared.h"

void __thiscall hkcCtfFlag_Draw(void * self, detour_cCtfFlag_Draw::tcCtfFlag_Draw original) {
    g_activeRenderType = uiRenderType::HudCtfFlag;
	original(self);
    g_activeRenderType = uiRenderType::None;
}

#endif // FEATURE_SCALED_HUD

