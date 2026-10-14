#include "feature_config.h"

#if FEATURE_ENTITY_VISUALIZER

#include "../ev_internal.h"
#include "generated_detours.h"

void entity_visualizer_PostCvarInit() {
	if (detour_Com_Printf::oCom_Printf)
		detour_Com_Printf::oCom_Printf("[entity_visualizer] registering CVars and commands\n");
	ev::create_entity_visualizer_cvars();
	ev::register_entity_visualizer_commands();
	ev::register_entity_visualizer_map_study_commands();
}

#endif // FEATURE_ENTITY_VISUALIZER
