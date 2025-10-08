/*
	Texture Mapping Min/Mag - CVars
	
	This file contains all cvar declarations and registration for the texture_mapping_min_mag feature.
*/

#include "../../../hdr/feature_config.h"

#if FEATURE_TEXTURE_MAPPING_MIN_MAG

#include "../../../hdr/sof_buddy.h"
#include "../../../hdr/sof_compat.h"

// CVar declarations
cvar_t * _sofbuddy_minfilter_unmipped = NULL;
cvar_t * _sofbuddy_magfilter_unmipped = NULL;
cvar_t * _sofbuddy_minfilter_mipped = NULL;
cvar_t * _sofbuddy_magfilter_mipped = NULL;
cvar_t * _sofbuddy_minfilter_ui = NULL;
cvar_t * _sofbuddy_magfilter_ui = NULL;

// Engine CVars (created by ref.dll, accessed for feature coordination)
cvar_t * _gl_texturemode = NULL;

// Forward declarations of change callbacks (defined in hooks.cpp)
extern void minfilter_change(cvar_t * cvar);
extern void magfilter_change(cvar_t * cvar);

/*
	Create and register all texture_mapping_min_mag cvars
*/
void create_texturemapping_cvars(void) {
	// Sky detailtextures (unmipped)
	_sofbuddy_minfilter_unmipped = orig_Cvar_Get("_sofbuddy_minfilter_unmipped","GL_LINEAR",CVAR_ARCHIVE,minfilter_change);
	_sofbuddy_magfilter_unmipped = orig_Cvar_Get("_sofbuddy_magfilter_unmipped","GL_LINEAR",CVAR_ARCHIVE,magfilter_change);
	
	// Mipped textures
	// Important ones - obtain a value from both mipmaps and take the weighted average between the 2 values - sample each mipmap by using 4 neighbours of each.
	_sofbuddy_minfilter_mipped = orig_Cvar_Get("_sofbuddy_minfilter_mipped","GL_LINEAR_MIPMAP_LINEAR",CVAR_ARCHIVE,minfilter_change);
	_sofbuddy_magfilter_mipped = orig_Cvar_Get("_sofbuddy_magfilter_mipped","GL_LINEAR",CVAR_ARCHIVE,magfilter_change);
	
	// UI - left magfilter_ui at GL_NEAREST for now because fonts look really bad with LINEAR, even tho others might be better at 4k
	_sofbuddy_minfilter_ui = orig_Cvar_Get("_sofbuddy_minfilter_ui","GL_NEAREST",CVAR_ARCHIVE,minfilter_change);
	_sofbuddy_magfilter_ui = orig_Cvar_Get("_sofbuddy_magfilter_ui","GL_NEAREST",CVAR_ARCHIVE,magfilter_change);
}

#endif // FEATURE_TEXTURE_MAPPING_MIN_MAG
