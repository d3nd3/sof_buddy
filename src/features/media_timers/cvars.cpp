/*
	Media Timers - CVars
	
	This file contains all cvar declarations and registration for the media_timers feature.
*/

#include "feature_config.h"

#if FEATURE_MEDIA_TIMERS

#include "sof_buddy.h"
#include "sof_compat.h"

// CVar declarations
cvar_t * _sofbuddy_high_priority = NULL;
cvar_t * _sofbuddy_sleep = NULL;
cvar_t * _sofbuddy_sleep_jitter = NULL;
cvar_t * _sofbuddy_sleep_busyticks = NULL;

// External CVar declarations (defined in hooks.cpp)
extern cvar_t * cl_maxfps;

// Forward declarations of change callbacks (defined in hooks.cpp)
extern void cl_maxfps_change(cvar_t *cvar);
extern void high_priority_change(cvar_t * cvar);
extern void sleep_busyticks_change(cvar_t * cvar);
extern void sleep_jitter_change(cvar_t *cvar);
extern void sleep_change(cvar_t *cvar);

/*
	Create and register all media_timers cvars
*/
void create_mediatimers_cvars(void) {
	cl_maxfps = orig_Cvar_Get("cl_maxfps","30",0,&cl_maxfps_change);
	
	_sofbuddy_high_priority = orig_Cvar_Get("_sofbuddy_high_priority","1",CVAR_ARCHIVE,NULL);
	
	_sofbuddy_sleep = orig_Cvar_Get("_sofbuddy_sleep","1",CVAR_ARCHIVE,NULL);
	
	_sofbuddy_sleep_jitter = orig_Cvar_Get("_sofbuddy_sleep_jitter","0", CVAR_ARCHIVE, NULL);
	
	// How many 1ms ticks of busytick wait should use as protection from inaccurate Sleep(1) 
	_sofbuddy_sleep_busyticks = orig_Cvar_Get("_sofbuddy_sleep_busyticks","2", CVAR_ARCHIVE, NULL);
}

#endif // FEATURE_MEDIA_TIMERS
