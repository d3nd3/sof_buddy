/*
	Texture Filtering Configuration
	
	This feature allows customization of OpenGL texture filtering modes (min/mag filters)
	for different texture types (mipped, unmipped, and UI textures). The feature provides
	CVars to control GL_TEXTURE_MIN_FILTER and GL_TEXTURE_MAG_FILTER parameters.
*/

#include "../../../hdr/feature_config.h"

#if FEATURE_TEXTURE_MAPPING_MIN_MAG

#include "../../../hdr/shared_hook_manager.h"
#include "../../../hdr/sof_compat.h"
#include "../../../hdr/util.h"
#include <string.h>

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
static void texturemapping_PostCvarInit();
static void setup_minmag_filters(void);
void create_texturemapping_cvars(void);

// Hook registrations placed after function declarations for visibility
REGISTER_SHARED_HOOK_CALLBACK(PostCvarInit, texture_mapping, texturemapping_PostCvarInit, 50, Post);
REGISTER_SHARED_HOOK_CALLBACK(RefDllLoaded, texture_mapping, setup_minmag_filters, 70, Post);

/*
	PostCvarInit Callback
	
	Called after CVars are available. This is where we register our custom CVars.
*/
static void texturemapping_PostCvarInit()
{
	PrintOut(PRINT_LOG, "texture_mapping: Registering CVars\n");
	create_texturemapping_cvars();
}

// Forward declarations for GL parameter redirection and setup
typedef void (__stdcall *glTexParameterf_t)(int target_tex, int param_name, float value);
static glTexParameterf_t orig_glTexParameterf = NULL;

static void setup_minmag_filters(void);
void __stdcall orig_glTexParameterf_min_mipped(int target_tex, int param_name, float value);
void __stdcall orig_glTexParameterf_mag_mipped(int target_tex, int param_name, float value);
void __stdcall orig_glTexParameterf_min_unmipped(int target_tex, int param_name, float value);
void __stdcall orig_glTexParameterf_mag_unmipped(int target_tex, int param_name, float value);
void __stdcall orig_glTexParameterf_min_ui(int target_tex, int param_name, float value);
void __stdcall orig_glTexParameterf_mag_ui(int target_tex, int param_name, float value);

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
static cvar_t * gl_swapinterval = NULL;
static cvar_t * vid_ref = NULL;
static void setup_minmag_filters(void) {
	static bool first_run = true;

	// Not necessary to specify flags, because they get stacked, and can't be removed.
	// This is not used anymore, just need it to trigger.
	_gl_texturemode = orig_Cvar_Get("gl_texturemode","GL_LINEAR_MIPMAP_LINEAR",CVAR_ARCHIVE,NULL);

	// This overpowers sofplus _sp_cl_vid_gl_texture_mag_filter.
	// Could later allow his cvar to be used for mag_ui, maybe.
	orig_glTexParameterf = (glTexParameterf_t)(*(int*)0x300A457C);

	// Setup the glTexParameterf hooks for each individual one.
	WriteE8Call((void*)0x30006636 ,(void*)&orig_glTexParameterf_min_mipped);
	WriteByte((void*)0x3000663B,0x90);
	WriteE8Call((void*)0x30006660 ,(void*)&orig_glTexParameterf_mag_mipped);
	WriteByte((void*)0x30006665,0x90);
	WriteE8Call((void*)0x300065DF ,(void*)&orig_glTexParameterf_min_unmipped);
	WriteByte((void*)0x300065E4,0x90);
	WriteE8Call((void*)0x30006609 ,(void*)&orig_glTexParameterf_mag_unmipped);
	WriteByte((void*)0x3000660E,0x90);
	WriteE8Call((void*)0x3000659C ,(void*)&orig_glTexParameterf_min_ui);
	WriteByte((void*)0x300065A1,0x90);
	WriteE8Call((void*)0x300065B1 ,(void*)&orig_glTexParameterf_mag_ui);
	WriteByte((void*)0x300065B6,0x90);

	// Trigger re-apply of texturemode
	if (_gl_texturemode) _gl_texturemode->modified = true;

	// ==init== code for gl_swapinterval fix
	gl_swapinterval = orig_Cvar_Get("gl_swapinterval","0",0,NULL);
	vid_ref = orig_Cvar_Get("vid_ref","gl",0,NULL);
}

void __stdcall orig_glTexParameterf_min_mipped(int target_tex, int param_name, float value) {
	orig_glTexParameterf(target_tex,param_name,(float)minfilter_mipped);
}

