#pragma once

#include "feature_config.h"

#if FEATURE_CL_MAXFPS_SINGLEPLAYER

extern int (*orig_sp_Sys_Mil)(void);
int hksp_Sys_Mil(void);

void mfps_sync_exe_cinematicfreeze_from_game(void);
void mfps_restore_cl_frame_maxfps_gate(void);
void mfps_apply_cl_frame_sp_throttle_patch(void);
void mfps_install_cl_frame_exact_gate_detour(void);

#if FEATURE_MEDIA_TIMERS
void mfps_set_exact_cl_frame_patch(bool enable);
#endif

#endif
