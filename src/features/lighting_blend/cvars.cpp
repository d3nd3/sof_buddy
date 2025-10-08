/*
	Lighting Blend - CVars
	
	This file contains all cvar declarations and registration for the lighting_blend feature.
*/

#include "../../../hdr/feature_config.h"

#if FEATURE_LIGHTING_BLEND

#include "../../../hdr/sof_compat.h"

#ifndef NULL
#define NULL 0
#endif

// CVar declarations
cvar_t * _sofbuddy_whiteraven_lighting = NULL;
cvar_t * _sofbuddy_lightblend_src = NULL;
cvar_t * _sofbuddy_lightblend_dst = NULL;

// Forward declaration of change callback (defined in hooks.cpp)
extern void lightblend_change(cvar_t * cvar);

/*
	Create and register all lighting_blend cvars
*/
void create_lightingblend_cvars(void) {
	_sofbuddy_whiteraven_lighting = orig_Cvar_Get("_sofbuddy_whiteraven_lighting","0",CVAR_ARCHIVE,NULL);
	
	_sofbuddy_lightblend_src = orig_Cvar_Get("_sofbuddy_lightblend_src","GL_ZERO",CVAR_ARCHIVE,&lightblend_change);
	_sofbuddy_lightblend_dst = orig_Cvar_Get("_sofbuddy_lightblend_dst","GL_SRC_COLOR",CVAR_ARCHIVE,&lightblend_change);
}

#endif // FEATURE_LIGHTING_BLEND
