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
float *cam_vforward = nullptr;
float *cam_vup = nullptr;
float *cam_vright = nullptr;
void **ref_surfaces = nullptr;
void lightblend_RefDllLoaded(char const* name)
{
	PrintOut(PRINT_LOG, "lighting_blend: Initializing blend mode settings\n");

	lightblend_target_src = (int*)rvaToAbsRef((void*)0x000A4610);
	lightblend_target_dst = (int*)rvaToAbsRef((void*)0x000A43FC);
	lighting_cutoff_target = (float*)rvaToAbsRef((void*)0x2C368);
	water_size_double_target = (double*)rvaToAbsRef((void*)0x2C390);
	water_size_float_target = (float*)rvaToAbsRef((void*)0x2C398);
	shiny_spherical_target1 = (unsigned char*)rvaToAbsRef((void*)0x14814);
	shiny_spherical_target3 = (unsigned char*)rvaToAbsRef((void*)0x14A66);
	
	cam_vforward = (float*)rvaToAbsRef((void*)0x0008FD7C);
	cam_vup = (float*)rvaToAbsRef((void*)0x0008FD94);
	cam_vright = (float*)rvaToAbsRef((void*)0x0008FF30);
	ref_surfaces = (void**)rvaToAbsRef((void*)0xA1728);

	if (lighting_cutoff_target && _sofbuddy_lighting_cutoff) {
		writeFloatAt(lighting_cutoff_target, _sofbuddy_lighting_cutoff->value);
	}
	
	if (water_size_double_target && water_size_float_target && _sofbuddy_water_size && _sofbuddy_water_size->value >= 16.0f) {
		writeDoubleAt(water_size_double_target, (double)_sofbuddy_water_size->value);
		writeFloatAt(water_size_float_target, 1.0f / _sofbuddy_water_size->value);
	}

	if (shiny_spherical_target1 && _sofbuddy_shiny_spherical) {
		unsigned char value = _sofbuddy_shiny_spherical->value == 0.0f ? 0xEB : 0x74;
		WriteByte(shiny_spherical_target1, value);
	}

    if (shiny_spherical_target3 && _sofbuddy_shiny_spherical) {
        unsigned char value = _sofbuddy_shiny_spherical->value == 0.0f ? 0xEB : 0x74;
        WriteByte(shiny_spherical_target3, value);
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
