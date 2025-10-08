/*
	Scaled UI - CVars
	
	This file contains all cvar declarations and registration for the scaled_ui feature.
	
	CVars:
	- _sofbuddy_font_scale: Scale factor for console/UI fonts (default: 1.0)
	- _sofbuddy_console_size: Console size as fraction of screen (default: 0.5)
	- _sofbuddy_hud_scale: Scale factor for HUD elements (default: 1.0)
*/

#include "feature_config.h"

#if FEATURE_UI_SCALING

#include "sof_buddy.h"
#include "sof_compat.h"

// CVar declarations
cvar_t * _sofbuddy_font_scale = NULL;
cvar_t * _sofbuddy_console_size = NULL;
cvar_t * _sofbuddy_hud_scale = NULL;

// Forward declarations of change callbacks (defined in hooks.cpp)
extern void fontscale_change(cvar_t * cvar);
extern void consolesize_change(cvar_t * cvar);
extern void hudscale_change(cvar_t * cvar);

/*
	Create and register all scaled_ui cvars
	
	Called during initialization to register all CVars with the engine.
	Note: The actual initialization happens in my_Con_Init() in hooks.cpp,
	but we provide this function for consistency with the feature system.
*/
void create_scaled_ui_cvars(void) {
    
	// Font scale: rounds to nearest 0.25 (e.g., 1.0, 1.25, 1.5, 2.0)
    _sofbuddy_font_scale = orig_Cvar_Get("_sofbuddy_font_scale", "1", CVAR_ARCHIVE, fontscale_change);
    
    // Console size: fraction of screen height (0.0 to 1.0)
    _sofbuddy_console_size = orig_Cvar_Get("_sofbuddy_console_size", "0.5", CVAR_ARCHIVE, consolesize_change);
    
    // HUD scale: rounds to nearest 0.25 (e.g., 1.0, 1.25, 1.5, 2.0)
    _sofbuddy_hud_scale = orig_Cvar_Get("_sofbuddy_hud_scale", "1", CVAR_ARCHIVE, hudscale_change);

}

#endif // FEATURE_UI_SCALING
