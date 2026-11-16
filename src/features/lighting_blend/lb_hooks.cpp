/*
	Lighting Blend Tweak
	
	This feature allows customization of OpenGL blend modes used for lighting
	in the ref.dll renderer. It patches memory locations in ref.dll that store
	the source and destination blend factors for glBlendFunc().

	*IMPORTANT* REQUIRES: gl_ext_multitexture 0
*/

#include "feature_config.h"

#if FEATURE_LIGHTING_BLEND
#include "sof_compat.h"
#include "util.h"
#include "shared.h"
#include "DetourXS/detourxs.h"
#include <windows.h>
#include <string.h>
#include <cmath>
#include <stdlib.h>

typedef enum {
	it_skin,
	it_sprite,
	it_wall,
	it_pic,
	it_sky,
	it_detail,
	it_temp,
	it_font,
	it_shiny
} imagetype_t;

static const char* imagetype_to_string(imagetype_t type) {
	switch (type) {
		case it_skin: return "it_skin";
		case it_sprite: return "it_sprite";
		case it_wall: return "it_wall";
		case it_pic: return "it_pic";
		case it_sky: return "it_sky";
		case it_detail: return "it_detail";
		case it_temp: return "it_temp";
		case it_font: return "it_font";
		case it_shiny: return "it_shiny";
		default: return "unknown";
	}
}

// OpenGL blend function constants
#define GL_ZERO                           0x0000
#define GL_ONE                            0x0001
#define GL_SRC_COLOR                      0x0300
#define GL_ONE_MINUS_SRC_COLOR            0x0301
#define GL_DST_COLOR                      0x0306
#define GL_ONE_MINUS_DST_COLOR            0x0307
#define GL_SRC_ALPHA                      0x0302
#define GL_ONE_MINUS_SRC_ALPHA            0x0303
#define GL_DST_ALPHA                      0x0304
#define GL_ONE_MINUS_DST_ALPHA            0x0305
#define GL_CONSTANT_COLOR                 0x8001
#define GL_ONE_MINUS_CONSTANT_COLOR       0x8002
#define GL_CONSTANT_ALPHA                 0x8003
#define GL_ONE_MINUS_CONSTANT_ALPHA       0x8004
#define GL_SRC_ALPHA_SATURATE             0x0308

//SRC = Incoming Data(Light)
//DST = Existing Data(Texture)
//defaults.
int lightblend_src = GL_ZERO;
int lightblend_dst = GL_SRC_COLOR;
static void *shiny_nop_site = nullptr;
static unsigned char shiny_nop_saved[10];
static bool shiny_nop_saved_init = false;
static void *shiny_nop_site3 = nullptr;
static unsigned char shiny_nop3_saved[21];
static bool shiny_nop3_saved_init = false;

// Forward declarations
static void lightblend_ApplySettings();

// Function pointer declarations
void (__stdcall *real_glBlendFunc)(unsigned int, unsigned int) = nullptr;
void (__cdecl *oAdjustTexCoords)(float* player_pos, float* vertex, float* in_normal, float* in_tangent, float* in_bitangent, float* out_new_s_t) = nullptr;
void (__stdcall *glTexCoord2f)(float s, float t) = nullptr;
void (__stdcall *glVertex3f)(float x, float y, float z) = nullptr;
void (__stdcall *glBegin)(unsigned int mode) = nullptr;
void (__stdcall *glEnd)(void) = nullptr;
void (__stdcall *glColor3f)(float r, float g, float b) = nullptr;



// Function declarations
void __stdcall glBlendFunc_R_BlendLightmaps(unsigned int sfactor, unsigned int dfactor);

void __cdecl hkAdjustTexCoords(float* player_pos, float* vertex, float* in_normal, float* in_tangent, float* in_bitangent, float* out_new_s_t);
void lightblend_change(cvar_t * cvar);
void lighting_overbright_change(cvar_t * cvar);
void lighting_cutoff_change(cvar_t * cvar);
void water_size_change(cvar_t * cvar);
void shiny_spherical_change(cvar_t * cvar);



