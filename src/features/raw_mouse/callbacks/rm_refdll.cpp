#include "feature_config.h"

#if FEATURE_RAW_MOUSE

#include "sof_compat.h"
#include "util.h"
#include "../shared.h"

void raw_mouse_RefDllLoaded(char const* name)
{
    create_raw_mouse_cvars();
}

#endif // FEATURE_RAW_MOUSE

