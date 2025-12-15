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
extern cvar_t * _sofbuddy_lighting_overbright;
extern cvar_t * _sofbuddy_lighting_cutoff;

// Scaled UI features (shared infrastructure)
extern void create_scaled_ui_cvars(void);
extern cvar_t * _sofbuddy_font_scale;
extern cvar_t * _sofbuddy_console_size;
extern cvar_t * _sofbuddy_hud_scale;
extern cvar_t * _sofbuddy_crossh_scale;

// Menu scaling variables (part of scaled_menu feature)
extern bool menuSliderDraw;
extern bool menuLoadboxDraw;
extern bool menuVerticalScrollDraw;
extern bool isDrawPicTiled;
extern bool isDrawPicCenter;
extern bool mainMenuBgTiled;
extern int DrawPicWidth;
extern int DrawPicHeight;
