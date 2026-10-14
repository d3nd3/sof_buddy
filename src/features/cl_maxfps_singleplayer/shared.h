#pragma once

#include "feature_config.h"

#if FEATURE_CL_MAXFPS_SINGLEPLAYER

extern int (*orig_sp_Sys_Mil)(void);
int hksp_Sys_Mil(void);

void mfps_sync_exe_cinematicfreeze_from_game(void);

#endif