// Callbacks for R_BlendLightmaps
void r_blendlightmaps_pre_callback(void);
void r_blendlightmaps_post_callback(void);



	/*
		Lighting Improrvement gl_ext_multitexture 1
	*/
	/*
	WriteE8Call(0x30015C20,&my_GL_RenderLightmappedPoly_intercept);
	//Jmp to glEnd() afterwards.
	WriteE9Jmp(0x30015C25,0x30015D18);
	//Make push surface_t, instead of image_t (push EAX -> push EBP instead)
	WriteByte(0x30015C1F,0x50);

	WriteByte(0x30015D18,0x90);
	WriteByte(0x30015D19,0x90);
	WriteByte(0x30015D1A,0x90);
	WriteByte(0x30015D1B,0x90);
	WriteByte(0x30015D1C,0x90);
	WriteByte(0x30015D1D,0x90);
	*/

	/*
	glMTexCoord2fSGIS = *(int*)0x300A45EC;
	glMTexCoord2fvSGIS = *(int*)0x300A431C;
	glVertex3fv = *(int*)0x300A4520;
	glBegin = *(int*)0x300A4710;
*/

	/*
		For multiply blending /w gl_ext_multitexture 0.
		R_BlendLightmaps hook is now automatically registered via REGISTER_HOOK macro
	*/


/*
	CVar change callback for _sofbuddy_lighting_overbright
	
	Called when _sofbuddy_lighting_overbright is changed.
	This function is externally referenced in cvars.cpp.
*/
void lighting_overbright_change(cvar_t * cvar) {
	if (gl_ext_multitexture && gl_ext_multitexture->value != 0.0f) {
		detour_Cvar_Set2::oCvar_Set2(const_cast<char*>("gl_ext_multitexture"), const_cast<char*>("0"), true);
		cvar_t * vid_ref = findCvar(const_cast<char*>("vid_ref"));
		if (vid_ref) vid_ref->modified = true;
		PrintOut(PRINT_DEV, "lighting_blend: Set gl_ext_multitexture to 0 and flagged vid_ref for restart.\n");
	}
	
	PrintOut(PRINT_LOG, "lighting_blend: CVar changed: %s\n", cvar->name);
}

/*
	CVar change callback for _sofbuddy_lighting_cutoff
	
	Called when _sofbuddy_lighting_cutoff is changed.
	Writes to ref_gl address 0x2C368 as float*.
	This function is externally referenced in cvars.cpp.
*/
void lighting_cutoff_change(cvar_t * cvar) {
	if (lighting_cutoff_target) {
		writeFloatAt(lighting_cutoff_target, cvar->value);
		PrintOut(PRINT_LOG, "lighting_blend: Set lighting cutoff to %f\n", cvar->value);
	} else {
		PrintOut(PRINT_BAD, "lighting_blend: lighting_cutoff_target not initialized (ref_gl not loaded)\n");
	}
}

/*
	CVar change callback for _sofbuddy_water_size
	
	Called when _sofbuddy_water_size is changed.
	Writes double to ref_gl address 0x2C390 and float to 0x2C398.
	This function is externally referenced in cvars.cpp.
*/
void water_size_change(cvar_t * cvar) {
	if (water_size_double_target && water_size_float_target) {
		if (cvar->value == 0.0f) {
			PrintOut(PRINT_BAD, "lighting_blend: water_size cannot be 0 (division by zero)\n");
			detour_Cvar_Set2::oCvar_Set2(const_cast<char*>("_sofbuddy_water_size"), const_cast<char*>("16"), true);
			return;
		}
		if (cvar->value < 16.0f) {
			PrintOut(PRINT_BAD, "lighting_blend: water_size must be at least 16 (got %f)\n", cvar->value);
			detour_Cvar_Set2::oCvar_Set2(const_cast<char*>("_sofbuddy_water_size"), const_cast<char*>("16"), true);
			return;
		}
		writeDoubleAt(water_size_double_target, (double)cvar->value);
		writeFloatAt(water_size_float_target, 1.0f / cvar->value);
		PrintOut(PRINT_LOG, "lighting_blend: Set water_size to %f (double=0x2C390, float=0x2C398)\n", cvar->value);
	} else {
		PrintOut(PRINT_BAD, "lighting_blend: water_size targets not initialized (ref_gl not loaded)\n");
	}
}

