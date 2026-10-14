/*
	Media Timers - CVars
	
	This file contains all cvar declarations and registration for the media_timers feature.
*/

#include "feature_config.h"

#if FEATURE_MEDIA_TIMERS

#include "sof_buddy.h"
#include "sof_compat.h"
#include "shared.h"
#include "generated_detours.h"

// CVar declarations
cvar_t * _sofbuddy_high_priority = NULL;
cvar_t * _sofbuddy_sleep = NULL;
cvar_t * _sofbuddy_sleep_jitter = NULL;
cvar_t * _sofbuddy_sleep_busyticks = NULL;

/*
	Create and register all media_timers cvars
*/
void create_mediatimers_cvars(void) {
	_sofbuddy_high_priority = detour_Cvar_Get::oCvar_Get("_sofbuddy_high_priority","1",CVAR_SOFBUDDY_ARCHIVE,&high_priority_change);
	
	_sofbuddy_sleep = detour_Cvar_Get::oCvar_Get("_sofbuddy_sleep","1",CVAR_SOFBUDDY_ARCHIVE,&sleep_change);
	
	_sofbuddy_sleep_jitter = detour_Cvar_Get::oCvar_Get("_sofbuddy_sleep_jitter","0", CVAR_SOFBUDDY_ARCHIVE, &sleep_jitter_change);
	
	_sofbuddy_sleep_busyticks = detour_Cvar_Get::oCvar_Get("_sofbuddy_sleep_busyticks","2", CVAR_SOFBUDDY_ARCHIVE, &sleep_busyticks_change);

	// Engine framerate cap, default 30. The engine creates it later in CL_InitLocal with the
	// same default, so resolving it here is safe and lets cl_maxfps_change keep the
	// sleep/busyloop frame budget in sync with the cap picked on F12 -> CPU.
	cl_maxfps = detour_Cvar_Get::oCvar_Get("cl_maxfps", "30", 0, &cl_maxfps_change);

	// Apply archived/default values immediately so internal state matches CVars at startup.
	if (_sofbuddy_high_priority) high_priority_change(_sofbuddy_high_priority);
	if (_sofbuddy_sleep) sleep_change(_sofbuddy_sleep);
	if (_sofbuddy_sleep_jitter) sleep_jitter_change(_sofbuddy_sleep_jitter);
	if (_sofbuddy_sleep_busyticks) sleep_busyticks_change(_sofbuddy_sleep_busyticks);
	if (cl_maxfps) cl_maxfps_change(cl_maxfps);
}

#endif // FEATURE_MEDIA_TIMERS
