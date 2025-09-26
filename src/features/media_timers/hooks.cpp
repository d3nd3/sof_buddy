#include "sof_buddy.h"
#include "util.h"
#include "features/media_timers/hooks.h"

// For now, delegate to existing my_Sys_Milliseconds implementation until it's split further
int my_Sys_Milliseconds(void);

int media_timers_on_Sys_Milliseconds(void)
{
	return my_Sys_Milliseconds();
}


