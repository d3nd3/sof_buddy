#include "feature_config.h"

#if FEATURE_HASH_LOOKUP

#include "cvar.h"
#include "generated_detours.h"
#include "runtime_features.h"
#include "sof_compat.h"
#include "util.h"

namespace {

cvar_t* g_cvEnable = nullptr;  // _sofbuddy_hashmap

}  // namespace

void HashLookup_InitCvars() {
    if (!g_cvEnable && detour_Cvar_Get::oCvar_Get)
        g_cvEnable = detour_Cvar_Get::oCvar_Get(
            "_sofbuddy_hashmap", "1", CVAR_SOFBUDDY_ARCHIVE, nullptr);
}

bool HashLookup_Enabled() {
    return RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_HASH_LOOKUP) &&
           (!g_cvEnable || g_cvEnable->value != 0.0f);
}

#endif  // FEATURE_HASH_LOOKUP
