/*
	Lighting Blend - CVars
	
	This file contains all cvar declarations and registration for the lighting_blend feature.
*/

#include "feature_config.h"

#if FEATURE_LIGHTING_BLEND

#include "sof_compat.h"

#ifndef NULL
#define NULL 0
#endif

// CVar declarations
cvar_t * _sofbuddy_whiteraven_lighting = NULL;
cvar_t * _sofbuddy_lightblend_src = NULL;
cvar_t * _sofbuddy_lightblend_dst = NULL;
cvar_t * gl_ext_multitexture = NULL;

// Forward declaration of change callbacks (defined in hooks.cpp)
extern void lightblend_change(cvar_t * cvar);
extern void whiteraven_lighting_change(cvar_t * cvar);

/*
	Create and register all lighting_blend cvars
*/
void create_lightingblend_cvars(void) {
	_sofbuddy_whiteraven_lighting = orig_Cvar_Get("_sofbuddy_whiteraven_lighting","0",CVAR_ARCHIVE,&whiteraven_lighting_change);
	
	_sofbuddy_lightblend_src = orig_Cvar_Get("_sofbuddy_lightblend_src","GL_ZERO",CVAR_ARCHIVE,&lightblend_change);
	_sofbuddy_lightblend_dst = orig_Cvar_Get("_sofbuddy_lightblend_dst","GL_SRC_COLOR",CVAR_ARCHIVE,&lightblend_change);
	gl_ext_multitexture = orig_Cvar_Get("gl_ext_multitexture","1",CVAR_ARCHIVE,NULL);
}

#endif // FEATURE_LIGHTING_BLEND
