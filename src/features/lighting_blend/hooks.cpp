/*
	Lighting Blend Tweak
	
	This feature allows customization of OpenGL blend modes used for lighting
	in the ref.dll renderer. It patches memory locations in ref.dll that store
	the source and destination blend factors for glBlendFunc().

	*IMPORTANT* REQUIRES: gl_ext_multitexture 0
*/

#include "feature_config.h"

#if FEATURE_LIGHTING_BLEND

#include "feature_macro.h"
#include "shared_hook_manager.h"
#include "sof_compat.h"
#include "util.h"
#include <windows.h>
#include <string.h>

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
static int lightblend_src = GL_ZERO;
static int lightblend_dst = GL_SRC_COLOR;
static int *lightblend_target_src = nullptr;
static int *lightblend_target_dst = nullptr;
static float *lighting_cutoff_target = nullptr;
static double *water_size_double_target = nullptr;
static float *water_size_float_target = nullptr;

// Forward declarations
static void lightblend_ApplySettings();
static void lightblend_PostCvarInit();
static void lightblend_RefDllLoaded();
void create_lightingblend_cvars(void);

// Function pointer declarations
static void (__stdcall *real_glBlendFunc)(unsigned int, unsigned int) = nullptr;

// Function declarations
void __stdcall glBlendFunc_R_BlendLightmaps(unsigned int sfactor, unsigned int dfactor);
void lightblend_change(cvar_t * cvar);
void lighting_overbright_change(cvar_t * cvar);
void lighting_cutoff_change(cvar_t * cvar);
void water_size_change(cvar_t * cvar);

// CVar extern declarations
extern cvar_t * _sofbuddy_lighting_overbright;
extern cvar_t * _sofbuddy_lighting_cutoff;
extern cvar_t * _sofbuddy_water_size;
extern cvar_t * _sofbuddy_lightblend_src;
extern cvar_t * _sofbuddy_lightblend_dst;
extern cvar_t * gl_ext_multitexture;

// Hook registrations placed after function declarations for visibility
REGISTER_SHARED_HOOK_CALLBACK(PostCvarInit, lighting_blend, lightblend_PostCvarInit, 50, Post);
REGISTER_SHARED_HOOK_CALLBACK(RefDllLoaded, lighting_blend, lightblend_RefDllLoaded, 60, Post);
REGISTER_HOOK_VOID(R_BlendLightmaps, (void*)0x00015440, RefDll, void, __cdecl);

/*
	PostCvarInit Callback
	
	Called after CVars are available. This is where we register our custom CVars.
*/
static void lightblend_PostCvarInit()
{
	PrintOut(PRINT_LOG, "lighting_blend: Registering CVars\n");
	create_lightingblend_cvars();
}

// Hook registrations moved to top of file


/*
	RefDllLoaded Callback
	
	Called when ref.dll is loaded. This is where we apply the initial blend settings
	to ref.dll memory.
*/
static void lightblend_RefDllLoaded()
{
	PrintOut(PRINT_LOG, "lighting_blend: Initializing blend mode settings\n");

	lightblend_target_src = (int*)rvaToAbsRef((void*)0x000A4610);
	lightblend_target_dst = (int*)rvaToAbsRef((void*)0x000A43FC);
	lighting_cutoff_target = (float*)rvaToAbsRef((void*)0x2C368);
	water_size_double_target = (double*)rvaToAbsRef((void*)0x2C390);
	water_size_float_target = (float*)rvaToAbsRef((void*)0x2C398);
	
	if (lighting_cutoff_target && _sofbuddy_lighting_cutoff) {
		writeFloatAt(lighting_cutoff_target, _sofbuddy_lighting_cutoff->value);
	}
	
	if (water_size_double_target && water_size_float_target && _sofbuddy_water_size && _sofbuddy_water_size->value >= 16.0f) {
		writeDoubleAt(water_size_double_target, (double)_sofbuddy_water_size->value);
		writeFloatAt(water_size_float_target, 1.0f / _sofbuddy_water_size->value);
	}

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
	

	real_glBlendFunc = (void(__stdcall *)(unsigned int, unsigned int))*(int*)rvaToAbsRef((void*)0x000A426C);
	WriteE8Call(rvaToAbsRef((void*)0x0001B9A4), (void*)&glBlendFunc_R_BlendLightmaps);
	WriteByte(rvaToAbsRef((void*)0x0001B9A9), 0x90);
	WriteE8Call(rvaToAbsRef((void*)0x0001B690), (void*)&glBlendFunc_R_BlendLightmaps);
	WriteByte(rvaToAbsRef((void*)0x0001B695), 0x90);

	WriteByte(rvaToAbsRef((void*)0x00015584), 0x90);
	WriteByte(rvaToAbsRef((void*)0x00015585), 0x90);
	WriteByte(rvaToAbsRef((void*)0x00015586), 0x90);
	WriteByte(rvaToAbsRef((void*)0x00015587), 0x90);
	WriteByte(rvaToAbsRef((void*)0x00015588), 0x90);
	WriteByte(rvaToAbsRef((void*)0x00015589), 0x90);
	WriteByte(rvaToAbsRef((void*)0x0001558A), 0x90);
	WriteByte(rvaToAbsRef((void*)0x0001558B), 0x90);
	WriteByte(rvaToAbsRef((void*)0x0001558C), 0x90);
	WriteByte(rvaToAbsRef((void*)0x0001558D), 0x90);
	WriteByte(rvaToAbsRef((void*)0x0001558E), 0x90);

	PrintOut(PRINT_LOG, "lighting_blend: Applied blend modes (src=0x%X, dst=0x%X)\n", 
	         lightblend_src, lightblend_dst);
}

