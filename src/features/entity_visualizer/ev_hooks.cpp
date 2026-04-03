#include "features/entity_visualizer/shared.h"

#if FEATURE_ENTITY_VISUALIZER

#include "ev_internal.h"
#include "generated_detours.h"

// ---------------------------------------------------------------------------
// Detour override callbacks (wired via hooks.json and generated_detours.*)
// ---------------------------------------------------------------------------

// ED_CallSpawn: after real spawn. During ss_loading we track each spawn on a stack so G_FreeEdict can
// snapshot ents that delete themselves (e.g. info_player_deathmatch) before the slot is cleared.
void entity_visualizer_ED_CallSpawn(void* ent, detour_ED_CallSpawn::tED_CallSpawn original) {
    ev::TickLevelCacheServerState();
    ev::ResetLevelEntityCacheOnWorldspawn(ent);

    // Spawn-cache recording is gated on _sofbuddy_entity_edit; worldspawn reset above is not — otherwise loading a
    // map with the cvar off leaves the previous map's cache and sofbuddy_entities_draw merges stale rows with live edicts.
    if (ev::IsEnabled()) {
        bool const track = ev::ShouldTrackMapSpawnsForCache();
        if (track) {
            ev::PushSpawnStack(ent);
        }
        if (original) {
            original(ent);
        }
        if (track) {
            ev::FinalizeSpawnDraw(ent);
            ev::PopSpawnStack(ent);
        }
    } else if (original) {
        original(ent);
    }
}

// G_FreeEdict: snapshot current spawn target before free (see OnFreeEdictBeforeOriginal).
void entity_visualizer_G_FreeEdict(void* ed, detour_G_FreeEdict::tG_FreeEdict original) {
    if (ev::IsEnabled()) {
        ev::OnFreeEdictBeforeOriginal(ed);
    }
    if (original) {
        original(ed);
    }
}

// ClientThink: forward only. Attack→intersect is handled in PollAttackIntersectForLocalPlayer (Qcommon_Frame post)
// so it still runs after gamex86 reload when the ClientThink detour can stop matching the live ClientThink entry.
void entity_visualizer_ClientThink(
    void* ent,
    void* ucmd,
    detour_ClientThink::tClientThink original)
{
    (void)ent;
    (void)ucmd;
    if (original) {
        original(ent, ucmd);
    }
}

#endif // FEATURE_ENTITY_VISUALIZER

