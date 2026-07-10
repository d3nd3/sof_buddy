/*
	Scaled UI - CVars
	
	This file contains all cvar declarations and registration for the scaled_ui feature.
	
	CVars:
	- _sofbuddy_font_scale: Scale factor for console/UI fonts (default: -1 = auto)
	- _sofbuddy_console_size: Console size as fraction of screen (default: 0.5)
	- _sofbuddy_hud_scale: Scale factor for HUD elements (default: -1 = auto)
	- _sofbuddy_crossh_scale: Scale factor for crosshair textures (default: 1.0)
	- _sofbuddy_scale_round_auto: Snap auto font/HUD scale to round steps (default: 0)
	- _sofbuddy_scale_round_ratio: Round step for scale values (default: 0.25)
*/

#include "feature_config.h"

#if FEATURE_SCALED_CON || FEATURE_SCALED_HUD

#include "sof_buddy.h"
#include "sof_compat.h"
#include "generated_detours.h"

#if FEATURE_SCALED_CON
cvar_t * _sofbuddy_font_scale = NULL;
cvar_t * _sofbuddy_console_size = NULL;
#endif
#if FEATURE_SCALED_HUD
cvar_t * _sofbuddy_hud_scale = NULL;
cvar_t * _sofbuddy_crossh_scale = NULL;
cvar_t * _sofbuddy_scale_cinematic_pics = NULL;
#endif

#if FEATURE_SCALED_CON
extern void fontscale_change(cvar_t * cvar);
extern void consolesize_change(cvar_t * cvar);
#endif
#if FEATURE_SCALED_HUD
extern void hudscale_change(cvar_t * cvar);
extern void crosshairscale_change(cvar_t * cvar);
extern bool g_scaleCinematicPics;
static void scalecinematicpics_change(cvar_t * cvar) {
    g_scaleCinematicPics = cvar && cvar->value != 0.0f;
}
#endif
extern void scale_round_auto_change(cvar_t * cvar);
extern void scale_round_ratio_change(cvar_t * cvar);

void create_scaled_ui_cvars(void) {
#if FEATURE_SCALED_CON
    _sofbuddy_font_scale = detour_Cvar_Get::oCvar_Get("_sofbuddy_font_scale", "-1", CVAR_SOFBUDDY_ARCHIVE, fontscale_change);
    fontscale_change(_sofbuddy_font_scale);
    _sofbuddy_console_size = detour_Cvar_Get::oCvar_Get("_sofbuddy_console_size", "0.5", CVAR_SOFBUDDY_ARCHIVE, consolesize_change);
    detour_Cvar_Get::oCvar_Get("_sofbuddy_font_scale_rounded", "1", 0, nullptr);
#endif
#if FEATURE_SCALED_HUD
    _sofbuddy_hud_scale = detour_Cvar_Get::oCvar_Get("_sofbuddy_hud_scale", "-1", CVAR_SOFBUDDY_ARCHIVE, hudscale_change);
    hudscale_change(_sofbuddy_hud_scale);
    _sofbuddy_crossh_scale = detour_Cvar_Get::oCvar_Get("_sofbuddy_crossh_scale", "1", CVAR_SOFBUDDY_ARCHIVE, crosshairscale_change);
    _sofbuddy_scale_cinematic_pics = detour_Cvar_Get::oCvar_Get("_sofbuddy_scale_cinematic_pics", "1", CVAR_SOFBUDDY_ARCHIVE, scalecinematicpics_change);
    scalecinematicpics_change(_sofbuddy_scale_cinematic_pics);
    detour_Cvar_Get::oCvar_Get("_sofbuddy_hud_scale_rounded", "1", 0, nullptr);
#endif
    detour_Cvar_Get::oCvar_Get("_sofbuddy_scale_round_ratio", "0.25", CVAR_SOFBUDDY_ARCHIVE, scale_round_ratio_change);
    detour_Cvar_Get::oCvar_Get("_sofbuddy_scale_round_auto", "0", CVAR_SOFBUDDY_ARCHIVE, scale_round_auto_change);
}

#endif // FEATURE_SCALED_CON || FEATURE_SCALED_HUD
