#include "feature_config.h"

#if FEATURE_SCALED_CON

#include "sof_compat.h"
#include "util.h"
#include "generated_detours.h"
#include "../../scaled_ui_base/shared.h"

using detour_Con_CheckResize::oCon_CheckResize;

void hkCon_CheckResize(detour_Con_CheckResize::tCon_CheckResize original) {
	if (!oCon_CheckResize && original) oCon_CheckResize = original;
	//This makes con.linewidth smaller in order to reduce the character count per line.
	int viddef_before = current_vid_w;
	*viddef_width = 1 / fontScale * current_vid_w;
	
	oCon_CheckResize();
	
	*viddef_width = viddef_before;
}

#endif // FEATURE_SCALED_CON

