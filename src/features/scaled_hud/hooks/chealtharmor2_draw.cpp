#include "feature_config.h"

#if FEATURE_SCALED_HUD

#include "sof_compat.h"
#include "util.h"
#include "generated_detours.h"
#include "../../scaled_ui_base/shared.h"

using detour_cHealthArmor2_Draw::ocHealthArmor2_Draw;

void __thiscall hkcHealthArmor2_Draw(void * self, detour_cHealthArmor2_Draw::tcHealthArmor2_Draw original) {
	if (!ocHealthArmor2_Draw && original) ocHealthArmor2_Draw = original;

    g_activeRenderType = uiRenderType::HudHealthArmor;
    ocHealthArmor2_Draw(self);
    g_activeRenderType = uiRenderType::None;
}

#endif // FEATURE_SCALED_HUD

