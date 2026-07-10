#pragma once

#include "feature_config.h"

#if FEATURE_CL_MAXFPS_SINGLEPLAYER

extern int (*orig_sp_Sys_Mil)(void);
int hksp_Sys_Mil(void);

// Exe cl.ps.cinematicfreeze (spectatorId_cinematicFreeze in SoF.exe)
void mfps_set_exe_cinematicfreeze(bool enable);
void mfps_clear_exe_cinematicfreeze(void);
void mfps_sync_exe_cinematicfreeze_from_game(void);

#endif
