#include "feature_config.h"

#if FEATURE_CL_MAXFPS_SINGLEPLAYER

#include "../shared.h"

void mfps_M_PushMenu_pre(char const*& menu_file, char const*& parentFrame, bool& lock_input)
{
	(void)menu_file;
	(void)parentFrame;
	(void)lock_input;
	mfps_sync_exe_cinematicfreeze_from_game();
}

#endif // FEATURE_CL_MAXFPS_SINGLEPLAYER
