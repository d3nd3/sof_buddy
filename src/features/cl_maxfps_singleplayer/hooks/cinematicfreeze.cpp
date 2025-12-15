#include "feature_config.h"

#if FEATURE_CL_MAXFPS_SINGLEPLAYER

#include "sof_compat.h"
#include "util.h"

void cinematicfreeze_callback(bool bEnable)
{
	char * cl_ps_cinematicfreeze = (char*)rvaToAbsExe((void*)0x001E7F5B);
	*cl_ps_cinematicfreeze = bEnable;
}

#endif // FEATURE_CL_MAXFPS_SINGLEPLAYER

