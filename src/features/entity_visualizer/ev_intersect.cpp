#include "feature_config.h"

#if FEATURE_ENTITY_VISUALIZER

#include <cmath>
#include <cstring>
#include <unordered_map>

#include "ev_internal.h"
#include "generated_detours.h"

namespace ev {

namespace {
// Same forward vector as q_shared AngleVectors (forward only) — used if engine AngleVectors pointer is null.
void ForwardFromViewAngles(float const* angles, float* forward) {
	float const pitchRad = angles[0] * (6.28318530718f / 360.f);
	float const yawRad = angles[1] * (6.28318530718f / 360.f);
	float const sp = std::sin(pitchRad);
	float const cp = std::cos(pitchRad);
	float const sy = std::sin(yawRad);
	float const cy = std::cos(yawRad);
	forward[0] = cp * cy;
	forward[1] = cp * sy;
	forward[2] = -sp;
}
} // namespace

namespace {
std::unordered_map<uintptr_t, unsigned int> g_prevGclientButtonsPoll;
std::unordered_map<uintptr_t, unsigned>     g_lastHeldIntersectMsPoll;
constexpr unsigned                        kHeldAttackIntersectRepeatMs = 100u;
} // namespace

void Intersect(void* player_ent, bool quiet_miss);

void ClearClientThinkAttackLatchState(void) {
	g_prevGclientButtonsPoll.clear();
	g_lastHeldIntersectMsPoll.clear();
}

void PollAttackIntersectForLocalPlayer(void) {
	if (!IsEnabled()) {
		return;
	}
	unsigned const base = stget_u32(kEdictsBasePtrAddr);
	unsigned const num  = stget_u32(kNumEdictsPtrAddr);
	if (!base || !num) {
		return;
	}
	unsigned const limit = base + kSizeOfEdict * num;
	unsigned char*   player = nullptr;
	for (unsigned addr = base + kSizeOfEdict; addr < limit; addr += kSizeOfEdict) {
		unsigned char* e = reinterpret_cast<unsigned char*>(addr);
		if (!*reinterpret_cast<qboolean*>(e + kEdictInUseOffset)) {
			continue;
		}
		if (!*reinterpret_cast<void**>(e + kEdictGClientOffset)) {
			continue;
		}
		char* cn = *reinterpret_cast<char**>(e + kEdictClassnameOffset);
		if (cn && std::strncmp(cn, "player", 6) == 0) {
			player = e;
			break;
		}
	}
	if (!player) {
		return;
	}
	uintptr_t const ent_key = reinterpret_cast<uintptr_t>(player);
	void* const     client  = *reinterpret_cast<void**>(player + kEdictGClientOffset);
	unsigned int const cur  = ev::stget(reinterpret_cast<uintptr_t>(client), kGClientButtonsOffset);

	unsigned int const prev = g_prevGclientButtonsPoll[ent_key];
	bool const         attackDown = (cur & static_cast<unsigned int>(kButtonAttack)) != 0;
	bool const         edge = ((cur & ~prev) & static_cast<unsigned int>(kButtonAttack)) != 0;

	if (attackDown) {
		if (detour_Sys_Milliseconds::oSys_Milliseconds) {
			unsigned const now = static_cast<unsigned>(detour_Sys_Milliseconds::oSys_Milliseconds());
			unsigned&      last = g_lastHeldIntersectMsPoll[ent_key];
			bool const     repeat = (now >= last + kHeldAttackIntersectRepeatMs);
			if (edge || repeat) {
				Intersect(player);
				last = now;
			}
		} else if (edge) {
			Intersect(player);
		}
	}
	g_prevGclientButtonsPoll[ent_key] = cur;
}

void Intersect(void* player_ent, bool quiet_miss) {
	if (!IsEnabled()) return;

	const unsigned int g_edicts = stget_u32(kEdictsBasePtrAddr);
	const unsigned int num_edicts = stget_u32(kNumEdictsPtrAddr);
	if (!g_edicts || !num_edicts) return;

	unsigned char* player = reinterpret_cast<unsigned char*>(player_ent);
	float* pOrigin = reinterpret_cast<float*>(player + kEdictSOriginOffset);
	void* vClient = *reinterpret_cast<void**>(player + kEdictGClientOffset);
	if (!vClient) return;

	const unsigned int pm_flags = stget(reinterpret_cast<uintptr_t>(vClient), kGClientPmFlagsOffset);
	float* v_angle = reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(vClient) + kGClientVAngleOffset);

	float fwd[3];
	if (detour_AngleVectors::oAngleVectors) {
		detour_AngleVectors::oAngleVectors(v_angle, fwd, nullptr, nullptr);
	} else {
		ForwardFromViewAngles(v_angle, fwd);
	}

	const float fDuck = (pm_flags & kPmfDucked) ? 0.0f : 35.0f;
	float const ox = pOrigin[0];
	float const oy = pOrigin[1];
	float const oz = pOrigin[2] + fDuck;
	float const dx = fwd[0];
	float const dy = fwd[1];
	float const dz = fwd[2];

	float best_t = 1e30f;
	unsigned char* hit_ent = nullptr;

