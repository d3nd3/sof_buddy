#include "feature_config.h"

#if FEATURE_LIGHTING_BLEND

#include "../shared.h"

/*
	SV_ShutdownGameProgs runs while a map change or disconnect tears down the
	server/world.  A lightmap rebuild queued just before the map change must not
	run against the old or partially freed world model during that transition.
	The new map is built with the current LUT and can request a rebuild again
	after R_BeginRegistration if normalization needs it.
*/
void sv_shutdowngameprogs_lighting_pre_callback(void) {
	lighting_cancel_pending_rebuild();
}

#endif // FEATURE_LIGHTING_BLEND
