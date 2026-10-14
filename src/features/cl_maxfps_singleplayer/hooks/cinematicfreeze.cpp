#include "feature_config.h"

#if FEATURE_CL_MAXFPS_SINGLEPLAYER

#include "sof_compat.h"
#include "util.h"
#include "../shared.h"

// gamex86.dll: game.cinematicfreeze
static const void* kGameCinematicFreezeRva = (void*)0x0015D8D5;
// SoF.exe: cl.ps.cinematicfreeze — M_PushMenu bails when set
static const void* kExeCinematicFreezeRva = (void*)0x001E7F5B;

static char* exeCinematicFreezePtr(void)
{
	return (char*)rvaToAbsExe((void*)kExeCinematicFreezeRva);
}

static char* gameCinematicFreezePtr(void)
{
	return (char*)rvaToAbsGame((void*)kGameCinematicFreezeRva);
}

// With cl_maxfps SP throttle, intermission → M_PushMenu often runs at the next
// Qcommon_Frame top Cbuf_Execute (before CL_ReadPackets), not inside
// CL_SendCommand. Exe freeze can still be set while game already cleared it.
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
	char* flag = exeCinematicFreezePtr();
	if (flag) {
		*flag = bEnable ? 1 : 0;
	}
}

#endif // FEATURE_CL_MAXFPS_SINGLEPLAYER
