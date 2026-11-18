#include "feature_config.h"

#if FEATURE_LIGHTSTYLE_INTERPOLATION

#include "util.h"
#include "../shared.h"

void lightstyle_interpolation_PostCvarInit(void) {
	PrintOut(PRINT_LOG, "lightstyle_interpolation: Registering CVars\n");
	create_lightstyle_interpolation_cvars();
}

#endif // FEATURE_LIGHTSTYLE_INTERPOLATION
