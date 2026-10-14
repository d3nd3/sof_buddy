#include "feature_config.h"

#if FEATURE_ENTITY_VISUALIZER

#include "features/entity_visualizer/ev_internal.h"

void entity_visualizer_GameDllLoaded(void* game_export) {
    // Fresh game_export_t each gamex86 load; also reset attack-edge state so ClientThink intersect works
    // after reload (player edict address may be reused while prevButtons map still had stale bits).
    ev::InstallSpawnEntitiesWrapper(game_export);
    ev::ClearClientThinkAttackLatchState();
}

#endif // FEATURE_ENTITY_VISUALIZER
