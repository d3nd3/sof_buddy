#include "feature_config.h"

#if FEATURE_HD_TEXTURES

#include "util.h"
#include <cctype>
#include "../shared.h"

void gl_buildpolygonfromsurface_pre_callback(void*& msurface_s) {
	if (!msurface_s) return;
	
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
}

#endif // FEATURE_HD_TEXTURES

