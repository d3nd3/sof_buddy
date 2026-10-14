#include "feature_config.h"

#if FEATURE_SCALED_CON

#include "sof_compat.h"
#include "util.h"
#include "generated_detours.h"
#include "../../scaled_ui_base/shared.h"

void hkSCR_DrawPlayerInfo(detour_SCR_DrawPlayerInfo::tSCR_DrawPlayerInfo original) {
	isDrawingTeamicons = true;
	original();
	isDrawingTeamicons = false;
}

#endif // FEATURE_SCALED_CON