/*
	_sofbuddy_shiny_spherical modes

	0 = no shiny (skip the shiny/env-map pass entirely)
	1 = default (honour SURF_SIMPLE_SPHERICAL and the flat-surface heuristic)
	2 = no spherical (planar-style: camera position only, no view-angle change)
	3 = all spherical (camera position + view angle)
	4 = both (experimental: draw every shiny surface with both projections)
*/
enum shiny_spherical_mode_e {
	SHINY_MODE_OFF = 0,
	SHINY_MODE_DEFAULT = 1,
	SHINY_MODE_NO_SPHERICAL = 2,
	SHINY_MODE_ALL_SPHERICAL = 3,
	SHINY_MODE_DOUBLE = 4
};

static const char *shiny_spherical_mode_name(int mode) {
	switch (mode) {
		case SHINY_MODE_OFF: return "off";
		case SHINY_MODE_DEFAULT: return "default";
		case SHINY_MODE_NO_SPHERICAL: return "no spherical";
		case SHINY_MODE_ALL_SPHERICAL: return "all spherical";
		case SHINY_MODE_DOUBLE: return "both (double)";
		default: return "invalid";
	}
}

static void shiny_spherical_write_branch(unsigned char *target, unsigned char opcode, unsigned char operand) {
	if (!target) return;
	WriteByte(target, opcode);
	WriteByte(target + 1, operand);
}

/*
	Apply the shiny mode to ref_gl's R_DrawDetailChains branches.

	0x14814: branch after testing mtexinfo->flags for SURF_SIMPLE_SPHERICAL
	         74 11 = branch only when the surface flag is set (stock)
	         EB 11 = always take the non-spherical path
	         90 90 = always take the spherical path
	0x14A66: null-mtexinfo guard in the GL_SPHERE_MAP pass
	         74 25 = skip surfaces without texinfo (stock)
	         EB 25 = skip every spherical surface
	0x147DF: "shiny surfaces were collected" test before the shiny pass
	         85 C0 = test eax, eax (stock)
	         31 C0 = xor eax, eax, forcing the following jz to skip the pass
	0x2C434: flat-surface threshold (radiusSquared)
	         0.1f = stock, only floors/ceilings are classified spherical
	         10.1f = every surface is classified spherical (mode 4 double draw)
*/
void shiny_spherical_apply(int mode) {
	if (mode < SHINY_MODE_OFF || mode > SHINY_MODE_DOUBLE) {
		mode = SHINY_MODE_DEFAULT;
	}

	if (!shiny_spherical_target1 && !shiny_spherical_target3 && !shiny_pass_target && !shiny_flat_threshold_target) {
		PrintOut(PRINT_DEV, "lighting_blend: shiny targets not initialized (ref_gl not loaded)\n");
		return;
	}

	const bool no_spherical = mode == SHINY_MODE_NO_SPHERICAL;
	const bool all_spherical = mode == SHINY_MODE_ALL_SPHERICAL;
	const bool double_shiny = mode == SHINY_MODE_DOUBLE;

	unsigned char spherical_opcode = 0x74;
	unsigned char spherical_operand = 0x11;
	if (no_spherical || double_shiny) {
		spherical_opcode = 0xEB;
	} else if (all_spherical) {
		spherical_opcode = 0x90;
		spherical_operand = 0x90;
	}

	// "no spherical" skips the sphere pass; "double" keeps it enabled.
	const unsigned char sphere_texinfo_opcode = no_spherical ? 0xEB : 0x74;
	const unsigned char sphere_texinfo_operand = 0x25;

	const unsigned char pass_opcode = mode == SHINY_MODE_OFF ? 0x31 : 0x85;
	const unsigned char pass_operand = 0xC0;

	shiny_spherical_write_branch(shiny_spherical_target1, spherical_opcode, spherical_operand);
	shiny_spherical_write_branch(shiny_spherical_target3, sphere_texinfo_opcode, sphere_texinfo_operand);
	shiny_spherical_write_branch(shiny_pass_target, pass_opcode, pass_operand);

	// Mode 4 forces the flat-surface test to accept every surface, so every
	// shiny surface is queued for the GL_SPHERE_MAP pass as well as drawn by
	// the non-spherical pass. Other modes restore the stock threshold.
	if (shiny_flat_threshold_target) {
		writeFloatAt(shiny_flat_threshold_target, double_shiny ? 10.1f : 0.1f);
	}

	PrintOut(PRINT_LOG, "lighting_blend: Set shiny mode to %d (%s)\n", mode, shiny_spherical_mode_name(mode));
}

