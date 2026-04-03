#include "feature_config.h"

#if FEATURE_TEXTURE_MAPPING_MIN_MAG

#include "sof_compat.h"
#include "generated_detours.h"
#include "util.h"
#include <string.h>
#include <windows.h>
#include "../shared.h"
#include "../../vsync_toggle/shared.h"

void setup_minmag_filters(char const* name) {
	static bool first_run = true;

	_gl_texturemode = detour_Cvar_Get::oCvar_Get("gl_texturemode","GL_LINEAR_MIPMAP_LINEAR",CVAR_SOFBUDDY_ARCHIVE,NULL);

	orig_glTexParameterf = detour_glTexParameterf::oglTexParameterf;
	if (!orig_glTexParameterf) {
		HMODULE hGl = GetModuleHandleA("opengl32.dll");
		if (hGl) {
			orig_glTexParameterf = reinterpret_cast<glTexParameterf_t>(
				GetProcAddress(hGl, "glTexParameterf"));
		}
	}
	if (!orig_glTexParameterf) {
		PrintOut(PRINT_BAD, "texture_mapping_min_mag: glTexParameterf unresolved; skipping hook patching\n");
		return;
	}

	WriteE8Call(rvaToAbsRef((void*)0x00006636), (void*)&orig_glTexParameterf_min_mipped);
	WriteByte(rvaToAbsRef((void*)0x0000663B), 0x90);
	WriteE8Call(rvaToAbsRef((void*)0x00006660), (void*)&orig_glTexParameterf_mag_mipped);
	WriteByte(rvaToAbsRef((void*)0x00006665), 0x90);
	WriteE8Call(rvaToAbsRef((void*)0x000065DF), (void*)&orig_glTexParameterf_min_unmipped);
	WriteByte(rvaToAbsRef((void*)0x000065E4), 0x90);
	WriteE8Call(rvaToAbsRef((void*)0x00006609), (void*)&orig_glTexParameterf_mag_unmipped);
	WriteByte(rvaToAbsRef((void*)0x0000660E), 0x90);
	WriteE8Call(rvaToAbsRef((void*)0x0000659C), (void*)&orig_glTexParameterf_min_ui);
	WriteByte(rvaToAbsRef((void*)0x000065A1), 0x90);
	WriteE8Call(rvaToAbsRef((void*)0x000065B1), (void*)&orig_glTexParameterf_mag_ui);
	WriteByte(rvaToAbsRef((void*)0x000065B6), 0x90);

	if (_gl_texturemode) _gl_texturemode->modified = true;

#if FEATURE_VSYNC_TOGGLE
	gl_swapinterval = detour_Cvar_Get::oCvar_Get("gl_swapinterval","0",0,NULL);
	vid_ref = detour_Cvar_Get::oCvar_Get("vid_ref","gl",0,NULL);
#endif
}

#endif // FEATURE_TEXTURE_MAPPING_MIN_MAG
