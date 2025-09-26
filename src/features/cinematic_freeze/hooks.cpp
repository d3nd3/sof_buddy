#include "sof_buddy.h"
#include "util.h"
#include "features/cinematic_freeze/hooks.h"

void cinematic_freeze_on_CinematicFreeze(bool beforeState, bool afterState)
{
	// Keep this small and focused on the feature behavior only
	if (beforeState != afterState) {
		*(char*)0x201E7F5B = afterState ? 0x1 : 0x00;
	}
}


