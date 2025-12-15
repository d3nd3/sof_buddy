#include "feature_config.h"

#if FEATURE_SCALED_CON

#include "sof_compat.h"
#include "util.h"
#include "generated_detours.h"
#include "../../scaled_ui_base/shared.h"

// #pragma GCC push_options
// #pragma GCC optimize ("O0")
void hkCon_DrawConsole(float frac, detour_Con_DrawConsole::tCon_DrawConsole original) {
	g_activeRenderType = uiRenderType::Console;

	if (frac == 0.5 && *cls_state == 8) {
		frac = consoleSize / fontScale;
		draw_con_frac = consoleSize;
	} else {
		draw_con_frac = frac;
		frac = frac / fontScale;
	}

	original(frac);
	g_activeRenderType = uiRenderType::None;
}
// #pragma GCC pop_options

#endif // FEATURE_SCALED_CON

