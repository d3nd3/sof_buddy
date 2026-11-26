#include "feature_config.h"

#if FEATURE_FORCERUN_SINGLEPLAYER

#include "../shared.h"

void forcerun_PostCvarInit(void)
{
	forcerun_create_cvars();
}

#endif // FEATURE_FORCERUN_SINGLEPLAYER
