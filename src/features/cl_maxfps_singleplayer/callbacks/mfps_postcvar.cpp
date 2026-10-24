#include "feature_config.h"

#if FEATURE_CL_MAXFPS_SINGLEPLAYER

#include "../shared.h"

void cl_maxfps_PostCvarInit(void)
{
	mfps_restore_cl_frame_maxfps_gate();
	mfps_apply_cl_frame_sp_throttle_patch();
	mfps_install_cl_frame_exact_gate_detour();
}

#endif
