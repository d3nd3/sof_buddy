#include "sof_compat.h"

// ============================================================================
// SoF Buddy Feature Declarations
// ============================================================================
// This file contains extern declarations for functions and variables
// used across different features. Feature enable/disable is handled
// by FEATURES.txt and the auto-generated feature_config.h
// ============================================================================

// Core system variables
extern int current_vid_w;
extern int current_vid_h;
extern float screen_y_scale;
extern int * viddef_width;
extern int * viddef_height;

// Media Timers feature
extern void create_mediatimers_cvars(void);
extern void mediaTimers_apply_afterCmdline(void);
extern void mediaTimers_early(void);
extern void resetTimers(int val);
extern int oldtime;
extern cvar_t * _sofbuddy_high_priority;
extern cvar_t * _sofbuddy_sleep;
extern cvar_t * _sofbuddy_sleep_jitter;
extern cvar_t * _sofbuddy_sleep_busyticks;

// Lighting Blend feature
extern void create_reffixes_cvars(void);
extern void refFixes_early(void);
extern void refFixes_cvars_init(void);
extern cvar_t * _sofbuddy_lightblend_src;
extern cvar_t * _sofbuddy_lightblend_dst;
extern cvar_t * _sofbuddy_whiteraven_lighting;

// Scaled Font feature
extern void create_scaled_fonts_cvars(void);
extern void scaledFont_early(void);
extern void scaledFont_init(void);
extern void scaledFont_cvars_init(void);
extern void my_Con_CheckResize(void);
extern cvar_t * _sofbuddy_font_scale;
extern cvar_t * _sofbuddy_console_size;
extern cvar_t * con_maxlines;
extern cvar_t * con_buffersize;
extern cvar_t * test;
extern cvar_t * test2;

// Menu scaling variables (shared between scaled_font and scaled_menu)
extern bool menuSliderDraw;
extern bool menuLoadboxDraw;
extern bool menuVerticalScrollDraw;
extern bool isDrawPicTiled;
extern bool isDrawPicCenter;
extern int DrawPicWidth;
extern int DrawPicHeight;

// Scaled Menu feature
extern void scaledMenu_early(void); 
extern void scaledMenu_init(void);