	for (unsigned int addr = g_edicts; addr < g_edicts + kSizeOfEdict * num_edicts; addr += kSizeOfEdict) {
		unsigned char* ent = reinterpret_cast<unsigned char*>(addr);
		const qboolean inuse = *reinterpret_cast<qboolean*>(ent + kEdictInUseOffset);
		if (!inuse || ent == player) continue;
		// Match wireframe merge: skip world edict 0 (not a meaningful AABB for ray pick).
		if (addr == g_edicts) continue;

		const uintptr_t owner_u = static_cast<uintptr_t>(stget(reinterpret_cast<uintptr_t>(ent), kEdictOwnerOffset));
		if (reinterpret_cast<void*>(owner_u) == player_ent) continue;

		float o[3];
		float mn[3];
		float mx[3];
		std::memcpy(o, ent + kEdictSOriginOffset, sizeof(o));
		std::memcpy(mn, ent + kEdictMinsOffset, sizeof(mn));
		std::memcpy(mx, ent + kEdictMaxsOffset, sizeof(mx));

		char** classname_ptr = reinterpret_cast<char**>(ent + kEdictClassnameOffset);
		const char* classname = classname_ptr ? *classname_ptr : nullptr;

		if (EvIsNonFiniteOrigin(o)) continue;
		if (EvSuspiciousLivePickupOrigin(classname, o)) continue;

		EvFinalizeStudyBboxForWireframe(o, mn, mx, classname);
		if (EvIsZeroBounds(o, mn, mx)) {
			continue;
		}

		float const wmin[3] = {o[0] + mn[0], o[1] + mn[1], o[2] + mn[2]};
		float const wmax[3] = {o[0] + mx[0], o[1] + mx[1], o[2] + mx[2]};

		float t = 0.0f;
		if (!EvRayVsAabb(ox, oy, oz, dx, dy, dz, wmin, wmax, t)) {
			continue;
		}
		if (t > 0.f && t < kMinIntersectT) {
			continue;
		}

		if (t < best_t) {
			best_t = t;
			hit_ent = ent;
		}
	}

	char cache_classname[256];
	IntersectMergeCachedLevelRayHits(player_ent, ox, oy, oz, dx, dy, dz, &best_t, &hit_ent, cache_classname,
	                                 sizeof cache_classname);

	if (!detour_Com_Printf::oCom_Printf) {
		return;
	}

	if (hit_ent) {
		char** hit_classname_ptr = reinterpret_cast<char**>(hit_ent + kEdictClassnameOffset);
		const char* classname = hit_classname_ptr ? *hit_classname_ptr : nullptr;
		const unsigned int flags = stget(reinterpret_cast<uintptr_t>(hit_ent), kEdictFlagsOffset);

		if (classname) {
			detour_Com_Printf::oCom_Printf("[entity_visualizer] intersect: %s flags=%u t=%.3f\n", classname, flags,
			                               static_cast<double>(best_t));
		} else {
			detour_Com_Printf::oCom_Printf("[entity_visualizer] intersect: <no classname> flags=%u t=%.3f\n", flags,
			                               static_cast<double>(best_t));
		}
		return;
	}

	if (cache_classname[0]) {
		detour_Com_Printf::oCom_Printf("[entity_visualizer] intersect: %s (cached; edict freed) t=%.3f\n", cache_classname,
		                               static_cast<double>(best_t));
		return;
	}

	if (!quiet_miss) {
		detour_Com_Printf::oCom_Printf("[entity_visualizer] intersect: (no hit)\n");
	}
}

void IntersectOnceFromCommand(void) {
	if (!IsEnabled()) {
		if (detour_Com_Printf::oCom_Printf) {
			detour_Com_Printf::oCom_Printf("[entity_visualizer] sofbuddy_intersect_once requires _sofbuddy_entity_edit 1\n");
		}
		return;
	}
	unsigned const base = stget_u32(kEdictsBasePtrAddr);
	unsigned const num = stget_u32(kNumEdictsPtrAddr);
	if (!base || !num) {
		if (detour_Com_Printf::oCom_Printf) {
			detour_Com_Printf::oCom_Printf("[entity_visualizer] sofbuddy_intersect_once: g_edicts unavailable\n");
		}
		return;
	}
	unsigned const limit = base + kSizeOfEdict * num;
	for (unsigned addr = base + kSizeOfEdict; addr < limit; addr += kSizeOfEdict) {
		unsigned char* e = reinterpret_cast<unsigned char*>(addr);
		if (!*reinterpret_cast<qboolean*>(e + kEdictInUseOffset)) {
			continue;
		}
		if (!*reinterpret_cast<void**>(e + kEdictGClientOffset)) {
			continue;
		}
		char* cn = *reinterpret_cast<char**>(e + kEdictClassnameOffset);
		if (cn && std::strncmp(cn, "player", 6) == 0) {
			Intersect(e, /*quiet_miss=*/false);
			return;
		}
	}
	if (detour_Com_Printf::oCom_Printf) {
		detour_Com_Printf::oCom_Printf("[entity_visualizer] sofbuddy_intersect_once: no player* edict in use\n");
	}
}

} // namespace ev

#endif // FEATURE_ENTITY_VISUALIZER
