#include "feature_config.h"

#if FEATURE_SCALED_CON

#include "sof_compat.h"
#include "util.h"
#include "generated_detours.h"
#include "../../scaled_ui_base/shared.h"

using detour_SCR_DrawPlayerInfo::oSCR_DrawPlayerInfo;

void hkSCR_DrawPlayerInfo(detour_SCR_DrawPlayerInfo::tSCR_DrawPlayerInfo original) {
	if (!oSCR_DrawPlayerInfo && original) oSCR_DrawPlayerInfo = original;
	isDrawingTeamicons = true;
	oSCR_DrawPlayerInfo();
	isDrawingTeamicons = false;
}

#endif // FEATURE_SCALED_CON

