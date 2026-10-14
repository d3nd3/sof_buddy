#include "feature_config.h"

#if FEATURE_SCALED_HUD

#include "util.h"
#include "generated_detours.h"
#include "../../scaled_ui_base/shared.h"
#include "../../scaled_ui_base/caller_from.h"

void hkcMissionStatus_Draw(void * self, detour_cMissionStatus_Draw::tcMissionStatus_Draw original) {
    SOFBUDDY_ASSERT(original != nullptr);

    g_currentFontCaller = FontCaller::MissionStatus;
    original(self);
    g_currentFontCaller = FontCaller::Unknown;
}

#endif // FEATURE_SCALED_HUD
