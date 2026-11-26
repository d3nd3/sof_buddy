#include "feature_config.h"

#if FEATURE_LIGHTING_BLEND

#include "util.h"
#include "generated_detours.h"
#include "runtime_features.h"
#include "../shared.h"
#include <string.h>

void lightblend_PostCvarInit()
{
	PrintOut(PRINT_LOG, "lighting_blend: Registering CVars\n");
	create_lightingblend_cvars();
	lighting_register_commands();

	const bool feature_enabled =
		RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_LIGHTING_BLEND);
	const bool overbright_on = feature_enabled &&
		_sofbuddy_lighting_overbright && _sofbuddy_lighting_overbright->value != 0.0f;
	const int preset_value = _sofbuddy_lighting_preset
		? static_cast<int>(_sofbuddy_lighting_preset->value + 0.5f)
		: LIGHTING_PRESET_VANILLA;
	const bool old_raven_preset = feature_enabled && preset_value == LIGHTING_PRESET_RAVEN_2X;
	const bool legacy_custom = feature_enabled &&
		((_sofbuddy_lightblend_src && _sofbuddy_lightblend_src->string &&
		  strcmp(_sofbuddy_lightblend_src->string, "GL_ZERO") != 0) ||
		 (_sofbuddy_lightblend_dst && _sofbuddy_lightblend_dst->string &&
		  strcmp(_sofbuddy_lightblend_dst->string, "GL_SRC_COLOR") != 0));

	if (old_raven_preset) {
		// Old configs stored Raven 2x in the preset list. Promote it to the
		// independent Overbright Lighting toggle and leave a curve style
		// underneath so turning the toggle off does not fall back into Raven.
		if (detour_Cvar_Set2::oCvar_Set2) {
			if (!overbright_on) {
				detour_Cvar_Set2::oCvar_Set2(
					const_cast<char*>("_sofbuddy_lighting_overbright"),
					const_cast<char*>("1"), true);
			}
			detour_Cvar_Set2::oCvar_Set2(
				const_cast<char*>("_sofbuddy_lighting_preset"),
				const_cast<char*>("0"), true);
		}
		lighting_apply_effective_settings(false);
	} else if (legacy_custom) {
		// Populate the custom blend values and let the change callbacks select
		// the Custom preset. If Overbright Lighting is also on it wins, and
		// Custom is left selected for when the toggle is turned off.
		lightblend_change(_sofbuddy_lightblend_src);
		lightblend_change(_sofbuddy_lightblend_dst);
		lighting_apply_effective_settings(false);
	} else {
		lighting_apply_effective_settings(false);
	}

	// Put back a multitexture value that a previous Raven/Custom session had
	// forced off, before the renderer is initialised for this launch.
	lighting_restore_multitexture_at_startup();
}

#endif // FEATURE_LIGHTING_BLEND

