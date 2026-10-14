/*
	HD Textures Support - Placeholder
	
	Enables support for 4x larger textures with correct UV mapping.
	This file needs to be populated with the full texture database.
*/

#include "feature_config.h"

#if FEATURE_HD_TEXTURES

#include "util.h"
#include "shared.h"

#include <cctype>

std::unordered_map<std::string, m32size> default_textures;

void hd_textures_load_default_data(void) {
	if (!default_textures.empty()) return;
	default_textures.reserve(4096);
	#include "./default_textures.h"
}


#endif // FEATURE_HD_TEXTURES
