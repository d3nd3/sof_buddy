#pragma once

#include "feature_config.h"

#if FEATURE_FORCERUN_SINGLEPLAYER

#include "sof_compat.h"

extern cvar_t * _sofbuddy_forcerun_singleplayer;

void forcerun_create_cvars(void);

#endif // FEATURE_FORCERUN_SINGLEPLAYER
