#pragma once

#include <cstdint>
#include <cstring>

#include "feature_config.h"

#if FEATURE_ENTITY_VISUALIZER

#include "sof_compat.h"

namespace ev {

extern cvar_t* _sofbuddy_entity_edit;
// Non-zero: PRINT_DEV per-entity wireframe lines (sofbuddy_entities_draw — cache + live): OK vs SKIP + coords.
extern cvar_t* _sofbuddy_entities_draw_verbose;
void create_entity_visualizer_cvars(void);
void register_entity_visualizer_commands(void);
void register_entity_visualizer_map_study_commands(void);
// xcommand_t for Cmd_AddCommand (registered in register_entity_visualizer_commands from PostCvarInit).
void Cmd_EV_DebugBox_f(void);
void Cmd_EV_DrawCached_f(void);
void Cmd_EV_IntersectOnce_f(void);

// Sync sv.state / spawn-cache linger: Qcommon_Frame post each frame + ED_CallSpawn entry (not ClientThink-only).
void TickLevelCacheServerState(void);
// Must run for every ED_CallSpawn (even when _sofbuddy_entity_edit is 0): resets spawn stack / linger on
// worldspawn. Level-entity geometry for wireframes is rebuilt from the BSP entity string in SpawnEntities
// (see InstallSpawnEntitiesWrapper), not cleared here — ED_CallSpawn used to clear cache here, which wiped
// BSP-filled rows before sofbuddy_entities_draw could run.
void ResetLevelEntityCacheOnWorldspawn(void* ent);
// After Sys_GetGameApi: replace game_export_t::SpawnEntities to snapshot CM_EntityString() into the level cache.
void InstallSpawnEntitiesWrapper(void* game_export_ptr);

namespace detail {
void OnSpawnEntitiesPrologue(char* entities);
void OnSpawnEntitiesEpilogue();
} // namespace detail
// Before queueing `map` from map study: clear cache/stack and reset load-detection so the next map always repopulates.
void PrepareLevelEntityCacheForMapStudyLoad(void);
// SCR_BeginLoadingPlaque (entity_visualizer Pre + Post hooks): resets sv.state edge
// tracking and arms plaque-bootstrap capture until worldspawn or a time cap. Warm loads can run ED_CallSpawn in
// gamex86 while client sv.state (kSvStateAddr) is already ss_game, so IsSsLoading() is false for lights; bootstrap
// records spawns without relying on that flag. Does not clear the spawn cache.
void NotifyLoadingPlaque(void);
// True while map ents should be recorded: ss_loading, or brief linger after load if spawns finish in ss_game.
bool ShouldTrackMapSpawnsForCache(void);

// Global edict base pointer and count.
static constexpr uintptr_t kEdictsBasePtrAddr = 0x5015CCA0;
static constexpr uintptr_t kNumEdictsPtrAddr = 0x5015C938;

// SoF.exe sv.state — same order as Quake 2 server_state_t: ss_dead=0, ss_loading=1, ss_game=2, ...
// Level-entity cache fills during ss_loading and a short linger into ss_game (late spawns).
static constexpr uintptr_t kSvStateAddr = 0x203A1F20;
static constexpr int kSvStateLoading = 1;
static constexpr int kSvStateGame = 2;

// edict layout offsets.
static constexpr uintptr_t kSizeOfEdict = 0x464;
static constexpr uintptr_t kEdictInUseOffset = 0x78;
static constexpr uintptr_t kEdictSOriginOffset = 0x4;
static constexpr uintptr_t kEdictMinsOffset = 0x11C;
static constexpr uintptr_t kEdictMaxsOffset = 0x128;
static constexpr uintptr_t kEdictClassnameOffset = 0x1B4;
static constexpr uintptr_t kEdictFlagsOffset = 0x1A0;
static constexpr uintptr_t kEdictGClientOffset = 0x74;
static constexpr uintptr_t kEdictOwnerOffset = 0x160;

static constexpr uintptr_t kGClientPmFlagsOffset = 0x10;
// gclient_s::buttons (int) — set from ucmd->buttons at end of ClientThink (p_client.cpp). Placed 0x80 bytes
// before v_angle using SDK field layout; matches kGClientVAngleOffset 0x4FC − 0x80.
static constexpr uintptr_t kGClientButtonsOffset = 0x47C;
// gclient_s::v_angle (vec3_t) — first float at +0x4FC. Verified gamex86: mov 0x74(%esi),%ecx; flds 0x4fc(%ecx) (e.g. ~0x500f9af1). Not 0x24C (other code paths use flds 0x24c with a different base).
static constexpr uintptr_t kGClientVAngleOffset = 0x4FC;
static constexpr unsigned char kButtonAttack = 1;
static constexpr unsigned int kPmfDucked = 1;
static constexpr float kMinIntersectT = 0.125f;

// Same pseudo hulls as wireframe draw / level cache (ev_draw): QUAKED info_player_* sizes, ±8 fallback.
inline bool EvIsZeroBounds(float const* origin, float const* mins, float const* maxs) {
    return !origin[0] && !origin[1] && !origin[2] && !mins[0] && !mins[1] && !mins[2] && !maxs[0] && !maxs[1] &&
           !maxs[2];
}

// Live edict origins can be garbage (dropped items, bad physics). Skip wireframes for non-finite coords.
inline bool EvIsNonFiniteOrigin(float const* o) {
    for (int i = 0; i < 3; ++i) {
        float const v = o[i];
        if (v != v || v > 1e20f || v < -1e20f) {
            return true;
        }
    }
    return false;
}

// Pickups/weapons should not sit tens of thousands of units off the playable volume; cache spawn snapshot is
// authoritative when live state glitches (e.g. item_ammo_slug Z -18714 vs spawn -99).
inline bool EvSuspiciousLivePickupOrigin(char const* cn, float const* o) {
    if (!cn) {
        return false;
    }
    if (std::strncmp(cn, "item_", 5) != 0 && std::strncmp(cn, "weapon_", 7) != 0) {
        return false;
    }
    float const az = o[2] >= 0.f ? o[2] : -o[2];
    return az > 18000.f;
}

inline void EvApplyInfoPlayerDefaultHullIfUnset(float* mins, float* maxs, char const* classname) {
    if (!classname || std::strncmp(classname, "info_player_", 12) != 0) {
        return;
    }
    auto all_zero = [](float const* v) { return !v[0] && !v[1] && !v[2]; };
    if (!all_zero(mins) || !all_zero(maxs)) {
        return;
    }
    mins[0] = -16.f;
    mins[1] = -16.f;
    mins[2] = -24.f;
    maxs[0] = 16.f;
    maxs[1] = 16.f;
    maxs[2] = 32.f;
}

inline void EvApplyDefaultStudyBboxIfUnset(float* mins, float* maxs) {
    auto all_zero = [](float const* v) { return !v[0] && !v[1] && !v[2]; };
    if (!all_zero(mins) || !all_zero(maxs)) {
        return;
    }
    mins[0] = -8.f;
    mins[1] = -8.f;
    mins[2] = -8.f;
    maxs[0] = 8.f;
    maxs[1] = 8.f;
    maxs[2] = 8.f;
}

// Some ents have inverted or paper-thin size on an axis; FX line edges vanish and slab tests misbehave.
inline void EvExpandDegenerateBboxAxes(float* mins, float* maxs) {
    float const half = 4.f; // 8 unit span per axis (matches default study cube)
    for (int i = 0; i < 3; ++i) {
        float const span = maxs[i] - mins[i];
        if (span > 1.f) {
            continue;
        }
        float const mid = 0.5f * (mins[i] + maxs[i]);
        mins[i] = mid - half;
        maxs[i] = mid + half;
    }
}

// Single path for wireframe / cache / intersect: info_player hulls, default ±8, fix degenerate axes, last-resort cube.
inline void EvFinalizeStudyBboxForWireframe(float const* origin, float* mins, float* maxs, char const* classname) {
    EvApplyInfoPlayerDefaultHullIfUnset(mins, maxs, classname);
    EvApplyDefaultStudyBboxIfUnset(mins, maxs);
    if (EvIsZeroBounds(origin, mins, maxs)) {
        EvApplyDefaultStudyBboxIfUnset(mins, maxs);
    }
    EvExpandDegenerateBboxAxes(mins, maxs);
    for (int i = 0; i < 3; ++i) {
        if (!(maxs[i] > mins[i])) {
            float const mid = 0.5f * (mins[i] + maxs[i]);
            mins[i] = mid - 4.f;
            maxs[i] = mid + 4.f;
        }
    }
    if (EvIsZeroBounds(origin, mins, maxs)) {
        mins[0] = mins[1] = mins[2] = -8.f;
        maxs[0] = maxs[1] = maxs[2] = 8.f;
    }
}

// Slab AABB test; if the ray origin is inside the box, out_t is 0 (so small ents e.g. lights still register).
inline bool EvRayVsAabb(float ox, float oy, float oz, float dx, float dy, float dz, float const bmin[3],
                        float const bmax[3], float& out_t) {
    float const invx = (dx != 0.f) ? (1.f / dx) : 1e30f;
    float const invy = (dy != 0.f) ? (1.f / dy) : 1e30f;
    float const invz = (dz != 0.f) ? (1.f / dz) : 1e30f;
    float t1 = (bmin[0] - ox) * invx;
    float t2 = (bmax[0] - ox) * invx;
    float tmin_x = t1 < t2 ? t1 : t2;
    float tmax_x = t1 < t2 ? t2 : t1;
    t1 = (bmin[1] - oy) * invy;
    t2 = (bmax[1] - oy) * invy;
    float tmin_y = t1 < t2 ? t1 : t2;
    float tmax_y = t1 < t2 ? t2 : t1;
    t1 = (bmin[2] - oz) * invz;
    t2 = (bmax[2] - oz) * invz;
    float tmin_z = t1 < t2 ? t1 : t2;
    float tmax_z = t1 < t2 ? t2 : t1;
    float t_enter = tmin_x;
    if (tmin_y > t_enter) {
        t_enter = tmin_y;
    }
    if (tmin_z > t_enter) {
        t_enter = tmin_z;
    }
    float t_exit = tmax_x;
    if (tmax_y < t_exit) {
        t_exit = tmax_y;
    }
    if (tmax_z < t_exit) {
        t_exit = tmax_z;
    }
    if (t_enter > t_exit) {
        return false;
    }
    if (t_exit < 0.f) {
        return false;
    }
    if (t_enter >= 0.f) {
        out_t = t_enter;
        return true;
    }
    // t_enter < 0 && t_exit >= 0: origin inside or straddling the near face
    out_t = 0.f;
    return true;
}

// Merges closest hit from s_level_entity_cache (freed-at-spawn ents, e.g. SP_light in DM). Same TU as ev_draw.
void IntersectMergeCachedLevelRayHits(void* player_ent, float ox, float oy, float oz, float dx, float dy, float dz,
                                    float* inout_best_t, unsigned char** io_hit_live_ent, char* out_cache_classname,
                                    size_t out_cache_classname_bytes);

// cquad_t — IDA: sizeof 0xDC (220); +0x00 entity_t*; +0xBC ghoulinst* (see FX_AddQuads branch using [quad+0BCh]).
// FX_AllocNewQuad stores &fxCurTime at +0x68 and again at +0x8C. Line-quad setup / FX_GenericQuadLineUpdate can
// overwrite +0x68 with float bits (fadd (%reg) fault if mistaken for a pointer). Prefer reading live time via +0x8C
// and only dereference if the dword lies in the SoF.exe mapping (see kSofExeLoad*).
// +0x6C: float vs live fxCurTime in FX_AddQuads — expire threshold.
// +0x70: think fn; FX_SetQuadLineGenericInfo sets this to FX_GenericQuadLineUpdate, which can mov [quad+6Ch],0
// before FX_AddQuads — static lines clear +70 after setting expire (FX_AllocNewQuad left +70 NULL).
// +0x34: char* (shader / image name). FX_AddQuads @ ~0x2003B9FA skips V_AddQuad if null; ref_gl then scans
// this string (repne scasb) — storing a small integer (e.g. 1) faults with EDI=0x1.
static constexpr uintptr_t kCQuadFxCurTimePtrOffset = 0x68;
static constexpr uintptr_t kCQuadFxCurTimePtrDupOffset = 0x8C;
static constexpr uintptr_t kCQuadExpireTimeOffset = 0x6C;
static constexpr uintptr_t kCQuadThinkFnOffset = 0x70;
static constexpr uintptr_t kCQuadShaderNamePtrOffset = 0x34;
// Typical Wine/module listing for SoF.exe: 0x20000000–0x2040b000 (end exclusive).
static constexpr uint32_t kSofExeLoadBase = 0x20000000u;
static constexpr uint32_t kSofExeLoadEndExclusive = 0x2040B000u;

// ref_gl `GL_FindImage(char* name, int imagetype, char mimap, char allowPicmip)` — SoF imagetype indices.
static constexpr int kGlImageSkin = 0;            // M
static constexpr int kGlImageSprite = 1;          // S
static constexpr int kGlImageWall = 2;            // W
static constexpr int kGlImagePic = 3;             // P
static constexpr int kGlImageSky = 4;
static constexpr int kGlImageDetail = 5;          // D
static constexpr int kGlImageTemporary = 6;
static constexpr int kGlImageFont = 7;          // F
static constexpr int kGlImageSphericalTex = 8;

inline unsigned int stget_u32(uintptr_t addr) {
    return *reinterpret_cast<unsigned int*>(addr);
}

inline unsigned int stget(uintptr_t base, uintptr_t offset) {
    return *reinterpret_cast<unsigned int*>(base + offset);
}

inline bool IsSsLoading() {
    return static_cast<int>(stget_u32(kSvStateAddr)) == kSvStateLoading;
}

bool IsEnabled();
// Attack intersect: polled from Qcommon_Frame (exe) so it survives gamex86 reload; do not rely on ClientThink hook.
void PollAttackIntersectForLocalPlayer(void);
void Intersect(void* player_ent, bool quiet_miss = true);
// Console/debug: raycast pick without relying on usercmd (same math as attack intersect).
void IntersectOnceFromCommand(void);
// Clears attack-intersect latch (Qcommon_Frame poll). Call on map load and after gamex86 reload (Sys_GetGameApi).
void ClearClientThinkAttackLatchState(void);

// ED_CallSpawn / G_FreeEdict: stack only while ss_loading — snapshot self-freeing ents; cache geometry for
// sofbuddy_entities_draw.
void PushSpawnStack(void* ent);
void PopSpawnStack(void* ent);
void FinalizeSpawnDraw(void* ent);
void OnFreeEdictBeforeOriginal(void* ed);

} // namespace ev

#endif // FEATURE_ENTITY_VISUALIZER
