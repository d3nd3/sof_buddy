#pragma once

#include "feature_config.h"

#if FEATURE_LOCAL_MP_FULL_MOVESCALE

#include "sof_compat.h"

extern cvar_t* _sofbuddy_local_mp_full_movescale;

void lmfm_create_cvars(void);
void lmfm_restore_movescale(void);

#endif // FEATURE_LOCAL_MP_FULL_MOVESCALE
