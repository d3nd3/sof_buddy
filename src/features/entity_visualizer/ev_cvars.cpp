#include "feature_config.h"

#if FEATURE_ENTITY_VISUALIZER

#include "generated_detours.h"
#include "ev_internal.h"

namespace ev {

cvar_t* _sofbuddy_entity_edit = nullptr;
cvar_t* _sofbuddy_entities_draw_verbose = nullptr;

void create_entity_visualizer_cvars(void)
{
	// Non-zero: enables map-entity tools — spawn cache (ss_loading + linger), `sofbuddy_entities_draw` /
	// `ev_debugbox`, verbose wireframe logs, attack-button intersect. Off by default so normal play / hosted
	// games do not run map study hooks unless explicitly enabled.
	_sofbuddy_entity_edit =
	    detour_Cvar_Get::oCvar_Get("_sofbuddy_entity_edit", "0", 0, nullptr);
	_sofbuddy_entities_draw_verbose =
	    detour_Cvar_Get::oCvar_Get("_sofbuddy_entities_draw_verbose", "0", 0, nullptr);
	// Map study menu (F12 → Map study): map stem for "load + record spawns" (no path, no .bsp).
	detour_Cvar_Get::oCvar_Get("_sofbuddy_map_debug_map", "", CVAR_SOFBUDDY_ARCHIVE, nullptr);
	// Engine `deathmatch` mode (games_t) before map; default CTF (4) for typical map testing.
	detour_Cvar_Get::oCvar_Get("_sofbuddy_map_study_deathmatch", "4", CVAR_SOFBUDDY_ARCHIVE, nullptr);
}

} // namespace ev

#endif // FEATURE_ENTITY_VISUALIZER
