/*
	Light Style Interpolation - CVars

	Adds the optional smooth light-style animation toggle.
*/

#include "feature_config.h"

#if FEATURE_LIGHTSTYLE_INTERPOLATION

#include "generated_detours.h"
#include "shared.h"

cvar_t * _sofbuddy_lightstyle_interpolation = NULL;

void lightstyle_interpolation_change(cvar_t * cvar) {
	if (!cvar) return;
	PrintOut(PRINT_LOG, "lightstyle_interpolation: %s = %s\n", cvar->name, cvar->string);
}

void create_lightstyle_interpolation_cvars(void) {
	_sofbuddy_lightstyle_interpolation = detour_Cvar_Get::oCvar_Get(
		"_sofbuddy_lightstyle_interpolation",
		"0",
		CVAR_SOFBUDDY_ARCHIVE,
		&lightstyle_interpolation_change);
}

#endif // FEATURE_LIGHTSTYLE_INTERPOLATION
