/*
	Scaled UI - CVars
	
	This file contains all cvar declarations and registration for the scaled_ui feature.
	
	CVars:
	- _sofbuddy_font_scale: Scale factor for console/UI fonts (default: 1.0)
	- _sofbuddy_console_size: Console size as fraction of screen (default: 0.5)
	- _sofbuddy_hud_scale: Scale factor for HUD elements (default: 1.0)
	- _sofbuddy_crossh_scale: Scale factor for crosshair textures (default: 1.0)
*/

#include "feature_config.h"

#if FEATURE_SCALED_CON || FEATURE_SCALED_HUD

#include "sof_buddy.h"
#include "sof_compat.h"

// CVar declarations
#if FEATURE_SCALED_CON
cvar_t * _sofbuddy_font_scale = NULL;
cvar_t * _sofbuddy_console_size = NULL;
#endif
#if FEATURE_SCALED_HUD
cvar_t * _sofbuddy_hud_scale = NULL;
cvar_t * _sofbuddy_crossh_scale = NULL;
#endif

// Forward declarations of change callbacks (defined in hooks.cpp)
#if FEATURE_SCALED_CON
extern void fontscale_change(cvar_t * cvar);
extern void consolesize_change(cvar_t * cvar);
#endif
#if FEATURE_SCALED_HUD
extern void hudscale_change(cvar_t * cvar);
extern void crosshairscale_change(cvar_t * cvar);
#endif

/*
	Create and register all scaled_ui cvars
	
	Called during initialization to register all CVars with the engine.
	Note: The actual initialization happens in my_Con_Init() in hooks.cpp,
	but we provide this function for consistency with the feature system.
*/
void create_scaled_ui_cvars(void) {
#if FEATURE_SCALED_CON
    _sofbuddy_font_scale = orig_Cvar_Get("_sofbuddy_font_scale", "1", CVAR_ARCHIVE, fontscale_change);
    _sofbuddy_console_size = orig_Cvar_Get("_sofbuddy_console_size", "0.5", CVAR_ARCHIVE, consolesize_change);
#endif
#if FEATURE_SCALED_HUD
    _sofbuddy_hud_scale = orig_Cvar_Get("_sofbuddy_hud_scale", "1", CVAR_ARCHIVE, hudscale_change);
    _sofbuddy_crossh_scale = orig_Cvar_Get("_sofbuddy_crossh_scale", "1", CVAR_ARCHIVE, crosshairscale_change);
#endif
}

#endif // FEATURE_SCALED_CON || FEATURE_SCALED_HUD
