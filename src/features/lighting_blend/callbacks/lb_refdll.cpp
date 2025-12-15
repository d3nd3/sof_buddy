#include "feature_config.h"

#if FEATURE_LIGHTING_BLEND

#include "sof_compat.h"
#include "util.h"
#include "../shared.h"

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

	real_glBlendFunc = (void(__stdcall *)(unsigned int, unsigned int))*(int*)rvaToAbsRef((void*)0x000A426C);
	//Hook glBlendFunc - CheckComplexStates @call    glBlendFunc
	WriteE8Call(rvaToAbsRef((void*)0x0001B9A4), (void*)&glBlendFunc_R_BlendLightmaps);
	WriteByte(rvaToAbsRef((void*)0x0001B9A9), 0x90);

	//Hook glBlendFunc- CheckComplexStates @call    glBlendFunc
	WriteE8Call(rvaToAbsRef((void*)0x0001B690), (void*)&glBlendFunc_R_BlendLightmaps);
	WriteByte(rvaToAbsRef((void*)0x0001B695), 0x90);
	
	// Nop:
	//mov     target_blend_sfactor, ebp
	//mov     target_blend_dfactor, eax
	WriteNops(rvaToAbsRef((void*)0x00015584), 11);


	//Hook Builds_S_T_Shiny - R_DrawDetailChains @call    Builds_S_T_Shiny
	oAdjustTexCoords = (void(__cdecl *)(float*, float*, float*, float*, float*, float*))rvaToAbsRef((void*)0x00014C30);
	WriteE8Call(rvaToAbsRef((void*)0x00014995), (void*)&hkAdjustTexCoords);

	glTexCoord2f = (void(__stdcall *)(float, float))*(int*)rvaToAbsRef((void*)0x000A42F4);
	glVertex3f = (void(__stdcall *)(float, float, float))*(int*)rvaToAbsRef((void*)0x000A47C4);
	glBegin = (void(__stdcall *)(unsigned int))*(int*)rvaToAbsRef((void*)0x000A4710);
	glEnd = (void(__stdcall *)(void))*(int*)rvaToAbsRef((void*)0x000A45F4);
	glColor3f = (void(__stdcall *)(float, float, float))*(int*)rvaToAbsRef((void*)0x000A4490);
	

	//We will call gltexCoord2f instead.
	//Disable glTexCoord2f by NOP ... R_DrawDetailChains @call    glTexCoord2f
	void* glTexCoord2f_call_addr = rvaToAbsRef((void*)0x000149AC);
	PrintOut(PRINT_LOG, "lighting_blend: glTexCoord2f call address: 0x%p\n", glTexCoord2f_call_addr);
	WriteNops(glTexCoord2f_call_addr, 3);
	// Clean up stack: add esp, 8 (removes 2 float parameters from stack)
	// 0x83 0xC4 0x08 = add esp, 8
	WriteByte((char*)glTexCoord2f_call_addr + 3, 0x83);
	WriteByte((char*)glTexCoord2f_call_addr + 4, 0xC4);
	WriteByte((char*)glTexCoord2f_call_addr + 5, 0x08);

	PrintOut(PRINT_LOG, "lighting_blend: Applied blend modes (src=0x%X, dst=0x%X)\n", 
	         lightblend_src, lightblend_dst);
}

#endif // FEATURE_LIGHTING_BLEND

