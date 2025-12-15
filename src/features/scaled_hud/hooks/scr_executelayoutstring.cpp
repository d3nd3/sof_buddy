#include "feature_config.h"

#if FEATURE_SCALED_HUD

#include "sof_compat.h"
#include "util.h"
#include "generated_detours.h"
#include "../../scaled_ui_base/shared.h"

using detour_SCR_ExecuteLayoutString::oSCR_ExecuteLayoutString;

void hkSCR_ExecuteLayoutString(char * text, detour_SCR_ExecuteLayoutString::tSCR_ExecuteLayoutString original) {
	if (!oSCR_ExecuteLayoutString && original) oSCR_ExecuteLayoutString = original;
    g_activeRenderType = uiRenderType::Scoreboard;
    oSCR_ExecuteLayoutString(text);
    g_activeRenderType = uiRenderType::None;
}

#endif // FEATURE_SCALED_HUD

