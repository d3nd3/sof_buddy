/*
	Force Run Singleplayer - CVars

	Registers the F12 Gameplay toggle. The behaviour is applied per command by
	hooks/cl_basemove.cpp, so the cvar takes effect immediately without a restart.
*/

#include "feature_config.h"

#if FEATURE_FORCERUN_SINGLEPLAYER

#include "shared.h"
#include "generated_detours.h"

#ifndef NULL
#define NULL 0
#endif

cvar_t * _sofbuddy_forcerun_singleplayer = NULL;

void forcerun_create_cvars(void) {
	_sofbuddy_forcerun_singleplayer = detour_Cvar_Get::oCvar_Get(
		"_sofbuddy_forcerun_singleplayer", "0", CVAR_SOFBUDDY_ARCHIVE, NULL);
}

#endif // FEATURE_FORCERUN_SINGLEPLAYER
