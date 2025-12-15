#pragma once

#include "feature_config.h"

#if FEATURE_HD_TEXTURES

#include <string>
#include <unordered_map>

struct m32size {
    unsigned short w;
    unsigned short h;
};

extern std::unordered_map<std::string, m32size> default_textures;

#endif

