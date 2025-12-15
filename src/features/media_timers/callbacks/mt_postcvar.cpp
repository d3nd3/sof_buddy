#include "feature_config.h"

#if FEATURE_MEDIA_TIMERS

#include "sof_compat.h"
#include "util.h"
#include "../shared.h"

void mediaTimers_PostCvarInit(void)
{
	PrintOut(PRINT_LOG,"mediaTimers_PostCvarInit");
	cl_maxfps = orig_Cvar_Get("cl_maxfps","30",0,&cl_maxfps_change);
	
	create_mediatimers_cvars();
	
	PrintOut(PRINT_LOG, "Media timers: Applying memory patches...\n");
}

#endif // FEATURE_MEDIA_TIMERS

