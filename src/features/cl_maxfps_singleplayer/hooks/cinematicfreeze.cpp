#include "feature_config.h"

#if FEATURE_CL_MAXFPS_SINGLEPLAYER

#include "sof_compat.h"
#include "util.h"
#include "../shared.h"

// gamex86.dll: game.cinematicfreeze (game_locals_t::cinematicfreeze)
static const void* kGameCinematicFreezeRva = (void*)0x0015D8D5;
static const void* kExeCinematicFreezeRva = (void*)0x001E7F5B;

static char* exeCinematicFreezePtr(void)
{
	return (char*)rvaToAbsExe((void*)kExeCinematicFreezeRva);
}

static char* gameCinematicFreezePtr(void)
{
	return (char*)rvaToAbsGame((void*)kGameCinematicFreezeRva);
}

void mfps_set_exe_cinematicfreeze(bool enable)
{
	char* flag = exeCinematicFreezePtr();
	if (flag) {
		*flag = enable ? 1 : 0;
	}
}

void mfps_clear_exe_cinematicfreeze(void)
{
	mfps_set_exe_cinematicfreeze(false);
}

void mfps_sync_exe_cinematicfreeze_from_game(void)
{
	char* exeFlag = exeCinematicFreezePtr();
	char* gameFlag = gameCinematicFreezePtr();
	if (!exeFlag || !gameFlag || !*exeFlag) {
		return;
	}
	if (!*gameFlag) {
		*exeFlag = 0;
	}
}

void cinematicfreeze_callback(bool bEnable)
{
	mfps_set_exe_cinematicfreeze(bEnable);
}

#endif // FEATURE_CL_MAXFPS_SINGLEPLAYER
