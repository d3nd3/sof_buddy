/*
	Texture Filtering Configuration
	
	This feature allows customization of OpenGL texture filtering modes (min/mag filters)
	for different texture types (mipped, unmipped, and UI textures). The feature provides
	CVars to control GL_TEXTURE_MIN_FILTER and GL_TEXTURE_MAG_FILTER parameters.
*/

#include "feature_config.h"

#if FEATURE_TEXTURE_MAPPING_MIN_MAG
#include "sof_compat.h"
#include "util.h"
#include <string.h>
#include "shared.h"

// OpenGL texture filtering constants
#define GL_NEAREST                    0x2600
#define GL_LINEAR                     0x2601
#define GL_NEAREST_MIPMAP_NEAREST     0x2700
#define GL_LINEAR_MIPMAP_NEAREST      0x2701
#define GL_NEAREST_MIPMAP_LINEAR      0x2702
#define GL_LINEAR_MIPMAP_LINEAR       0x2703

// Filter mapping structure
typedef struct
{
	const char *name;
	int gl_code;
} filter_mapping;

// Available minification filter modes
static filter_mapping min_filter_modes[] = {
	{"GL_NEAREST", GL_NEAREST},
	{"GL_LINEAR", GL_LINEAR},
	{"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST},
	{"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST},
	{"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR},
	{"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR}
};

// Available magnification filter modes
static filter_mapping mag_filter_modes[] = {
	{"GL_NEAREST", GL_NEAREST},
	{"GL_LINEAR", GL_LINEAR}
};

// Current filter settings
static int minfilter_unmipped = GL_LINEAR;
static int magfilter_unmipped = GL_LINEAR;
static int minfilter_mipped = GL_LINEAR_MIPMAP_LINEAR;
static int magfilter_mipped = GL_LINEAR;
static int minfilter_ui = GL_NEAREST;
static int magfilter_ui = GL_NEAREST;

// External references to engine CVars
extern cvar_t* _gl_texturemode;  // Declared in sof_compat.h

// Forward declarations
void create_texturemapping_cvars(void);



glTexParameterf_t orig_glTexParameterf = NULL;

/*
	R_Init->GL_SetDefaultState() is handling texturemode for us, yet we are after that.
	So this patch is late. Thus we want to call it again, from R_beginFrame()

	Before, gl_texturemode would only affect:
	  sky/detailtextures(unmipped) using MAG_FILTER(gl_texturemode) for both min and mag
	  normal in-game mipped textures using MIN_FILTER(gl_texturemode) and MAG_FILTER(gl_texturemode)
	  UI was untouched, hardcode to NEAREST/NEAREST.

	Considering defaulting the UI to GL_LINEAR for MAG. For re-scale HUD aesthetics.

	gl_texturemode values:
	==GL_NEAREST==
	  MIN: GL_NEAREST
	  MAG: GL_NEAREST
	==GL_LINEAR==
	  MIN: GL_LINEAR
	  MAG: GL_LINEAR
	==GL_NEAREST_MIPMAP_NEAREST==
	  MIN: GL_NEAREST_MIPMAP_NEAREST
	  MAG: GL_NEAREST
	==GL_LINEAR_MIPMAP_NEAREST==
	  MIN: GL_LINEAR_MIPMAP_NEAREST 
	  MAG: GL_LINEAR
	==GL_NEAREST_MIPMAP_LINEAR==
	  MIN: GL_NEAREST_MIPMAP_LINEAR
	  MAG: GL_NEAREST
	==GL_LINEAR_MIPMAP_LINEAR==
	  MIN: GL_LINEAR_MIPMAP_LINEAR
	  MAG: GL_LINEAR  

	So:  
		MAG can be:  
			GL_NEAREST  
			GL_LINEAR  
			( can't use mipmaps ).  
		MIN can be:  
			GL_NEAREST_MIPMAP_NEAREST  
				Chooses the mipmap that most closely matches the size of the pixel being textured and uses the GL_NEAREST criterion  
				(the texture element nearest to the center of the pixel) to produce a texture value.
			GL_NEAREST_MIPMAP_LINEAR  
				Chooses the two mipmaps that most closely match the size of the pixel being textured and uses the GL_NEAREST criterion  
				(the texture element nearest to the center of the pixel) to produce a texture value from each mipmap.  
				The final texture value is a weighted average of those two values.
			GL_LINEAR_MIPMAP_NEAREST  
				Chooses the mipmap that most closely matches the size of the pixel being textured and uses the GL_LINEAR criterion  
				(a weighted average of the four texture elements that are closest to the center of the pixel) to produce a texture value.
			GL_LINEAR_MIPMAP_LINEAR			
				Chooses the two mipmaps that most closely match the size of the pixel being textured and uses the GL_LINEAR criterion			
				(a weighted average of the four texture elements that are closest to the center of the pixel) to produce a texture value from each mipmap.
				The final texture value is a weighted average of those two values.

	//how many mipmaps to sample
	GL_SOMETHING_MIPMAP_LINEAR = pick 2 mipmap that most closely matches size. and take the weighted average between them.
	GL_SOMETHING_MIPMAP_NEAREST = pick 1 mipmap that most closely matches size.
	//texel selection mechanism
	GL_NEAREST_MIPMAP_SOMETHING = the texel nearest to the center of the pixel
	GL_LINEAR_MIPMAP_SOMETHING = a weighted average of the four texels that are closest to the center of the pixel

	MIN == DISTANT ( zoomed out )
	MAG == SUPER CLOSE ( zoomed in )

	There are six defined minifying functions. 
	Two of them use the nearest one or nearest four texture elements to compute the texture value.
	The other four use mipmaps.
*/

void __stdcall orig_glTexParameterf_min_mipped(int target_tex, int param_name, float value) {
	if (orig_glTexParameterf) orig_glTexParameterf(target_tex,param_name,(float)minfilter_mipped);
}

void __stdcall orig_glTexParameterf_mag_mipped(int target_tex, int param_name, float value) {
	if (orig_glTexParameterf) orig_glTexParameterf(target_tex,param_name,(float)magfilter_mipped);
}

void __stdcall orig_glTexParameterf_min_unmipped(int target_tex, int param_name, float value) {
	if (orig_glTexParameterf) orig_glTexParameterf(target_tex,param_name,(float)minfilter_unmipped);
}

void __stdcall orig_glTexParameterf_mag_unmipped(int target_tex, int param_name, float value) {
	if (orig_glTexParameterf) orig_glTexParameterf(target_tex,param_name,(float)magfilter_unmipped);
}

void __stdcall orig_glTexParameterf_min_ui(int target_tex, int param_name, float value) {
	if (orig_glTexParameterf) orig_glTexParameterf(target_tex,param_name,(float)minfilter_ui);
}

void __stdcall orig_glTexParameterf_mag_ui(int target_tex, int param_name, float value) {
	if (orig_glTexParameterf) orig_glTexParameterf(target_tex,param_name,(float)magfilter_ui);
}

static const char* filter_name_for_code(filter_mapping* modes, int n, int code) {
	for (int i = 0; i < n; i++)
		if (modes[i].gl_code == code) return modes[i].name;
	return modes[0].name;
}

static bool is_mipmap_filter(int code) {
	return code >= GL_NEAREST_MIPMAP_NEAREST && code <= GL_LINEAR_MIPMAP_LINEAR;
}

// Reset poison/invalid string to last applied value (re-enters change cb with a valid name).
static void reset_filter_cvar(cvar_t* cvar, const char* value) {
	if (!cvar || !value || !detour_Cvar_Set2::oCvar_Set2) return;
	if (cvar->string && !strcmp(cvar->string, value)) return;
	detour_Cvar_Set2::oCvar_Set2(const_cast<char*>(cvar->name), const_cast<char*>(value), true);
}

void minfilter_change(cvar_t * cvar) {
	int* slot = nullptr;
	bool no_mipmap = false;
	if (!strcmp(cvar->name, "_sofbuddy_minfilter_unmipped")) { slot = &minfilter_unmipped; no_mipmap = true; }
	else if (!strcmp(cvar->name, "_sofbuddy_minfilter_mipped")) { slot = &minfilter_mipped; }
	else if (!strcmp(cvar->name, "_sofbuddy_minfilter_ui")) { slot = &minfilter_ui; no_mipmap = true; }
	if (!slot) return;

	for (int i = 0; i < 6; i++) {
		if (strcmp(min_filter_modes[i].name, cvar->string)) continue;
		if (no_mipmap && is_mipmap_filter(min_filter_modes[i].gl_code)) {
			PrintOut(PRINT_BAD, "texture_mapping: %s rejects MIPMAP (%s) — textures have no mipmaps\n",
			         cvar->name, cvar->string);
			reset_filter_cvar(cvar, filter_name_for_code(min_filter_modes, 6, *slot));
			return;
		}
		*slot = min_filter_modes[i].gl_code;
		PrintOut(PRINT_LOG, "texture_mapping: %s set to %s\n", cvar->name, min_filter_modes[i].name);
		if (_gl_texturemode) _gl_texturemode->modified = true;
		return;
	}
	PrintOut(PRINT_BAD, "Invalid minfilter value: %s\n", cvar->string);
	reset_filter_cvar(cvar, filter_name_for_code(min_filter_modes, 6, *slot));
}

void magfilter_change(cvar_t * cvar) {
	int* slot = nullptr;
	if (!strcmp(cvar->name, "_sofbuddy_magfilter_unmipped")) slot = &magfilter_unmipped;
	else if (!strcmp(cvar->name, "_sofbuddy_magfilter_mipped")) slot = &magfilter_mipped;
	else if (!strcmp(cvar->name, "_sofbuddy_magfilter_ui")) slot = &magfilter_ui;
	if (!slot) return;

	for (int i = 0; i < 2; i++) {
		if (strcmp(mag_filter_modes[i].name, cvar->string)) continue;
		*slot = mag_filter_modes[i].gl_code;
		PrintOut(PRINT_LOG, "texture_mapping: %s set to %s\n", cvar->name, mag_filter_modes[i].name);
		if (_gl_texturemode) _gl_texturemode->modified = true;
		return;
	}
	PrintOut(PRINT_BAD, "Invalid magfilter value: %s. Valid: GL_NEAREST, GL_LINEAR\n", cvar->string);
	reset_filter_cvar(cvar, filter_name_for_code(mag_filter_modes, 2, *slot));
}

#endif // FEATURE_TEXTURE_MAPPING_MIN_MAG