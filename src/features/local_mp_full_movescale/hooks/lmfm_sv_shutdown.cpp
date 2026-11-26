#include "feature_config.h"

#if FEATURE_LOCAL_MP_FULL_MOVESCALE

#include "../shared.h"

void lmfm_SV_ShutdownGameProgs_post(void) {
	lmfm_restore_movescale();
}

#endif // FEATURE_LOCAL_MP_FULL_MOVESCALE
