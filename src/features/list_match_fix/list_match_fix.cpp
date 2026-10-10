#include "feature_config.h"

#if FEATURE_LIST_MATCH_FIX

#include "shared.h"
#include "sof_compat.h"
#include "util.h"

#include <cstdlib>
#include <cstring>

namespace list_match_fix {

int match_count(void* self) {
    if (!self) return 0;
    char* first = (char*)read_ptr(self, kMatchFirstOff);
    char* last = (char*)read_ptr(self, kMatchLastOff);
    if (!first || !last || last <= first) return 0;
    return (int)((last - first) / kElemStride);
}

const char* match_str_at(void* self, int idx) {
    char* first = (char*)read_ptr(self, kMatchFirstOff);
    if (!first) return "";
    char* elem = first + idx * kElemStride;
    char* s = *(char**)(elem + kElemStrOff);
    return s ? s : "";
}

const char* cvar_string_of(void* self) {
    void* cvar = read_ptr(self, kCvarOff);
    if (!cvar) return "";
    char* s = *(char**)((char*)cvar + kCvarStringOff);
    return s ? s : "";
}

bool has_match_list(void* self) {
    if (!self) return false;
    if (read_int(self, kBitmaskOff) != 0) return false;
    if (!read_ptr(self, kCvarOff)) return false;
    return match_count(self) > 0;
}

bool cvar_starts_numeric(const char* s, double* out_val) {
    if (!s || !*s) return false;
    // Skip leading spaces (cvars should not have them, but strtod does).
    while (*s == ' ' || *s == '\t') ++s;
    if (!*s) return false;
    char* end = nullptr;
    double v = strtod(s, &end);
    if (end == s) return false;
    if (out_val) *out_val = v;
    return true;
}

double match_numeric_value(const char* s, bool* ok) {
    if (!s || !*s) {
        if (ok) *ok = false;
        return 0.0;
    }
    while (*s == ' ' || *s == '\t') ++s;
    char* end = nullptr;
    double v = strtod(s, &end);
    if (end == s) {
        if (ok) *ok = false;
        return 0.0;
    }
    if (ok) *ok = true;
    return v;
}

static int icmp(const char* a, const char* b) {
#ifdef _WIN32
    return _stricmp(a ? a : "", b ? b : "");
#else
    return strcasecmp(a ? a : "", b ? b : "");
#endif
}

int find_next_highest(void* self, const char* cur) {
    int n = match_count(self);
    if (n <= 0) return 0;
    double cur_num = 0.0;
    bool cur_is_num = cvar_starts_numeric(cur, &cur_num);
    if (cur_is_num) {
        // Numeric: smallest match value strictly greater than cur.
        int best = -1;
        double best_val = 0.0;
        bool have_best = false;
        double min_val = 0.0;
        int min_idx = 0;
        bool have_min = false;
        for (int i = 0; i < n; ++i) {
            const char* m = match_str_at(self, i);
            bool ok = false;
            double v = match_numeric_value(m, &ok);
            if (!ok) continue;
            if (!have_min || v < min_val) {
                min_val = v;
                min_idx = i;
                have_min = true;
            }
            if (v > cur_num) {
                if (!have_best || v < best_val) {
                    best_val = v;
                    best = i;
                    have_best = true;
                }
            }
        }
        if (have_best) return best;
        if (have_min) return min_idx; // wrap to smallest
        // No numeric matches at all: fall through to lex.
    }
    // Lexicographic (case-insensitive): smallest match strictly greater than cur.
    int best = -1;
    int min_idx = 0;
    bool have_min = false;
    for (int i = 0; i < n; ++i) {
        const char* m = match_str_at(self, i);
        int c = icmp(m, cur);
        if (!have_min || icmp(m, match_str_at(self, min_idx)) < 0) {
            min_idx = i;
            have_min = true;
        }
        if (c > 0) {
            if (best < 0 || icmp(m, match_str_at(self, best)) < 0) {
                best = i;
            }
        }
    }
    if (best >= 0) return best;
    return have_min ? min_idx : 0;
}

int find_next_lowest(void* self, const char* cur) {
    int n = match_count(self);
    if (n <= 0) return 0;
    double cur_num = 0.0;
    bool cur_is_num = cvar_starts_numeric(cur, &cur_num);
    if (cur_is_num) {
        int best = -1;
        double best_val = 0.0;
        bool have_best = false;
        double max_val = 0.0;
        int max_idx = 0;
        bool have_max = false;
        for (int i = 0; i < n; ++i) {
            const char* m = match_str_at(self, i);
            bool ok = false;
            double v = match_numeric_value(m, &ok);
            if (!ok) continue;
            if (!have_max || v > max_val) {
                max_val = v;
                max_idx = i;
                have_max = true;
            }
            if (v < cur_num) {
                if (!have_best || v > best_val) {
                    best_val = v;
                    best = i;
                    have_best = true;
                }
            }
        }
        if (have_best) return best;
        if (have_max) return max_idx; // wrap to largest
    }
    int best = -1;
    int max_idx = 0;
    bool have_max = false;
    for (int i = 0; i < n; ++i) {
        const char* m = match_str_at(self, i);
        int c = icmp(m, cur);
        if (!have_max || icmp(m, match_str_at(self, max_idx)) > 0) {
            max_idx = i;
            have_max = true;
        }
        if (c < 0) {
            if (best < 0 || icmp(m, match_str_at(self, best)) > 0) {
                best = i;
            }
        }
    }
    if (best >= 0) return best;
    return have_max ? max_idx : 0;
}

} // namespace list_match_fix

#endif // FEATURE_LIST_MATCH_FIX
