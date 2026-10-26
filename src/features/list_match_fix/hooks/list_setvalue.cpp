#include "feature_config.h"

#if FEATURE_LIST_MATCH_FIX

#include "generated_detours.h"
#include "sofbuddy_cfg.h"
#include "util.h"
#include "../shared.h"
#if FEATURE_MORE_CROSSHAIRS
#include "features/more_crosshairs/shared.h"
#endif
#include <cstring>

// SetValue override: when the row is unmatched (index -1) do not run
// Cvar_Set(match[0]) — that reset is the damaging behaviour. The cvar keeps
// its off-list value; width is left as Setup left it (max-label based) until
// the user picks a listed value, at which point the original path updates it.
void list_match_fix_setvalue(
    void* self, detour_List_SetValue::tList_SetValue original) {
    if (!self || !original) return;
    if (list_match_fix::read_int(self, list_match_fix::kIndexOff) ==
        list_match_fix::kUnmatchedIndex) {
        return;
    }
    original(self);
    cvar_t* cv = static_cast<cvar_t*>(list_match_fix::read_ptr(self, list_match_fix::kCvarOff));
    if (cv && cv->name) {
        sofbuddy_cfg_on_spm_cvar_written(cv->name);
#if FEATURE_MORE_CROSSHAIRS
        if (std::strcmp(cv->name, "_sofbuddy_crosshair_pick") == 0)
            more_crosshairs_apply_pick_index(static_cast<int>(cv->value + 0.5f));
#endif
    }
}

#endif // FEATURE_LIST_MATCH_FIX
