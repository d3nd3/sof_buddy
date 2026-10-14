#include "feature_config.h"

#if FEATURE_VSYNC_TOGGLE

#include "sof_compat.h"
#include "util.h"
#include "../shared.h"

void vsync_on_postcvarinit(char const* name)
{
    create_vsync_cvars();
}

#endif // FEATURE_VSYNC_TOGGLE

