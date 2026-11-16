#include "feature_config.h"

#if FEATURE_SCALED_HUD

#include "util.h"
#include "generated_detours.h"
#include "../../scaled_ui_base/shared.h"

void hkcCountdown_Draw(void * self, detour_cCountdown_Draw::tcCountdown_Draw original) {
    SOFBUDDY_ASSERT(self != nullptr);
    SOFBUDDY_ASSERT(original != nullptr);

    g_currentFontCaller = FontCaller::Countdown;
    original(self);
    g_currentFontCaller = FontCaller::Unknown;
}

#endif // FEATURE_SCALED_HUD
