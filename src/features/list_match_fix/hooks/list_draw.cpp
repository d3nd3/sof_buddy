#include "feature_config.h"

#if FEATURE_LIST_MATCH_FIX

#include "generated_detours.h"
#include "util.h"
#include "../shared.h"

#include <cstring>

// Draw override: unmatched rows (-1) show the live cvar string instead of
// label[0]. We temporarily graft the cvar string onto display element 0
// (char* at +4, length at +8/+12 to cover both Len/Res orders), set index 0,
// run the original draw, then restore. Matched rows run the original.
void list_match_fix_draw(void* self,
                         detour_List_Draw::tList_Draw original) {
    if (!self || !original) return;
    using namespace list_match_fix;
    if (read_int(self, kIndexOff) != kUnmatchedIndex) {
        original(self);
        return;
    }
    void* disp_first = read_ptr(self, kDispFirstOff);
    void* disp_last = read_ptr(self, kDispLastOff);
    void* cvar = read_ptr(self, kCvarOff);
    if (!disp_first || !disp_last || disp_last <= disp_first || !cvar) {
        return; // nothing safe to draw; skip rather than crash
    }
    const char* cvar_str = cvar_string_of(self);
    if (!cvar_str) cvar_str = "";
    size_t len = strlen(cvar_str);

    char* elem0 = (char*)disp_first;
    // Save original 16-byte element (MSVC string: allocator/? + Ptr + Len + Res).
    char saved[kElemStride];
    memcpy(saved, elem0, kElemStride);

    // Graft cvar string; keep +0 (allocator) from the original element.
    *(const char**)(elem0 + kElemStrOff) = cvar_str;
    *(int*)(elem0 + 8) = (int)len;
    *(int*)(elem0 + 12) = (int)len;
    write_int(self, kIndexOff, 0);

    original(self);

    memcpy(elem0, saved, kElemStride);
    write_int(self, kIndexOff, kUnmatchedIndex);
}

#endif // FEATURE_LIST_MATCH_FIX
