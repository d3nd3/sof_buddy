#include "feature_config.h"

#if FEATURE_CL_MAXFPS_SINGLEPLAYER

#include "../shared.h"

void cl_maxfps_GameDllLoaded(void* game_export)
{
	(void)game_export;
	mfps_clear_exe_cinematicfreeze();
}

void cl_maxfps_ShutdownGameProgs(void)
{
	mfps_clear_exe_cinematicfreeze();
}

#endif // FEATURE_CL_MAXFPS_SINGLEPLAYER
