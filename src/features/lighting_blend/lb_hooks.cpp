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
	CVar change callback for _sofbuddy_shiny_spherical
	
	Called when _sofbuddy_shiny_spherical is changed.
	Writes byte to ref_gl address 0x14814.
	This function is externally referenced in cvars.cpp.
*/
void shiny_spherical_change(cvar_t * cvar) {
	if (shiny_spherical_target1) {
		unsigned char value = cvar->value == 0.0f ? 0xEB : 0x74;
		WriteByte(shiny_spherical_target1, value);
		PrintOut(PRINT_LOG, "lighting_blend: Set shiny_spherical to %d (wrote 0x%02X to 0x14814)\n", (int)cvar->value, value);
	}

    if (shiny_spherical_target3) {
        unsigned char value = cvar->value == 0.0f ? 0xEB : 0x74;
        WriteByte(shiny_spherical_target3, value);
        PrintOut(PRINT_LOG, "lighting_blend: Set shiny_spherical to %d (wrote 0x%02X to 0x14A66)\n", (int)cvar->value, value);
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


static void __cdecl hkAdjustTexCoordsImpl(float* player_pos, float* vertex, float* in_normal, float* in_tangent, float* in_bitangent, float* out_new_s_t, void* caller_stack);

void __cdecl hkAdjustTexCoords(float* player_pos, float* vertex, float* in_normal, float* in_tangent, float* in_bitangent, float* out_new_s_t)
{
    void* esp_entry = __builtin_frame_address(0);
    hkAdjustTexCoordsImpl(player_pos, vertex, in_normal, in_tangent, in_bitangent, out_new_s_t, esp_entry);

}


static void __cdecl hkAdjustTexCoordsImpl(float* player_pos, float* vertex, float* in_normal, float* in_tangent, float* in_bitangent, float* out_new_s_t, void* caller_stack)
{
	unsigned int surf_index = *(unsigned int*)((char*)caller_stack + 0xD8 - 0xA4);

	// Hot path: ref_surfaces is refreshed on each ref.dll load (so it's safe across vid_restart).
	if (!ref_surfaces) {
		// Fail safe: behave like the original call site as best as we can.
		oAdjustTexCoords(player_pos, vertex, in_normal, in_tangent, in_bitangent, out_new_s_t);
		glTexCoord2f(out_new_s_t[0], out_new_s_t[1]);
		return;
	}

	void** surface_ptr = ref_surfaces + surf_index;
	void* cplane_ptr = (void*)((char*)(*surface_ptr) + 4);
	float* surface_normal = *(float**)cplane_ptr;
	float normal_x = surface_normal[0];
	float normal_y = surface_normal[1];
	float normal_z = surface_normal[2];
	
	void ** mtexinfo_ptr = (void**)((char*)(*surface_ptr) + 0x34);
	int flags = *(int*)((char*)(*mtexinfo_ptr) + 0x20);

	
	// Visualize normal direction once per surface using glVertex3f calls inside GL_TRIANGLE_FAN
	static unsigned int last_drawn_surf_index = 0xFFFFFFFF;
	static int vertex_count_per_surface = 0;
	if (surf_index != last_drawn_surf_index) {
		last_drawn_surf_index = surf_index;
		vertex_count_per_surface = 0;
	}
#if 0
	// Draw on every 3rd vertex to ensure visibility (not just first vertex which might be culled)
	if (vertex_count_per_surface % 3 == 0) {
		// Validate normal vector before drawing
		float normal_mag_sq = in_normal[0] * in_normal[0] + in_normal[1] * in_normal[1] + in_normal[2] * in_normal[2];
		bool is_zero = (normal_mag_sq < 1.0e-10f);
		bool has_nan = (in_normal[0] != in_normal[0] || in_normal[1] != in_normal[1] || in_normal[2] != in_normal[2]);
		bool has_inf = (normal_mag_sq > 1.0e10f);
		bool is_too_small = (normal_mag_sq > 0.0f && normal_mag_sq < 0.5f);
		bool is_too_large = (normal_mag_sq > 2.0f);
		
		if (is_zero || has_nan || has_inf || is_too_small || is_too_large) {
			PrintOut(PRINT_LOG, "DEBUG: Invalid normal detected for surface %u: in_normal = (%.6f, %.6f, %.6f), mag_sq = %.6f, zero=%d nan=%d inf=%d too_small=%d too_large=%d\n",
			         surf_index, in_normal[0], in_normal[1], in_normal[2], normal_mag_sq, is_zero, has_nan, has_inf, is_too_small, is_too_large);
		} else {
			const float normal_scale = 100.0f;
			float start_x = vertex[0];
			float start_y = vertex[1];
			float start_z = vertex[2];
			float end_x = start_x + in_normal[0] * normal_scale;
			float end_y = start_y + in_normal[1] * normal_scale;
			float end_z = start_z + in_normal[2] * normal_scale;
			
			// Draw multiple green vertices at the start to establish the color strongly
			glColor3f(0.0f, 1.0f, 0.0f);
			for (int j = 0; j < 5; j++) {
				glVertex3f(start_x, start_y, start_z);
			}
			
			// Draw multiple points along the normal with color interpolation from green to red
			const int num_points = 30;
			for (int i = 1; i <= num_points; i++) {
				float t = (float)i / (float)num_points;
				float r = t;  // Red increases from 0 to 1
				float g = 1.0f - t;  // Green decreases from 1 to 0
				glColor3f(r, g, 0.0f);
				glVertex3f(
					start_x + (end_x - start_x) * t,
					start_y + (end_y - start_y) * t,
					start_z + (end_z - start_z) * t
				);
			}
			
			// Draw arrowhead at the end (larger and red)
			const float arrowhead_size = 10.0f;
			glColor3f(1.0f, 0.0f, 0.0f);
			glVertex3f(end_x, end_y, end_z);
			glVertex3f(end_x - in_normal[0] * arrowhead_size + in_normal[1] * arrowhead_size * 0.3f, 
			           end_y - in_normal[1] * arrowhead_size + in_normal[2] * arrowhead_size * 0.3f, 
			           end_z - in_normal[2] * arrowhead_size - in_normal[0] * arrowhead_size * 0.3f);
			glVertex3f(end_x, end_y, end_z);
			glVertex3f(end_x - in_normal[0] * arrowhead_size - in_normal[1] * arrowhead_size * 0.3f, 
			           end_y - in_normal[1] * arrowhead_size - in_normal[2] * arrowhead_size * 0.3f, 
			           end_z - in_normal[2] * arrowhead_size + in_normal[0] * arrowhead_size * 0.3f);
			
			glColor3f(1.0f, 1.0f, 1.0f);
		}
	}
	#endif
	vertex_count_per_surface++;
	
	

	// Override input parameters with randomly selected basis
	// in_normal[0] = override_normal[0];
	// in_normal[1] = override_normal[1];
	// in_normal[2] = override_normal[2];
	
	// in_tangent[0] = override_tangent[0];
	// in_tangent[1] = override_tangent[1];
	// in_tangent[2] = override_tangent[2];
	
	// in_bitangent[0] = override_bitangent[0];
	// in_bitangent[1] = override_bitangent[1];
	// in_bitangent[2] = override_bitangent[2];
	float vertex_world[3];
	vertex_world[0] = vertex[0];
	vertex_world[1] = vertex[1];
	vertex_world[2] = vertex[2];
	if (flags & 0x00020000) {
		// Convert vertex from eye space to world space
		if (cam_vforward && cam_vup && cam_vright) {
			// PrintOut(PRINT_LOG, "DEBUG: cam_vforward = (%.6f, %.6f, %.6f)\n", cam_vforward[0], cam_vforward[1], cam_vforward[2]);
			// Quake-style eye space: z is forward-negative in view matrix
			vertex_world[0] = player_pos[0] + vertex[0] * cam_vright[0] + vertex[1] * cam_vup[0] - vertex[2] * cam_vforward[0];
			vertex_world[1] = player_pos[1] + vertex[0] * cam_vright[1] + vertex[1] * cam_vup[1] - vertex[2] * cam_vforward[1];
			vertex_world[2] = player_pos[2] + vertex[0] * cam_vright[2] + vertex[1] * cam_vup[2] - vertex[2] * cam_vforward[2];
		} else {
			vertex_world[0] = vertex[0];
			vertex_world[1] = vertex[1];
			vertex_world[2] = vertex[2];
		}
		
		// 	PrintOut(PRINT_LOG, "DEBUG: Env texture\n");
	// 	PrintOut(PRINT_LOG, "DEBUG: player_pos = (%.6f, %.6f, %.6f)\n", player_pos[0], player_pos[1], player_pos[2]);
	PrintOut(PRINT_LOG, "DEBUG: vertex = (%.6f, %.6f, %.6f)\n", vertex_world[0], vertex_world[1], vertex_world[2]);
	// PrintOut(PRINT_LOG, "DEBUG: in_normal = (%.6f, %.6f, %.6f)\n", in_normal[0], in_normal[1], in_normal[2]);
	// PrintOut(PRINT_LOG, "DEBUG: in_tangent = (%.6f, %.6f, %.6f)\n", in_tangent[0], in_tangent[1], in_tangent[2]);
	// PrintOut(PRINT_LOG, "DEBUG: in_bitangent = (%.6f, %.6f, %.6f)\n", in_bitangent[0], in_bitangent[1], in_bitangent[2]);

		#if 0
		// Env texture: Generate valid shiny texture coordinates
		// Calculate view vector from vertex to player
		float view_vec_x = player_pos[0] - vertex_world[0];
		float view_vec_y = player_pos[1] - vertex_world[1];
		float view_vec_z = player_pos[2] - vertex_world[2];
		
		// PrintOut(PRINT_LOG, "DEBUG: view_vec (before normalize) = (%.6f, %.6f, %.6f)\n", view_vec_x, view_vec_y, view_vec_z);
		
		// Calculate distance and normalize
		float distance = sqrtf(view_vec_x * view_vec_x + view_vec_y * view_vec_y + view_vec_z * view_vec_z);
		// PrintOut(PRINT_LOG, "DEBUG: distance = %.6f\n", distance);
		
		const float ZERO_VECTOR_EPSILON = 1.0e-10f;
		if (distance >= ZERO_VECTOR_EPSILON) {
			float inv_dist = 1.0f / distance;
			view_vec_x *= inv_dist;
			view_vec_y *= inv_dist;
			view_vec_z *= inv_dist;
		}
		
		// PrintOut(PRINT_LOG, "DEBUG: view_vec (after normalize) = (%.6f, %.6f, %.6f)\n", view_vec_x, view_vec_y, view_vec_z);
		
		// Project normalized view vector onto in_normal basis vector
		float dot_s = view_vec_x * in_normal[0] + view_vec_y * in_normal[1] + view_vec_z * in_normal[2];
		// PrintOut(PRINT_LOG, "DEBUG: dot_s = %.6f\n", dot_s);
		
		// Calculate rejection vector (perpendicular to in_normal)
		float rejection_x = view_vec_x - in_normal[0] * dot_s;
		float rejection_y = view_vec_y - in_normal[1] * dot_s;
		float rejection_z = view_vec_z - in_normal[2] * dot_s;
		
		// PrintOut(PRINT_LOG, "DEBUG: rejection = (%.6f, %.6f, %.6f)\n", rejection_x, rejection_y, rejection_z);
		
		// Project rejection onto in_tangent basis vector
		float dot_t = rejection_x * in_tangent[0] + rejection_y * in_tangent[1] + rejection_z * in_tangent[2];
		// PrintOut(PRINT_LOG, "DEBUG: dot_t = %.6f\n", dot_t);
		
		// Project rejection onto in_bitangent basis vector
		float dot_tex_s = rejection_x * in_bitangent[0] + rejection_y * in_bitangent[1] + rejection_z * in_bitangent[2];
		// PrintOut(PRINT_LOG, "DEBUG: dot_tex_s = %.6f\n", dot_tex_s);
		
		// Remap from [-1, 1] to [0, 1]
		const float ADD_CONSTANT = 1.0f;
		const float SCALE_CONSTANT = 0.5f;

		// PrintOut(PRINT_LOG, "before new_s_t is : %f, %f\n", out_new_s_t[0], out_new_s_t[1]);
		// out_new_s_t[0] = 1.0f - (dot_t + ADD_CONSTANT) * SCALE_CONSTANT;
		// out_new_s_t[1] = 1.0f - (dot_tex_s + ADD_CONSTANT) * SCALE_CONSTANT;
		out_new_s_t[0] = (dot_t + ADD_CONSTANT) * SCALE_CONSTANT;
		out_new_s_t[1] = (dot_tex_s + ADD_CONSTANT) * SCALE_CONSTANT;
		// PrintOut(PRINT_LOG, "after new_s_t is : %f, %f\n", out_new_s_t[0], out_new_s_t[1]);
		#endif
	}
	
	// Call original function with (potentially corrected) texture coordinates
	// PrintOut(PRINT_LOG, "DEBUG: before calling oAdjustTexCoords, out_new_s_t = (%.6f, %.6f)\n", out_new_s_t[0], out_new_s_t[1]);
	oAdjustTexCoords(player_pos, vertex_world, in_normal, in_tangent, in_bitangent, out_new_s_t);
	// PrintOut(PRINT_LOG, "DEBUG: after calling oAdjustTexCoords, out_new_s_t = (%.6f, %.6f)\n", out_new_s_t[0], out_new_s_t[1]);

	
    // Setup glTexCoord2f function pointer from dereference of 0xA42F4
    glTexCoord2f(out_new_s_t[0], out_new_s_t[1]);
	// glTexCoord2f(out_new_s_t[1], out_new_s_t[0] );

}

#endif // FEATURE_LIGHTING_BLEND
