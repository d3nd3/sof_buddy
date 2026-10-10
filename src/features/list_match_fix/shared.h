#pragma once

#include "feature_config.h"

#if FEATURE_LIST_MATCH_FIX

// list_c (SoF.exe 2001, MSVC) field offsets. Verified against 1.06a
// SoF.exe (ImageBase 0x20000000): GetMatchedValue 0x200D3830,
// SetValue 0x200D3640, Handle 0x200D39E0, Draw 0x200D3AE0.
// Element stride 0x10, char* at +4. cvar_t: +0 name, +4 string.
namespace list_match_fix {

constexpr int kCvarOff = 0x50;
constexpr int kIndexOff = 0x9C;
constexpr int kBitmaskOff = 0xA8;
constexpr int kDispFirstOff = 0xB0;
constexpr int kDispLastOff = 0xB4;
constexpr int kMatchFirstOff = 0xC0;
constexpr int kMatchLastOff = 0xC4;
constexpr int kElemStride = 0x10;
constexpr int kElemStrOff = 0x04;
constexpr int kCvarNameOff = 0x00;
constexpr int kCvarStringOff = 0x04;

constexpr int kUnmatchedIndex = -1;

inline void* read_ptr(void* self, int off) {
    return *(void**)((char*)self + off);
}
inline int read_int(void* self, int off) {
    return *(int*)((char*)self + off);
}
inline void write_int(void* self, int off, int v) {
    *(int*)((char*)self + off) = v;
}

int match_count(void* self);
const char* match_str_at(void* self, int idx);
const char* cvar_string_of(void* self);
bool has_match_list(void* self);
bool cvar_starts_numeric(const char* s, double* out_val);
double match_numeric_value(const char* s, bool* ok);
int find_next_highest(void* self, const char* cur);
int find_next_lowest(void* self, const char* cur);

} // namespace list_match_fix

#endif // FEATURE_LIST_MATCH_FIX
