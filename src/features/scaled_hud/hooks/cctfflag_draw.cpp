#include "feature_config.h"

#if FEATURE_SCALED_HUD

#include "sof_compat.h"
#include "util.h"
#include "generated_detours.h"
#include "../../scaled_ui_base/shared.h"

using detour_cCtfFlag_Draw::ocCtfFlag_Draw;

void __thiscall hkcCtfFlag_Draw(void * self, detour_cCtfFlag_Draw::tcCtfFlag_Draw original) {
	if (!ocCtfFlag_Draw && original) ocCtfFlag_Draw = original;
    g_activeRenderType = uiRenderType::HudCtfFlag;
    ocCtfFlag_Draw(self);
    g_activeRenderType = uiRenderType::None;
}

#endif // FEATURE_SCALED_HUD

