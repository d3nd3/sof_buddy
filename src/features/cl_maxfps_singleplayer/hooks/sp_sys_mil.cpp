#include "feature_config.h"

#if FEATURE_CL_MAXFPS_SINGLEPLAYER

#include "sof_compat.h"
#include "util.h"
#include "sof_buddy.h"
#include "../shared.h"
#if FEATURE_MEDIA_TIMERS
#include "features/media_timers/shared.h"
#endif

int (*orig_sp_Sys_Mil)(void) = NULL;

int hksp_Sys_Mil(void)
{
	//we set this to 0 _before_ we call spcl_Sys_Mil()
	if (o_sofplus) {
		int* _sp_cl_frame_delay = (int*)rvaToAbsSoFPlus((void*)0x331BC);
		SOFBUDDY_ASSERT(_sp_cl_frame_delay != nullptr);
		*_sp_cl_frame_delay = 0x00;
	}

    const int ret = orig_sp_Sys_Mil ? orig_sp_Sys_Mil() : 0;
#if FEATURE_MEDIA_TIMERS
    sofbuddy_sp_clear_sofplus_frame_delay_if_exact();
#endif
    return ret;
}

#endif // FEATURE_CL_MAXFPS_SINGLEPLAYER

