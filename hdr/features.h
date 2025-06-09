extern void mediaTimers_apply_afterCmdline(void);
extern void mediaTimers_early(void);

extern void refFixes_early(void);
extern void refFixes_cvars_init(void);

extern void scaledFont_early(void);
extern void scaledFont_cvars_init(void);
extern void my_Con_CheckResize(void);

extern void resetTimers(int val);
extern int oldtime;

/*
	Will enable all features by default. The reasoning to disable would be to be super optimiser.
*/
#define FEATURE_MEDIA_TIMERS
#define FEATURE_HD_TEX
#define FEATURE_TEAMICON_FIX
#define FEATURE_ALT_LIGHTING
#define FEATURE_FONT_SCALING

extern void create_sofbuddy_cvars(void);
extern void create_mediatimers_cvars(void);
extern void create_reffixes_cvars(void);
extern void create_scaled_fonts_cvars(void);


//sof_buddy.cpp
extern cvar_t * _sofbuddy_classic_timers;


//meda_timers.cpp
extern cvar_t * _sofbuddy_high_priority;
extern cvar_t * _sofbuddy_sleep;
extern cvar_t * _sofbuddy_sleep_gamma;
extern cvar_t * _sofbuddy_sleep_busyticks;


//ref_fixes.cpp
extern cvar_t * _sofbuddy_lightblend_src;
extern cvar_t * _sofbuddy_lightblend_dst;

extern cvar_t * _sofbuddy_minfilter_unmipped;
extern cvar_t * _sofbuddy_magfilter_unmipped;
extern cvar_t * _sofbuddy_minfilter_mipped;
extern cvar_t * _sofbuddy_magfilter_mipped;
extern cvar_t * _sofbuddy_minfilter_ui;
extern cvar_t * _sofbuddy_magfilter_ui;

extern cvar_t * _sofbuddy_whiteraven_lighting;

extern int current_vid_w;
extern int current_vid_h;

//scaled_font.cpp
extern cvar_t * _sofbuddy_font_scale;
extern cvar_t * _sofbuddy_console_size;
extern cvar_t * test;
extern cvar_t * test2;
