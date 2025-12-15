#include "feature_config.h"

#if FEATURE_CL_MAXFPS_SINGLEPLAYER

#include "sof_compat.h"
#include "util.h"
#include "sof_buddy.h"
#include "../shared.h"

int (*orig_sp_Sys_Mil)(void) = NULL;

int hksp_Sys_Mil(void)
{
	//we set this to 0 _before_ we call spcl_Sys_Mil()
	if (o_sofplus) {
		int* _sp_cl_frame_delay = (int*)rvaToAbsSoFPlus((void*)0x331BC);
		*_sp_cl_frame_delay = 0x00;
	}

    if (orig_sp_Sys_Mil) return orig_sp_Sys_Mil();
    return 0;
}

#endif // FEATURE_CL_MAXFPS_SINGLEPLAYER

