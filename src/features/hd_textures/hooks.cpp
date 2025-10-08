/*
	HD Textures Support - Placeholder
	
	Enables support for 4x larger textures with correct UV mapping.
	This file needs to be populated with the full texture database.
*/

#include "../../../hdr/feature_config.h"

#if FEATURE_HD_TEXTURES

#include "../../../hdr/feature_macro.h"
#include "../../../hdr/shared_hook_manager.h"
#include "../../../hdr/util.h"

#include <unordered_map>
#include <string>

// Texture size structure
typedef struct {
	unsigned short w;
	unsigned short h;
} m32size;

// Map to store default texture sizes
std::unordered_map<std::string, m32size> default_textures;

// Populate defaults once at static initialization time
static const bool s_default_textures_loaded = ([]() {
	#include "./default_textures.h"
	return true;
})();

// Forward declarations
static void hd_textures_RefDllLoaded(void);

// Hook for GL_BuildPolygonFromSurface to fix UV coordinates
REGISTER_HOOK(GL_BuildPolygonFromSurface, 0x30016390, void, __cdecl, void* msurface_s);

// Register for ref.dll loaded callback
REGISTER_SHARED_HOOK_CALLBACK(RefDllLoaded, hd_textures, hd_textures_RefDllLoaded, 70, Post);

void hkGL_BuildPolygonFromSurface(void* msurface_s)
{
	// Get texture info from surface
	unsigned int* mtexinfo_t = (unsigned int*)((char*)msurface_s + 0x34);
	unsigned int* image_s = (unsigned int*)((char*)*mtexinfo_t + 0x2C);
	unsigned short* img_w = (unsigned short*)((char*)*image_s + 0x44);
	unsigned short* img_h = (unsigned short*)((char*)*image_s + 0x46);
	
	char* name = (char*)*image_s;
	
	// Convert texture name to lowercase
	std::string texName = name;
	for(auto& elem : texName)
		elem = std::tolower(elem);
	
	// Restore original dimensions if known
	if (default_textures.count(texName) > 0) {
		*img_w = default_textures[texName].w;
		*img_h = default_textures[texName].h;
	}
	
	oGL_BuildPolygonFromSurface(msurface_s);
}

static void hd_textures_RefDllLoaded(void)
{
	PrintOut(PRINT_LOG, "HD Textures: Initializing...\n");
	PrintOut(PRINT_LOG, "HD Textures: Loaded %zu textures\n", default_textures.size());
}

#endif // FEATURE_HD_TEXTURES