void __stdcall orig_glTexParameterf_mag_mipped(int target_tex, int param_name, float value) {
	orig_glTexParameterf(target_tex,param_name,(float)magfilter_mipped);
}

void __stdcall orig_glTexParameterf_min_unmipped(int target_tex, int param_name, float value) {
	orig_glTexParameterf(target_tex,param_name,(float)minfilter_unmipped);
}

void __stdcall orig_glTexParameterf_mag_unmipped(int target_tex, int param_name, float value) {
	orig_glTexParameterf(target_tex,param_name,(float)magfilter_unmipped);
}

void __stdcall orig_glTexParameterf_min_ui(int target_tex, int param_name, float value) {
	orig_glTexParameterf(target_tex,param_name,(float)minfilter_ui);
}

void __stdcall orig_glTexParameterf_mag_ui(int target_tex, int param_name, float value) {
	orig_glTexParameterf(target_tex,param_name,(float)magfilter_ui);
}

/*
	CVar change callback for minification filter settings
	
	This function is called when any of the _sofbuddy_minfilter_* CVars are changed.
	It validates the input and updates the corresponding filter setting.
*/
void minfilter_change(cvar_t * cvar) {
	for (int i = 0; i < 6; i++) {
		if (!strcmp(min_filter_modes[i].name, cvar->string)) {
			if (!strcmp(cvar->name, "_sofbuddy_minfilter_unmipped")) {
				minfilter_unmipped = min_filter_modes[i].gl_code;
				PrintOut(PRINT_LOG, "texture_mapping: minfilter_unmipped set to %s\n", min_filter_modes[i].name);
			} else if (!strcmp(cvar->name, "_sofbuddy_minfilter_mipped")) {
				minfilter_mipped = min_filter_modes[i].gl_code;
				PrintOut(PRINT_LOG, "texture_mapping: minfilter_mipped set to %s\n", min_filter_modes[i].name);
			} else if (!strcmp(cvar->name, "_sofbuddy_minfilter_ui")) {
				minfilter_ui = min_filter_modes[i].gl_code;
				PrintOut(PRINT_LOG, "texture_mapping: minfilter_ui set to %s\n", min_filter_modes[i].name);
			}
			
			// Trigger texture mode refresh if available
			if (_gl_texturemode) {
				_gl_texturemode->modified = true;
			}
			return;
		}
	}
	PrintOut(PRINT_BAD, "Invalid minfilter value: %s. Valid values: GL_NEAREST, GL_LINEAR, "
	                     "GL_NEAREST_MIPMAP_NEAREST, GL_LINEAR_MIPMAP_NEAREST, "
	                     "GL_NEAREST_MIPMAP_LINEAR, GL_LINEAR_MIPMAP_LINEAR\n", cvar->string);
}

/*
	CVar change callback for magnification filter settings
	
	This function is called when any of the _sofbuddy_magfilter_* CVars are changed.
	It validates the input and updates the corresponding filter setting.
*/
void magfilter_change(cvar_t * cvar) {
	for (int i = 0; i < 2; i++) {
		if (!strcmp(mag_filter_modes[i].name, cvar->string)) {
			if (!strcmp(cvar->name, "_sofbuddy_magfilter_unmipped")) {
				magfilter_unmipped = mag_filter_modes[i].gl_code;
				PrintOut(PRINT_LOG, "texture_mapping: magfilter_unmipped set to %s\n", mag_filter_modes[i].name);
			} else if (!strcmp(cvar->name, "_sofbuddy_magfilter_mipped")) {
				magfilter_mipped = mag_filter_modes[i].gl_code;
				PrintOut(PRINT_LOG, "texture_mapping: magfilter_mipped set to %s\n", mag_filter_modes[i].name);
			} else if (!strcmp(cvar->name, "_sofbuddy_magfilter_ui")) {
				magfilter_ui = mag_filter_modes[i].gl_code;
				PrintOut(PRINT_LOG, "texture_mapping: magfilter_ui set to %s\n", mag_filter_modes[i].name);
			}
			
			// Trigger texture mode refresh if available
			if (_gl_texturemode) {
				_gl_texturemode->modified = true;
			}
			return;
		}
	}
	PrintOut(PRINT_BAD, "Invalid magfilter value: %s. Valid values: GL_NEAREST, GL_LINEAR\n", 
	                     cvar->string);
}

#endif // FEATURE_TEXTURE_MAPPING_MIN_MAG