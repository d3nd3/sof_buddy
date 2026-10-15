#include "feature_config.h"

#if FEATURE_MORE_CROSSHAIRS

#include "shared.h"
#include "crosshair_catalog.inc"
#include "generated_detours.h"
#include "util.h"
#include <cstdio>
#include <cstring>
#include <string>

namespace {

constexpr const char* kPickCvar = "_sofbuddy_crosshair_pick";
constexpr const char* kActivePakName = "active_crosshair.pak";
constexpr unsigned kCrosshairCvarRva = 0x00230eb0;
constexpr unsigned kCrosshairPicPtrRva = 0x00225940;

std::string buddy_root() {
    cvar_t* basedir = findCvar(const_cast<char*>("basedir"));
    const char* root = basedir && basedir->string && basedir->string[0] ? basedir->string : ".";
    return std::string(root) + "/sof_buddy";
}

std::string active_pak_path() { return buddy_root() + "/" + kActivePakName; }

void invalidate_crosshair_pic() {
    char* pic = static_cast<char*>(rvaToAbsExe(reinterpret_cast<void*>(kCrosshairPicPtrRva)));
    if (pic) pic[0] = '\0';
    cvar_t** crosshair = reinterpret_cast<cvar_t**>(rvaToAbsExe(reinterpret_cast<void*>(kCrosshairCvarRva)));
    if (crosshair && *crosshair) {
        auto* body = reinterpret_cast<unsigned char*>(*crosshair);
        body[0x14] = 1;
    }
}

bool copy_file(const std::string& src, const std::string& dst) {
    if (!CopyFileA(src.c_str(), dst.c_str(), FALSE)) return false;
    return true;
}

void remove_active_pak() {
    DeleteFileA(active_pak_path().c_str());
}

bool apply_catalog_index(int index) {
    if (index < 0 || index >= kCrosshairCatalogCount) return false;
    const CrosshairCatalogEntry& e = kCrosshairCatalog[index];
    if (e.pak_relpath && e.pak_relpath[0]) {
        const std::string src = buddy_root() + "/" + e.pak_relpath;
        if (!copy_file(src, active_pak_path())) {
            PrintOut(PRINT_BAD, "more_crosshairs: missing pack %s\n", src.c_str());
            return false;
        }
    } else {
        remove_active_pak();
    }
    if (!detour_Cvar_Set2::oCvar_Set2) return false;
    char slot[8];
    std::snprintf(slot, sizeof(slot), "%d", e.slot);
    detour_Cvar_Set2::oCvar_Set2(const_cast<char*>("crosshair"), slot, true);
    invalidate_crosshair_pic();
    return true;
}

void crosshair_pick_change(cvar_t* cvar) {
    if (!cvar) return;
    const int index = static_cast<int>(cvar->value + 0.5f);
    static int last_applied = -1;
    if (index == last_applied) return;
    if (apply_catalog_index(index)) last_applied = index;
}

}  // namespace

void more_crosshairs_PostCvarInit(void) {
    if (!detour_Cvar_Get::oCvar_Get) return;
    cvar_t* pick = detour_Cvar_Get::oCvar_Get(kPickCvar, "0", CVAR_SOFBUDDY_ARCHIVE, crosshair_pick_change);
    if (!pick) return;
    const int index = static_cast<int>(pick->value + 0.5f);
    if (!apply_catalog_index(index)) {
        detour_Cvar_Set2::oCvar_Set2(const_cast<char*>(kPickCvar), const_cast<char*>("0"), true);
        apply_catalog_index(0);
    }
}

#endif
