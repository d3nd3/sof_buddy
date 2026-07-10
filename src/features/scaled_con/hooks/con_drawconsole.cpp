#include "feature_config.h"

#if FEATURE_SCALED_CON

#include "sof_compat.h"
#include "util.h"
#include "generated_detours.h"
#include "../../scaled_ui_base/shared.h"

extern void (*orig_SRC_AddDirtyPoint)(int x, int y);

// #pragma GCC push_options
// #pragma GCC optimize ("O0")
void hkCon_DrawConsole(float frac, detour_Con_DrawConsole::tCon_DrawConsole original) {
	SOFBUDDY_ASSERT(cls_state != nullptr);
	SOFBUDDY_ASSERT(viddef_width != nullptr);
	SOFBUDDY_ASSERT(fontScale > 0.0f);
	SOFBUDDY_ASSERT(frac >= 0.0f && frac <= 1.0f);
	SOFBUDDY_ASSERT(consoleSize >= 0.0f && consoleSize <= 1.0f);
	
	resetGlVertexQuadState();
	g_activeRenderType = uiRenderType::Console;
	g_currentStretchPicCaller = StretchPicCaller::CON_DrawConsole;

	if (frac == 0.5 && *cls_state == 8) {
		frac = consoleSize / fontScale;
		draw_con_frac = consoleSize;
	} else {
		draw_con_frac = frac;
		frac = frac / fontScale;
	}

	SOFBUDDY_ASSERT(frac >= 0.0f);
	real_refdef_width = current_vid_w;
	*viddef_width = (int)(current_vid_w / fontScale);
	original(frac);
	*viddef_width = real_refdef_width;

	// scaledcon_early NOPs CON_DrawConsole's SCR_AddDirtyPoint calls; ensure repaint
	if (orig_SRC_AddDirtyPoint && draw_con_frac > 0.0f) {
		int consoleHeight = (int)(draw_con_frac * current_vid_h);
		if (consoleHeight > 0) {
			orig_SRC_AddDirtyPoint(0, 0);
			orig_SRC_AddDirtyPoint(current_vid_w - 1, consoleHeight - 1);
		}
	}

	g_currentStretchPicCaller = StretchPicCaller::Unknown;
	g_activeRenderType = uiRenderType::None;
	resetGlVertexQuadState();
}
// #pragma GCC pop_options

#endif // FEATURE_SCALED_CON

