#include "feature_config.h"

#if FEATURE_HD_TEXTURES

#include "sof_compat.h"
#include "util.h"
#include "../shared.h"

void hd_textures_RefDllLoaded(char const* name)
{
	PrintOut(PRINT_LOG, "HD Textures: Initializing...\n");
	PrintOut(PRINT_LOG, "HD Textures: Loaded %zu textures\n", default_textures.size());
}

#endif // FEATURE_HD_TEXTURES

