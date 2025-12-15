#include "feature_config.h"

#if FEATURE_SCALED_CON

#include "sof_compat.h"
#include "util.h"
#include "generated_detours.h"
#include "../../scaled_ui_base/shared.h"

using detour_Con_Init::oCon_Init;

void hkCon_Init(detour_Con_Init::tCon_Init original) {
    if (!oCon_Init && original) oCon_Init = original;
    PrintOut(PRINT_LOG, "scaled_con: Registering CVars\n");
    
    create_scaled_ui_cvars();
    if (oCon_Init) oCon_Init();
}

#endif // FEATURE_SCALED_CON

