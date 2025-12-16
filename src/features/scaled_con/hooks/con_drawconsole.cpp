#include "feature_config.h"

#if FEATURE_SCALED_CON

#include "sof_compat.h"
#include "util.h"
#include "generated_detours.h"
#include "../../scaled_ui_base/shared.h"

// #pragma GCC push_options
// #pragma GCC optimize ("O0")
void hkCon_DrawConsole(float frac, detour_Con_DrawConsole::tCon_DrawConsole original) {
	SOFBUDDY_ASSERT(cls_state != nullptr);
	SOFBUDDY_ASSERT(fontScale > 0.0f);
	SOFBUDDY_ASSERT(frac >= 0.0f && frac <= 1.0f);
	SOFBUDDY_ASSERT(consoleSize >= 0.0f && consoleSize <= 1.0f);
	
	g_activeRenderType = uiRenderType::Console;

	if (frac == 0.5 && *cls_state == 8) {
		frac = consoleSize / fontScale;
		draw_con_frac = consoleSize;
	} else {
		draw_con_frac = frac;
		frac = frac / fontScale;
	}

	SOFBUDDY_ASSERT(frac >= 0.0f);
	original(frac);
	g_activeRenderType = uiRenderType::None;
}
// #pragma GCC pop_options

#endif // FEATURE_SCALED_CON