/*
	CVar change callback for _sofbuddy_shiny_spherical

	Called when _sofbuddy_shiny_spherical is changed.
	This function is externally referenced in cvars.cpp.
*/
void shiny_spherical_change(cvar_t * cvar) {
	int mode = (int)cvar->value;
	if (mode < SHINY_MODE_OFF || mode > SHINY_MODE_DOUBLE) {
		PrintOut(PRINT_BAD, "lighting_blend: invalid shiny mode %d, using %d (%s)\n",
		         mode, SHINY_MODE_DEFAULT, shiny_spherical_mode_name(SHINY_MODE_DEFAULT));
		mode = SHINY_MODE_DEFAULT;
		if (detour_Cvar_Set2::oCvar_Set2) {
			detour_Cvar_Set2::oCvar_Set2(const_cast<char*>("_sofbuddy_shiny_spherical"), const_cast<char*>("1"), true);
		}
	}

	shiny_spherical_apply(mode);
}

/*
	CVar change callback for lightblend cvars
	
	Called when _sofbuddy_lightblend_src or _sofbuddy_lightblend_dst cvars are changed.
	This function is externally referenced in cvars.cpp.
*/
void lightblend_change(cvar_t * cvar) {
	if (gl_ext_multitexture && gl_ext_multitexture->value != 0.0f) {
		detour_Cvar_Set2::oCvar_Set2(const_cast<char*>("gl_ext_multitexture"), const_cast<char*>("0"), true);
		cvar_t * vid_ref = findCvar(const_cast<char*>("vid_ref"));
		if (vid_ref) vid_ref->modified = true;
		PrintOut(PRINT_DEV, "lighting_blend: Set gl_ext_multitexture to 0 and flagged vid_ref for restart.\n");
	}
	
	PrintOut(PRINT_LOG, "lighting_blend: CVar changed: %s\n", cvar->name);
	int value = cvar->value;
	bool is_src = !strcmp(cvar->name, "_sofbuddy_lightblend_src");
	
	if (!strcmp(cvar->string,"GL_ZERO")) {
		value = GL_ZERO;
	} else if (!strcmp(cvar->string,"GL_ONE")) {
		value = GL_ONE;
	} else if (!strcmp(cvar->string,"GL_SRC_COLOR")) {
		value = GL_SRC_COLOR;
	} else if (!strcmp(cvar->string,"GL_ONE_MINUS_SRC_COLOR")) {
		value = GL_ONE_MINUS_SRC_COLOR;
	} else if (!strcmp(cvar->string,"GL_DST_COLOR")) {
		value = GL_DST_COLOR;
	} else if (!strcmp(cvar->string,"GL_ONE_MINUS_DST_COLOR")) {
		value = GL_ONE_MINUS_DST_COLOR;
	} else if (!strcmp(cvar->string,"GL_SRC_ALPHA")) {
		value = GL_SRC_ALPHA;
	} else if (!strcmp(cvar->string,"GL_ONE_MINUS_SRC_ALPHA")) {
		value = GL_ONE_MINUS_SRC_ALPHA;
	} else if (!strcmp(cvar->string,"GL_DST_ALPHA")) {
		value = GL_DST_ALPHA;
	} else if (!strcmp(cvar->string,"GL_ONE_MINUS_DST_ALPHA")) {
		value = GL_ONE_MINUS_DST_ALPHA;
	} else if (!strcmp(cvar->string,"GL_CONSTANT_COLOR")) {
		value = GL_CONSTANT_COLOR;
	} else if (!strcmp(cvar->string,"GL_ONE_MINUS_CONSTANT_COLOR")) {
		value = GL_ONE_MINUS_CONSTANT_COLOR;
	} else if (!strcmp(cvar->string,"GL_CONSTANT_ALPHA")) {
		value = GL_CONSTANT_ALPHA;
	} else if (!strcmp(cvar->string,"GL_ONE_MINUS_CONSTANT_ALPHA")) {
		value = GL_ONE_MINUS_CONSTANT_ALPHA;
	} else if (!strcmp(cvar->string,"GL_SRC_ALPHA_SATURATE")) {
		value = GL_SRC_ALPHA_SATURATE;
	} else {
		PrintOut(PRINT_BAD, "Bad lightblend value: %s. Valid values:\n"
		                     "GL_ZERO, GL_ONE, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR, GL_DST_COLOR, "
		                     "GL_ONE_MINUS_DST_COLOR, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA, "
		                     "GL_ONE_MINUS_DST_ALPHA, GL_CONSTANT_COLOR, GL_ONE_MINUS_CONSTANT_COLOR, "
		                     "GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA, GL_SRC_ALPHA_SATURATE\n",
		                     cvar->string);
		
		if (is_src) {
			PrintOut(PRINT_DEV, "Defaulting to src:GL_ZERO\n");
			value = GL_ZERO;
		} else {
			PrintOut(PRINT_DEV, "Defaulting to dst:GL_SRC_COLOR\n");
			value = GL_SRC_COLOR;
		}
	}
	
	// Update the appropriate blend mode
	if (is_src) {
		lightblend_src = value;
	} else {
		lightblend_dst = value;
	}
}

