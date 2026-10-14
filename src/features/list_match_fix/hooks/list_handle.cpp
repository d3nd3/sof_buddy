#include "feature_config.h"

#if FEATURE_LIST_MATCH_FIX

#include "generated_detours.h"
#include "util.h"
#include "../shared.h"

// Handle override (param 1 = left/next, 2 = right/prev).
// Matched rows (and non-1/2 params) run the original. Unmatched rows (-1)
// compute the next-highest (left) or next-lowest (right) match entry by
// value — numeric leading-double order when the cvar starts with a number,
// else case-insensitive lex order — then bias curIndex by ∓1 so the
// original increment/decrement lands exactly on it. The original then does
// SetValue, sound and key-action dispatch unchanged.
int list_match_fix_handle(void* self, int param,
                          detour_List_Handle::tList_Handle original) {
    if (!self || !original) return 0;
    using namespace list_match_fix;
    int cur = read_int(self, kIndexOff);
    if (cur != kUnmatchedIndex) return original(self, param);
    if (param != 1 && param != 2) return original(self, param);
    if (!has_match_list(self)) return original(self, param);

    const char* cur_str = cvar_string_of(self);
    int n = match_count(self);
    if (n <= 0) return original(self, param);

    int desired = (param == 1) ? find_next_highest(self, cur_str)
                               : find_next_lowest(self, cur_str);
    if (desired < 0) desired = 0;
    if (desired >= n) desired = n - 1;

    // Bias so original ++/-- lands on desired:
    // left (++) from desired-1 (-1 wraps to 0 correctly),
    // right (--) from desired+1 (count wraps to count-1 correctly).
    write_int(self, kIndexOff, (param == 1) ? (desired - 1) : (desired + 1));
    return original(self, param);
}

#endif // FEATURE_LIST_MATCH_FIX
