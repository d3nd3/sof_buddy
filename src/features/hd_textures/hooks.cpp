/*
	HD Textures Support - Placeholder
	
	Enables support for 4x larger textures with correct UV mapping.
	This file needs to be populated with the full texture database.
*/

#include "feature_config.h"

#if FEATURE_HD_TEXTURES

#include "feature_macro.h"
#include "shared_hook_manager.h"
#include "util.h"

#include <unordered_map>
#include <string>
#include <cctype>

typedef struct {
	unsigned short w;
	unsigned short h;
} m32size;

std::unordered_map<std::string, m32size> default_textures;

static const bool s_default_textures_loaded = ([]() {
	#include "./default_textures.h"
	return true;
})();

static void hd_textures_RefDllLoaded(void);
REGISTER_SHARED_HOOK_CALLBACK(RefDllLoaded, hd_textures, hd_textures_RefDllLoaded, 70, Post);

REGISTER_HOOK(GL_BuildPolygonFromSurface, (void*)0x00016390, RefDll, void, __cdecl, void* msurface_s)
{
	unsigned int* mtexinfo_t = (unsigned int*)((char*)msurface_s + 0x34);
	unsigned int* image_s = (unsigned int*)((char*)*mtexinfo_t + 0x2C);
	unsigned short* img_w = (unsigned short*)((char*)*image_s + 0x44);
	unsigned short* img_h = (unsigned short*)((char*)*image_s + 0x46);
	
	char* name = (char*)*image_s;
	
	std::string texName = name;
	for(auto& elem : texName)
		elem = std::tolower((unsigned char)elem);
	
	if (default_textures.count(texName) > 0) {
		*img_w = default_textures[texName].w;
		*img_h = default_textures[texName].h;
	}
	
	if (oGL_BuildPolygonFromSurface) {
		oGL_BuildPolygonFromSurface(msurface_s);
	}
}

static void hd_textures_RefDllLoaded(void)
{
	PrintOut(PRINT_LOG, "HD Textures: Initializing...\n");
	PrintOut(PRINT_LOG, "HD Textures: Loaded %zu textures\n", default_textures.size());
}

#endif // FEATURE_HD_TEXTURES
