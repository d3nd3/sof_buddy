#include "feature_config.h"

#if FEATURE_ENTITY_VISUALIZER

#include "features/entity_visualizer/ev_internal.h"

#include <cstddef>

namespace {

using SpawnFn = void(__cdecl*)(char*, char*, char*);

SpawnFn s_orig_spawn_entities = nullptr;

void __cdecl entity_visualizer_SpawnEntities_Hook(char* mapname, char* entities, char* spawnpoint) {
    ev::detail::OnSpawnEntitiesPrologue(entities);
    if (s_orig_spawn_entities) {
        s_orig_spawn_entities(mapname, entities, spawnpoint);
    }
    ev::detail::OnSpawnEntitiesEpilogue();
}

// game_export_t (SDK game.h): apiversion @0, Init @4, Shutdown @8, SpawnEntities @0xC
struct game_export_spawn_patch {
    int          apiversion;
    void(__cdecl *Init)(void);
    void(__cdecl *Shutdown)(void);
    SpawnFn      SpawnEntities;
};

} // namespace

namespace ev {

void InstallSpawnEntitiesWrapper(void* game_export_ptr) {
    if (!game_export_ptr) {
        return;
    }
    auto* ge = reinterpret_cast<game_export_spawn_patch*>(game_export_ptr);
    if (!ge->SpawnEntities) {
        return;
    }
    // Each gamex86 load returns a fresh game_export_t; always point at the real DLL entry.
    s_orig_spawn_entities = ge->SpawnEntities;
    ge->SpawnEntities     = &entity_visualizer_SpawnEntities_Hook;
}

} // namespace ev

#endif // FEATURE_ENTITY_VISUALIZER
