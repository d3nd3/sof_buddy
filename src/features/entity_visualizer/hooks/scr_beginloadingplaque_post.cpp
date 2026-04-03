#include "feature_config.h"

#if FEATURE_ENTITY_VISUALIZER

#include "features/entity_visualizer/ev_internal.h"

// Before internal_menus Post (lower priority): warm map loads can miss sv.state edges in
// TickLevelCacheServerState (lights are cache-only after bake).
void entity_visualizer_SCR_BeginLoadingPlaque_post(qboolean noPlaque) {
    (void)noPlaque;
    ev::NotifyLoadingPlaque();
}

#endif // FEATURE_ENTITY_VISUALIZER
