#pragma once

#include "feature_config.h"

#if FEATURE_MEDIA_TIMERS

#include "sof_compat.h"
#include <windows.h>

extern bool sleep_mode;
extern bool sleep_jitter;
extern int sleep_busyticks;
extern int extratime_resume;

extern cvar_t *cl_maxfps;

extern int (*sp_Sys_Mil)(void);

extern LARGE_INTEGER base;
extern LARGE_INTEGER freq;

extern int *sp_whileLoopCount;
extern int *sp_lastFullClientFrame;
extern int *sp_current_timestamp;
extern int *sp_current_timestamp2;

extern void (*spcl_Timers)(void);
extern void (*spcl_FreeScript)(void);

inline void resetTimers(int val)
{
	if (sp_current_timestamp) *sp_current_timestamp = val;
	if (sp_current_timestamp2) *sp_current_timestamp2 = val;
	if (sp_lastFullClientFrame) *sp_lastFullClientFrame = val;
}

int winmain_loop(void);
int my_Sys_Milliseconds(void);
long long qpc_timers(bool force);
void create_mediatimers_cvars(void);
void mediaTimers_EarlyStartup(void);
void mediaTimers_PostCvarInit(void);

void cl_maxfps_change(cvar_t *cvar);
void high_priority_change(cvar_t *cvar);
void sleep_change(cvar_t *cvar);
void sleep_jitter_change(cvar_t *cvar);
void sleep_busyticks_change(cvar_t *cvar);

#endif // FEATURE_MEDIA_TIMERS


