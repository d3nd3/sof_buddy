#include "feature_config.h"

#if FEATURE_LIGHTING_BLEND

#include "sof_compat.h"
#include "generated_detours.h"
#include "util.h"
#include "../shared.h"
#include <windows.h>

int *lightblend_target_src = nullptr;
int *lightblend_target_dst = nullptr;
float *lighting_cutoff_target = nullptr;
double *water_size_double_target = nullptr;
float *water_size_float_target = nullptr;
unsigned char *shiny_spherical_target1 = nullptr;
unsigned char *shiny_spherical_target3 = nullptr;
unsigned char *shiny_pass_target = nullptr;
float *shiny_flat_threshold_target = nullptr;
void lightblend_RefDllLoaded(char const* name)
{
	PrintOut(PRINT_LOG, "lighting_blend: Initializing blend mode settings\n");

	lighting_reset_runtime_state();

	lightblend_target_src = (int*)rvaToAbsRef((void*)0x000A4610);
	lightblend_target_dst = (int*)rvaToAbsRef((void*)0x000A43FC);
	lighting_cutoff_target = (float*)rvaToAbsRef((void*)0x2C368);
	water_size_double_target = (double*)rvaToAbsRef((void*)0x2C390);
	water_size_float_target = (float*)rvaToAbsRef((void*)0x2C398);
	shiny_spherical_target1 = (unsigned char*)rvaToAbsRef((void*)0x14814);
	shiny_spherical_target3 = (unsigned char*)rvaToAbsRef((void*)0x14A66);
	shiny_pass_target = (unsigned char*)rvaToAbsRef((void*)0x147DF);
	shiny_flat_threshold_target = (float*)rvaToAbsRef((void*)0x2C434);

	lighting_gamma_table = (unsigned char*)rvaToAbsRef((void*)0x0004C520);
	gl_state_lightmap_textures_ptr = (int*)rvaToAbsRef((void*)0x0008FFD0);
	lightmap_chains_ptr = (int*)rvaToAbsRef((void*)0x00090028);
	r_worldmodel_ptr = (void**)rvaToAbsRef((void*)0x0008FC6C);
	rdflags_ptr = (unsigned char*)rvaToAbsRef((void*)0x0008FD3C);
	lighting_lightstyles_ptr = (float**)rvaToAbsRef((void*)0x0008FD44);
	lighting_set_cache_state = (unsigned char (__cdecl *)(int))rvaToAbsRef((void*)0x00009510);
	gl_mtex_coord_ptr = (void**)rvaToAbsRef((void*)0x000A45EC);
	oCheckComplexStates = (void(__cdecl*)(void))rvaToAbsRef((void*)0x0001B650);
	gl_select_texture_sgis = (void(__stdcall**)(unsigned int))rvaToAbsRef((void*)0x000A458C);
	engine_active_texture = (void(__cdecl*)(unsigned int))rvaToAbsRef((void*)0x00006130);

	/*
		Patch only the two CheckComplexStates calls and the glEnd call inside
		GL_RenderLightmappedPoly.  The wrappers add/remove the x2 lightmap
		combiner around the draw; they are no-ops unless the test cvar is on.
	*/
	auto call_target = [](unsigned char* site) -> void* {
		if (!site || site[0] != 0xE8) return nullptr;
		const int rel = *reinterpret_cast<int*>(site + 1);
		return site + 5 + rel;
	};
	unsigned char* site1 = reinterpret_cast<unsigned char*>(rvaToAbsRef((void*)0x00015C53));
	unsigned char* site2 = reinterpret_cast<unsigned char*>(rvaToAbsRef((void*)0x00015CC6));
	unsigned char* site3 = reinterpret_cast<unsigned char*>(rvaToAbsRef((void*)0x00015D18));

	if (call_target(site1) == reinterpret_cast<void*>(&lighting_check_complex_states_for_lightmap)) {
		; // already patched (renderer reloaded without resetting code)
	} else if (site1 && site1[0] == 0xE8) {
		WriteE8Call(site1, reinterpret_cast<void*>(&lighting_check_complex_states_for_lightmap));
	} else {
		PrintOut(PRINT_BAD, "lighting_blend: raven combiner patch 1 skipped (unexpected opcodes)\n");
	}
	if (call_target(site2) == reinterpret_cast<void*>(&lighting_check_complex_states_for_lightmap)) {
		;
	} else if (site2 && site2[0] == 0xE8) {
		WriteE8Call(site2, reinterpret_cast<void*>(&lighting_check_complex_states_for_lightmap));
	} else {
		PrintOut(PRINT_BAD, "lighting_blend: raven combiner patch 2 skipped (unexpected opcodes)\n");
	}
	if (call_target(site3) == reinterpret_cast<void*>(&lighting_glend_for_lightmap)) {
		;
	} else if (site3 && site3[0] == 0xFF && site3[1] == 0x15) {
		WriteE8Call(site3, reinterpret_cast<void*>(&lighting_glend_for_lightmap));
		WriteByte(site3 + 5, 0x90);
	} else {
		PrintOut(PRINT_BAD, "lighting_blend: raven combiner glEnd patch skipped (unexpected opcodes)\n");
	}

	// R_Init() has already built the gamma table by the time RefDllLoaded runs.
	// Sync the preset first so the LUT is built for the active style before the
	// patched R_BuildLightMap starts using it.
	lighting_apply_effective_settings(false);
	// The config may have restored the value we forced in a previous session;
	// apply it to the renderer that R_Init just created. This only restarts
	// when the effective value actually differs.
	lighting_sync_multitexture(true);
	if (!lighting_patch_lut_reads()) {
		PrintOut(PRINT_BAD, "lighting_blend: per-channel lightmap curve unavailable for this ref_gl build (preserve modes can still work via the R_BuildLightMap hook)\n");
	}
	if (r_worldmodel_ptr && *r_worldmodel_ptr) {
		lighting_request_lightmap_rebuild();
	}

	if (lighting_cutoff_target && _sofbuddy_lighting_cutoff) {
		writeFloatAt(lighting_cutoff_target, _sofbuddy_lighting_cutoff->value);
	}
	
	if (water_size_double_target && water_size_float_target && _sofbuddy_water_size && _sofbuddy_water_size->value >= 16.0f) {
		writeDoubleAt(water_size_double_target, (double)_sofbuddy_water_size->value);
		writeFloatAt(water_size_float_target, 1.0f / _sofbuddy_water_size->value);
	}

	if (_sofbuddy_shiny_spherical) {
		shiny_spherical_apply((int)_sofbuddy_shiny_spherical->value);
	}

	real_glBlendFunc = detour_glBlendFunc::oglBlendFunc;
	if (!real_glBlendFunc) {
		HMODULE hGl = GetModuleHandleA("opengl32.dll");
		if (hGl) {
			real_glBlendFunc = reinterpret_cast<void(__stdcall *)(unsigned int, unsigned int)>(
				GetProcAddress(hGl, "glBlendFunc"));
		}
	}
	//Hook glBlendFunc - CheckComplexStates @call glBlendFunc
	if (real_glBlendFunc) {
		WriteE8Call(rvaToAbsRef((void*)0x0001B9A4), (void*)&glBlendFunc_R_BlendLightmaps);
		WriteByte(rvaToAbsRef((void*)0x0001B9A9), 0x90);

		//Hook glBlendFunc- CheckComplexStates @call glBlendFunc
		WriteE8Call(rvaToAbsRef((void*)0x0001B690), (void*)&glBlendFunc_R_BlendLightmaps);
		WriteByte(rvaToAbsRef((void*)0x0001B695), 0x90);
	} else {
		PrintOut(PRINT_BAD, "lighting_blend: glBlendFunc unresolved; skipping blend hook patching\n");
	}
	
	// Nop:
	//mov     target_blend_sfactor, ebp
	//mov     target_blend_dfactor, eax
	WriteNops(rvaToAbsRef((void*)0x00015584), 11);


	//Hook Builds_S_T_Shiny - R_DrawDetailChains @call Builds_S_T_Shiny
	oAdjustTexCoords = detour_AdjustTexCoords::oAdjustTexCoords;
	if (oAdjustTexCoords) {
		WriteE8Call(rvaToAbsRef((void*)0x00014995), (void*)&hkAdjustTexCoords);
	} else {
		PrintOut(PRINT_BAD, "lighting_blend: AdjustTexCoords unresolved; skipping shiny hook patching\n");
	}

	glTexCoord2f = detour_glTexCoord2f::oglTexCoord2f;
	glVertex3f = detour_glVertex3f::oglVertex3f;
	glBegin = detour_glBegin::oglBegin;
	glEnd = detour_glEnd::oglEnd;
	glColor3f = detour_glColor3f::oglColor3f;
	if (!glTexCoord2f || !glVertex3f || !glBegin || !glEnd || !glColor3f) {
		HMODULE hGl = GetModuleHandleA("opengl32.dll");
		if (hGl) {
			if (!glTexCoord2f) glTexCoord2f = reinterpret_cast<void(__stdcall *)(float, float)>(GetProcAddress(hGl, "glTexCoord2f"));
			if (!glVertex3f) glVertex3f = reinterpret_cast<void(__stdcall *)(float, float, float)>(GetProcAddress(hGl, "glVertex3f"));
			if (!glBegin) glBegin = reinterpret_cast<void(__stdcall *)(unsigned int)>(GetProcAddress(hGl, "glBegin"));
			if (!glEnd) glEnd = reinterpret_cast<void(__stdcall *)(void)>(GetProcAddress(hGl, "glEnd"));
			if (!glColor3f) glColor3f = reinterpret_cast<void(__stdcall *)(float, float, float)>(GetProcAddress(hGl, "glColor3f"));
		}
	}
	

	// We will call glTexCoord2f directly instead.
	// Disable inline glTexCoord2f call in R_DrawDetailChains.
	if (glTexCoord2f) {
		void* glTexCoord2f_call_addr = rvaToAbsRef((void*)0x000149AC);
		PrintOut(PRINT_LOG, "lighting_blend: glTexCoord2f call address: 0x%p\n", glTexCoord2f_call_addr);
		WriteNops(glTexCoord2f_call_addr, 3);
		// Clean up stack: add esp, 8 (removes 2 float parameters from stack)
		WriteByte((char*)glTexCoord2f_call_addr + 3, 0x83);
		WriteByte((char*)glTexCoord2f_call_addr + 4, 0xC4);
		WriteByte((char*)glTexCoord2f_call_addr + 5, 0x08);
	}

	PrintOut(PRINT_LOG, "lighting_blend: Applied blend modes (src=0x%X, dst=0x%X)\n", 
	         lightblend_src, lightblend_dst);
}

#endif // FEATURE_LIGHTING_BLEND
