#include "feature_config.h"

#if FEATURE_HD_TEXTURES

#include "sof_compat.h"
#include "util.h"
#include "../shared.h"
#include "runtime_features.h"

extern void hd_textures_load_default_data(void);

void hd_textures_RefDllLoaded(char const* name)
{
	if (!RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_HD_TEXTURES)) return;
	hd_textures_load_default_data();
	PrintOut(PRINT_LOG, "HD Textures: Initializing...\n");
	PrintOut(PRINT_LOG, "HD Textures: Loaded %zu textures\n", default_textures.size());
}

#endif // FEATURE_HD_TEXTURES

