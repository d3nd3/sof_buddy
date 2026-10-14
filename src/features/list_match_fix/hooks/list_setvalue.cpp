#include "feature_config.h"

#if FEATURE_LIST_MATCH_FIX

#include "generated_detours.h"
#include "util.h"
#include "../shared.h"

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
}

#endif // FEATURE_LIST_MATCH_FIX
