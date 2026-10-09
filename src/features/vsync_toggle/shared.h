#pragma once

#include "feature_config.h"

#if FEATURE_VSYNC_TOGGLE

#include "sof_compat.h"

extern cvar_t *vid_ref;
extern cvar_t *gl_swapinterval;
extern cvar_t *vid_fullscreen;
extern cvar_t *gl_displayrefresh;
extern cvar_t *vs_cl_maxfps;
extern cvar_t *_sb_internal_display_info;
extern cvar_t *_sb_internal_fps_cap_mismatch;
extern cvar_t *_sb_internal_display_modes_rmf;

void create_vsync_cvars(void);
void vsync_pre_vid_checkchanges(void);

// Re-reads the display and recomputes the cap-check status (see vs_display.cpp).
void sofbuddy_refresh_display_info(void);
void Cmd_SoFBuddy_Display_Refresh_f(void);

#endif // FEATURE_VSYNC_TOGGLE


