/*
	Local MP Full Movescale

	SoF's engine registers g_movescale (default 1.0, archived, serverinfo) and
	the game DLL exposes it as the "Movescale" server option (Half = 0.65,
	Full = 1.0). In a local listen server the client's saved value is reused as
	the server value, so a stale/leftover Half setting silently slows the whole
	game. Remote servers are unaffected because the server's own value arrives
	through the playerState movescale byte.

	When gamex86 is loaded for a locally hosted multiplayer game, force
	g_movescale to 1.0 for the lifetime of that server and restore the saved
	value when the game DLL shuts down. Dedicated servers are left alone.
*/

#include "feature_config.h"

#if FEATURE_LOCAL_MP_FULL_MOVESCALE

#include <string>

#include "util.h"
#include "generated_detours.h"
#include "runtime_features.h"
#include "../shared.h"

namespace {

std::string s_saved_movescale;
bool s_override_active = false;

bool lmfm_enabled(void) {
	if (!RuntimeFeatures::Enabled(
	        RuntimeFeatures::Feature::RUNTIME_FEATURE_LOCAL_MP_FULL_MOVESCALE))
		return false;
	return _sofbuddy_local_mp_full_movescale &&
	       _sofbuddy_local_mp_full_movescale->value != 0.0f;
}

} // namespace

void lmfm_GameDllLoaded(void* game_export) {
	(void)game_export;
	if (s_override_active || !lmfm_enabled()) return;

	// Only multiplayer: singleplayer/co-op use the dmnone movescale instead.
	cvar_t* deathmatch = findCvar(const_cast<char*>("deathmatch"));
	if (!deathmatch || deathmatch->value == 0.0f) return;

	// Dedicated servers own their configuration; do not rewrite it.
	cvar_t* dedicated = findCvar(const_cast<char*>("dedicated"));
	if (dedicated && dedicated->value != 0.0f) return;

	cvar_t* movescale = findCvar(const_cast<char*>("g_movescale"));
	if (!movescale || !detour_Cvar_Set2::oCvar_Set2) return;
	if (movescale->value == 1.0f) return;

	s_saved_movescale = movescale->string ? movescale->string : "1";
	s_override_active = true;
	detour_Cvar_Set2::oCvar_Set2(
		const_cast<char*>("g_movescale"), const_cast<char*>("1"), true);

	if (detour_Com_Printf::oCom_Printf) {
		detour_Com_Printf::oCom_Printf(
			"SoF Buddy: local multiplayer movescale %s -> 1 (Full).\n",
			s_saved_movescale.c_str());
	}
	PrintOut(PRINT_LOG,
	         "local_mp_full_movescale: g_movescale %s -> 1 for local server\n",
	         s_saved_movescale.c_str());
}

void lmfm_restore_movescale(void) {
	if (!s_override_active) return;
	s_override_active = false;

	// If the user changed the cvar during the session, keep their value.
	cvar_t* current = findCvar(const_cast<char*>("g_movescale"));
	if (current && current->value != 1.0f) {
		s_saved_movescale.clear();
		return;
	}

	if (detour_Cvar_Set2::oCvar_Set2 && !s_saved_movescale.empty()) {
		detour_Cvar_Set2::oCvar_Set2(const_cast<char*>("g_movescale"),
		                             const_cast<char*>(s_saved_movescale.c_str()),
		                             true);
	}
	s_saved_movescale.clear();
}

#endif // FEATURE_LOCAL_MP_FULL_MOVESCALE
