#include "feature_config.h"

#if FEATURE_LIST_MATCH_FIX

#include "generated_detours.h"
#include "util.h"
#include "../shared.h"

#include <cstring>

// GetMatchedValue override: return -1 when a <list match> row holds an
// off-list cvar value instead of resetting to index 0. Callers
// (Reinitialise/Cleanup) then preserve the cvar via the SetValue guard.
int list_match_fix_getmatchedvalue(
    void* self, detour_List_GetMatchedValue::tList_GetMatchedValue original) {
    if (!self || !original) return 0;
    int orig = original(self);
    if (orig != 0) return orig;

    using namespace list_match_fix;
    // Only match-backed rows can be unmatched. Bitmask/cvari-without-match
    // and null-cvar rows keep the engine result (0).
    if (read_int(self, kBitmaskOff) != 0) return orig;
    if (!read_ptr(self, kCvarOff)) return orig;
    if (match_count(self) <= 0) return orig;

    const char* cur = cvar_string_of(self);
    const char* first = match_str_at(self, 0);
#ifdef _WIN32
    if (_stricmp(cur, first) == 0) return 0;
#else
    if (strcasecmp(cur, first) == 0) return 0;
#endif
    return kUnmatchedIndex;
}

#endif // FEATURE_LIST_MATCH_FIX
