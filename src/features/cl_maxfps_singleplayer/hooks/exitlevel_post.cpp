#include "feature_config.h"

#if FEATURE_CL_MAXFPS_SINGLEPLAYER

#include "../shared.h"

// ExitLevel clears game.cinematicfreeze without calling CinematicFreeze(false).
void mfps_ExitLevel_post(void)
{
	mfps_clear_exe_cinematicfreeze();
}

#endif // FEATURE_CL_MAXFPS_SINGLEPLAYER
