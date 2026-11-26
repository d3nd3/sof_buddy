/*
	Local MP Full Movescale - CVars

	Registers the F12 Gameplay toggle. The override is applied when gamex86 is
	loaded for a locally hosted multiplayer game, so the cvar takes effect on the
	next map load without a restart.
*/

#include "feature_config.h"

#if FEATURE_LOCAL_MP_FULL_MOVESCALE

#include "shared.h"
#include "generated_detours.h"

#ifndef NULL
#define NULL 0
#endif

cvar_t* _sofbuddy_local_mp_full_movescale = NULL;

void lmfm_create_cvars(void) {
	_sofbuddy_local_mp_full_movescale = detour_Cvar_Get::oCvar_Get(
		"_sofbuddy_local_mp_full_movescale", "1", CVAR_SOFBUDDY_ARCHIVE, NULL);
}

#endif // FEATURE_LOCAL_MP_FULL_MOVESCALE
