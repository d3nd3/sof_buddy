#include "feature_config.h"

#if FEATURE_ENTITY_VISUALIZER

#include "features/entity_visualizer/ev_internal.h"

// Runs in SoF.exe every frame (survives gamex86 unload/reload). ClientThink is patched inside gamex86 and can
// stop matching the live entry after reload; attack→intersect must not depend on that hook alone.
void entity_visualizer_qcommon_frame_post_callback(int msec) {
	(void)msec;
	ev::TickLevelCacheServerState();
	ev::PollAttackIntersectForLocalPlayer();
}

#endif // FEATURE_ENTITY_VISUALIZER