bool is_blending = false;

// r_blendlightmaps_pre_callback and r_blendlightmaps_post_callback implementations moved to hooks/ directory


/*
  This only gets called when gl_ext_multitexture is 0.
*/
// src = Incoming pixel Data (Light Data)
// dest = Pixel Data in color Buffer (Texture Data)
void __stdcall glBlendFunc_R_BlendLightmaps(unsigned int sfactor,unsigned int dfactor)
{

	// Multiply Blend Mode.
	// real_glBlendFunc(GL_DST_COLOR,GL_ONE_MINUS_SRC_ALPHA);
	// Default
	// real_glBlendFunc(GL_ZERO,GL_SRC_COLOR);
	if ( is_blending ) { 
		if ( _sofbuddy_lighting_overbright->value == 1.0f ) {
			
			real_glBlendFunc(GL_DST_COLOR,GL_SRC_COLOR);
			
		} else {
			real_glBlendFunc(lightblend_src,lightblend_dst);
		}
	}
	else {
		real_glBlendFunc(sfactor,dfactor);
	}
}


void __cdecl hkAdjustTexCoords(float* player_pos, float* vertex, float* in_normal, float* in_tangent, float* in_bitangent, float* out_new_s_t)
{
	// The engine passes world-space vertices to the non-spherical shiny path.
	// Do not convert them with the camera basis here: that made "No Spherical"
	// still depend on view angle instead of only on the camera position.
	oAdjustTexCoords(player_pos, vertex, in_normal, in_tangent, in_bitangent, out_new_s_t);
	glTexCoord2f(out_new_s_t[0], out_new_s_t[1]);
}

#endif // FEATURE_LIGHTING_BLEND
