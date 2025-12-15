#include "feature_config.h"

#if FEATURE_SCALED_CON

#include "sof_compat.h"
#include "util.h"
#include "generated_detours.h"
#include "../../scaled_ui_base/shared.h"

#define GL_BLEND 0x0BE2

void hkCon_DrawNotify(detour_Con_DrawNotify::tCon_DrawNotify original) {
	real_refdef_width = current_vid_w;

	//fake the width
	*viddef_width = 1 / fontScale * current_vid_w;
	
	g_activeRenderType = uiRenderType::Console;
	orig_glDisable(GL_BLEND);
	original();
	
	//restore the width
	*viddef_width = real_refdef_width;
	g_activeRenderType = uiRenderType::None;
}

#endif // FEATURE_SCALED_CON

