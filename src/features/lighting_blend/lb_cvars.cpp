/*
	Lighting Blend - CVars
	
	This file contains all cvar declarations and registration for the lighting_blend feature.
*/

#include "feature_config.h"

#if FEATURE_LIGHTING_BLEND

#include "shared.h"

#ifndef NULL
#define NULL 0
#endif

// CVar declarations
cvar_t * _sofbuddy_lighting_overbright = NULL;
cvar_t * _sofbuddy_lighting_cutoff = NULL;
cvar_t * _sofbuddy_water_size = NULL;
cvar_t * _sofbuddy_lightblend_src = NULL;
cvar_t * _sofbuddy_lightblend_dst = NULL;
cvar_t * _sofbuddy_shiny_spherical = NULL;
cvar_t * gl_ext_multitexture = NULL;

/*
	Create and register all lighting_blend cvars
*/
void create_lightingblend_cvars(void) {
	_sofbuddy_lighting_overbright = orig_Cvar_Get("_sofbuddy_lighting_overbright","0",CVAR_ARCHIVE,&lighting_overbright_change);
	_sofbuddy_lighting_cutoff = orig_Cvar_Get("_sofbuddy_lighting_cutoff","64",CVAR_ARCHIVE,&lighting_cutoff_change);
	_sofbuddy_water_size = orig_Cvar_Get("_sofbuddy_water_size","64",CVAR_ARCHIVE,&water_size_change);
	
	_sofbuddy_lightblend_src = orig_Cvar_Get("_sofbuddy_lightblend_src","GL_ZERO",CVAR_ARCHIVE,&lightblend_change);
	_sofbuddy_lightblend_dst = orig_Cvar_Get("_sofbuddy_lightblend_dst","GL_SRC_COLOR",CVAR_ARCHIVE,&lightblend_change);
	_sofbuddy_shiny_spherical = orig_Cvar_Get("_sofbuddy_shiny_spherical","1",CVAR_ARCHIVE,&shiny_spherical_change);
	gl_ext_multitexture = orig_Cvar_Get("gl_ext_multitexture","1",CVAR_ARCHIVE,NULL);
}

#endif // FEATURE_LIGHTING_BLEND
