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

static const bool s_default_textures_loaded = ([]() {
	#include "./default_textures.h"
	return true;
})();


#endif // FEATURE_HD_TEXTURES
