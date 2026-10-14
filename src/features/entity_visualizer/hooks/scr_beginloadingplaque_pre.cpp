#include "feature_config.h"

#if FEATURE_ENTITY_VISUALIZER

#include "features/entity_visualizer/ev_internal.h"
#include "sof_compat.h"

// Pre (before engine): warm map loads can run ED_CallSpawn while client sv.state at kSvStateAddr is already
// ss_game, so IsSsLoading() is false and lights (freed after bake) never hit the cache. Post is too late if
// spawns run inside the original plaque. NotifyLoadingPlaque arms plaque-bootstrap capture there.
void entity_visualizer_SCR_BeginLoadingPlaque_pre(qboolean& noPlaque) {
    (void)noPlaque;
    ev::NotifyLoadingPlaque();
}

#endif
