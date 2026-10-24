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
int my_TimeGetTime(void);
void create_mediatimers_cvars(void);
void mediaTimers_EarlyStartup(void);
void mediaTimers_PostCvarInit(void);

void cl_maxfps_change(cvar_t *cvar);
void cl_maxfps_exact_change(cvar_t *cvar);
void sofbuddy_quantize_cl_maxfps(void);
bool sofbuddy_cl_maxfps_exact_enabled(void);
float sofbuddy_cl_maxfps_effective_fps(float stored);
void sofbuddy_sp_clear_sofplus_frame_delay_if_exact(void);
int sofbuddy_exact_fps_quantize(float v);
void sofbuddy_exact_bresenham_reset(void);
int sofbuddy_exact_bresenham_period_ms(int fps);
bool sofbuddy_exact_frame_allows(int extratime_ms, float maxfps_stored);
void sofbuddy_exact_on_frame_window_start(float maxfps_stored);
int sofbuddy_exact_frame_budget_ms(void);
void high_priority_change(cvar_t *cvar);
void sleep_change(cvar_t *cvar);
void sleep_jitter_change(cvar_t *cvar);
void sleep_busyticks_change(cvar_t *cvar);

// Sleep(1) stutter prevention: request 1ms timer period while sleep mode is enabled.
void mediaTimers_Request1msTimerPeriod(bool enable);

#endif // FEATURE_MEDIA_TIMERS

