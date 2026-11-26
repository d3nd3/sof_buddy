#include "feature_config.h"

#if FEATURE_LOCAL_MP_FULL_MOVESCALE

#include "../shared.h"

void lmfm_PostCvarInit(void) {
	lmfm_create_cvars();
}

#endif // FEATURE_LOCAL_MP_FULL_MOVESCALE
