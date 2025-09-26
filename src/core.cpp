#include "sof_buddy.h"
#include "util.h"

// Centralized hook shims that delegate to feature-owned implementations.

// Media Timers
#include "features/media_timers/hooks.h"
int core_Sys_Milliseconds(void)
{
	return media_timers_on_Sys_Milliseconds();
}

// Cinematic Freeze
#include "features/cinematic_freeze/hooks.h"
extern void (*orig_CinematicFreeze)(bool);
void core_CinematicFreeze(bool bEnable)
{
	bool before = *(char*)0x5015D8D5 == 1 ? true : false;
	orig_CinematicFreeze(bEnable);
	bool after = *(char*)0x5015D8D5 == 1 ? true : false;
	cinematic_freeze_on_CinematicFreeze(before, after);
}


