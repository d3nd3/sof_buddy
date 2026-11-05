#include "feature_config.h"

#if FEATURE_INTRO_SCALER

#ifdef UI_MENU

#include "sof_compat.h"
#include "util.h"
#include "../shared.h"

void introScaler_EarlyStartup(void) {
	PrintOut(PRINT_LOG, "intro_scaler: installing intro/disclaimer hooks\n");
	if (!intro_scaler_install_hooks())
		PrintOut(PRINT_BAD, "intro_scaler: hook install failed\n");
}

#endif // UI_MENU

#endif // FEATURE_INTRO_SCALER
