#include "feature_config.h"

#if FEATURE_LIGHTING_BLEND

#include "util.h"
#include "../shared.h"

void lightblend_PostCvarInit()
{
	PrintOut(PRINT_LOG, "lighting_blend: Registering CVars\n");
	create_lightingblend_cvars();
}

#endif // FEATURE_LIGHTING_BLEND

