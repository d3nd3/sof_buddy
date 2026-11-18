#pragma once

#include "feature_config.h"

#if FEATURE_LIGHTSTYLE_INTERPOLATION

#include "sof_compat.h"

extern cvar_t * _sofbuddy_lightstyle_interpolation;

void create_lightstyle_interpolation_cvars(void);
void lightstyle_interpolation_change(cvar_t * cvar);
void lightstyle_interpolation_run_post_callback(void);

#endif // FEATURE_LIGHTSTYLE_INTERPOLATION
