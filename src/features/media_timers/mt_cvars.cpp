/*
	Media Timers - CVars
	
	This file contains all cvar declarations and registration for the media_timers feature.
*/

#include "feature_config.h"

#if FEATURE_MEDIA_TIMERS

#include "sof_buddy.h"
#include "sof_compat.h"
#include "shared.h"

// CVar declarations
cvar_t * _sofbuddy_high_priority = NULL;
cvar_t * _sofbuddy_sleep = NULL;
cvar_t * _sofbuddy_sleep_jitter = NULL;
cvar_t * _sofbuddy_sleep_busyticks = NULL;

/*
	Create and register all media_timers cvars
*/
void create_mediatimers_cvars(void) {
	_sofbuddy_high_priority = orig_Cvar_Get("_sofbuddy_high_priority","1",CVAR_SOFBUDDY_ARCHIVE,&high_priority_change);
	
	_sofbuddy_sleep = orig_Cvar_Get("_sofbuddy_sleep","1",CVAR_SOFBUDDY_ARCHIVE,&sleep_change);
	
	_sofbuddy_sleep_jitter = orig_Cvar_Get("_sofbuddy_sleep_jitter","0", CVAR_SOFBUDDY_ARCHIVE, &sleep_jitter_change);
	
	_sofbuddy_sleep_busyticks = orig_Cvar_Get("_sofbuddy_sleep_busyticks","2", CVAR_SOFBUDDY_ARCHIVE, &sleep_busyticks_change);

	// Apply archived/default values immediately so internal state matches CVars at startup.
	if (_sofbuddy_high_priority) high_priority_change(_sofbuddy_high_priority);
	if (_sofbuddy_sleep) sleep_change(_sofbuddy_sleep);
	if (_sofbuddy_sleep_jitter) sleep_jitter_change(_sofbuddy_sleep_jitter);
	if (_sofbuddy_sleep_busyticks) sleep_busyticks_change(_sofbuddy_sleep_busyticks);
}

#endif // FEATURE_MEDIA_TIMERS
