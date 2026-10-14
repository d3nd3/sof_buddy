#include "feature_config.h"

#if FEATURE_TEXTURE_MAPPING_MIN_MAG

#include "sof_compat.h"
#include "util.h"
#include "../shared.h"

void texturemapping_PostCvarInit()
{
	PrintOut(PRINT_LOG, "texture_mapping: Registering CVars\n");
	create_texturemapping_cvars();
}

#endif // FEATURE_TEXTURE_MAPPING_MIN_MAG

