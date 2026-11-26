#pragma once

#include "feature_config.h"

#if FEATURE_MODEL_ANISOTROPIC

// Patches the engine's model/ghoul GL dispatch table so model textures receive
// the same anisotropic filtering as world textures.
void model_aniso_install(void* gl_table);

#endif // FEATURE_MODEL_ANISOTROPIC
