#include "feature_config.h"

#if FEATURE_SCALED_HUD

#include "sof_compat.h"
#include "util.h"
#include "generated_detours.h"
#include "../../scaled_ui_base/shared.h"

void hkSCR_ExecuteLayoutString(char * text, detour_SCR_ExecuteLayoutString::tSCR_ExecuteLayoutString original) {
    g_activeRenderType = uiRenderType::Scoreboard;
    original(text);
    g_activeRenderType = uiRenderType::None;
}

#endif // FEATURE_SCALED_HUD

