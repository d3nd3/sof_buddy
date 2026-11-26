/*
	Force Run Singleplayer

	SoF's client builds the +speed (run) button state in CL_BaseMove from the
	cl_run cvar: with cl_run 0 (stock) the player walks unless the run key is
	held. SoF Plus's Community Edition simply sets "cl_run 1" for its first-run
	defaults.

	This hook applies that same state for singleplayer commands only, without
	touching the user's saved cl_run value. The engine's own walk key still
	works, so holding +speed walks. Multiplayer is left alone.
*/

#include "feature_config.h"

#if FEATURE_FORCERUN_SINGLEPLAYER

#include "util.h"
#include "generated_detours.h"
#include "runtime_features.h"
#include "../shared.h"

namespace {

// SoF.exe sv.state (same server_state_t order as Quake 2: ss_dead=0,
// ss_loading=1, ss_game=2). A remote multiplayer client leaves this at ss_dead,
// so it cannot be spoofed by changing the deathmatch cvar.
constexpr uintptr_t kSoFServerStateRva = 0x003A1F20u;

unsigned forcerun_server_state(void) {
	return *reinterpret_cast<const unsigned*>(
		rvaToAbsExe(reinterpret_cast<void*>(kSoFServerStateRva)));
}

bool forcerun_is_singleplayer(void) {
	const unsigned state = forcerun_server_state();
	if (state != 1 && state != 2) return false;  // ss_loading / ss_game only
	// The client mirrors the server's deathmatch cvar (0 in the campaign).
	cvar_t* deathmatch = findCvar(const_cast<char*>("deathmatch"));
	return !deathmatch || deathmatch->value == 0.0f;
}

bool forcerun_active(void) {
	if (!RuntimeFeatures::Enabled(
	        RuntimeFeatures::Feature::RUNTIME_FEATURE_FORCERUN_SINGLEPLAYER))
		return false;
	if (!_sofbuddy_forcerun_singleplayer ||
	    _sofbuddy_forcerun_singleplayer->value == 0.0f)
		return false;
	return forcerun_is_singleplayer();
}

} // namespace

void hkCL_BaseMove(void* cmd, detour_CL_BaseMove::tCL_BaseMove original) {
	if (!original) return;
	if (!forcerun_active()) {
		original(cmd);
		return;
	}

	cvar_t* run = findCvar(const_cast<char*>("cl_run"));
	if (!run) {
		original(cmd);
		return;
	}

	// Scope the override to this command so a saved cl_run preference (and any
	// menu/config change) is never overwritten by the feature.
	const float saved = run->value;
	run->value = 1.0f;
	original(cmd);
	run->value = saved;
}

#endif // FEATURE_FORCERUN_SINGLEPLAYER
