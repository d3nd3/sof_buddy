#include "feature_config.h"

#if FEATURE_SCALED_CON || FEATURE_SCALED_HUD

#include "sof_compat.h"
#include "util.h"
#include "generated_detours.h"
#include "../../scaled_ui_base/shared.h"

void hkCon_Init(detour_Con_Init::tCon_Init original) {
    PrintOut(PRINT_LOG, "scaled_ui_base: Registering CVars\n");
    create_scaled_ui_cvars();
    original();
}

#endif


