#pragma once

#include "feature_config.h"

#if FEATURE_VSYNC_TOGGLE

#include "sof_compat.h"

extern cvar_t *vid_ref;
extern cvar_t *gl_swapinterval;

void create_vsync_cvars(void);
void vsync_pre_vid_checkchanges();

#endif // FEATURE_VSYNC_TOGGLE