// Hook registrations moved to top of file


/*
	CVar change callback for _sofbuddy_lighting_overbright
	
	Called when _sofbuddy_lighting_overbright is changed.
	This function is externally referenced in cvars.cpp.
*/
void lighting_overbright_change(cvar_t * cvar) {
	if (gl_ext_multitexture && gl_ext_multitexture->value != 0.0f) {
		orig_Cvar_Set2(const_cast<char*>("gl_ext_multitexture"), const_cast<char*>("0"), true);
		cvar_t * vid_ref = findCvar(const_cast<char*>("vid_ref"));
		if (vid_ref) vid_ref->modified = true;
		PrintOut(PRINT_GOOD, "lighting_blend: Set gl_ext_multitexture to 0 and flagged vid_ref for restart.\n");
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
			orig_Cvar_Set2(const_cast<char*>("_sofbuddy_water_size"), const_cast<char*>("16"), true);
			return;
		}
		if (cvar->value < 16.0f) {
			PrintOut(PRINT_BAD, "lighting_blend: water_size must be at least 16 (got %f)\n", cvar->value);
			orig_Cvar_Set2(const_cast<char*>("_sofbuddy_water_size"), const_cast<char*>("16"), true);
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
	CVar change callback for lightblend cvars
	
	Called when _sofbuddy_lightblend_src or _sofbuddy_lightblend_dst cvars are changed.
	This function is externally referenced in cvars.cpp.
*/
void lightblend_change(cvar_t * cvar) {
	if (gl_ext_multitexture && gl_ext_multitexture->value != 0.0f) {
		orig_Cvar_Set2(const_cast<char*>("gl_ext_multitexture"), const_cast<char*>("0"), true);
		cvar_t * vid_ref = findCvar(const_cast<char*>("vid_ref"));
		if (vid_ref) vid_ref->modified = true;
		PrintOut(PRINT_GOOD, "lighting_blend: Set gl_ext_multitexture to 0 and flagged vid_ref for restart.\n");
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
			PrintOut(PRINT_GOOD, "Defaulting to src:GL_ZERO\n");
			value = GL_ZERO;
		} else {
			PrintOut(PRINT_GOOD, "Defaulting to dst:GL_SRC_COLOR\n");
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
/*
  This gets called when gl_ext_multitexture is 0.
*/
//Maybe blend is already active before calling R_BlendLightmaps (Nope, it is not.)
void hkR_BlendLightmaps(void) {
	// orig_Com_Printf("ComplexState is %i\n",((*(int*)0x300A46E0) & 0x01));

	if ( _sofbuddy_lighting_overbright->value == 1.0f ) {
		PrintOut(PRINT_LOG, "Using overbright lighting\n");
		*lightblend_target_src = GL_DST_COLOR;
		*lightblend_target_dst = GL_SRC_COLOR;
	} else {
		PrintOut(PRINT_LOG, "Using custom lighting\n");
		*lightblend_target_src = lightblend_src;
		*lightblend_target_dst = lightblend_dst;	
	}
	

	is_blending = true;
	// Calls CheckComplexStates, which calls glBlendFunc
	oR_BlendLightmaps();
	is_blending = false;
}


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

#endif // FEATURE_LIGHTING_BLEND
