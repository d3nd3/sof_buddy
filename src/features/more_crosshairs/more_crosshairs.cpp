#include "feature_config.h"

#if FEATURE_MORE_CROSSHAIRS

#include "shared.h"
#include "crosshair_catalog.inc"
#include "generated_detours.h"
#include "util.h"
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <windows.h>

namespace {

constexpr const char* kThemeCvar = "_sofbuddy_crosshair_theme";
constexpr const char* kPickCvar = "_sofbuddy_crosshair_pick";
constexpr const char* kPreviewRmfCvar = "_sb_internal_crosshair_preview_rmf";
constexpr unsigned kCrosshairPicPtrRva = 0x00225940;
constexpr int kGlImageSprite = 1;

bool g_suppress_theme_callback = false;

void invalidate_crosshair_pic() {
    char* pic = static_cast<char*>(rvaToAbsExe(reinterpret_cast<void*>(kCrosshairPicPtrRva)));
    if (pic) pic[0] = '\0';
}

int clamp_crosshair_slot(float value) {
    int slot = static_cast<int>(value + 0.5f);
    if (slot < 0) slot = 0;
    if (slot > 4) slot = 4;
    return slot;
}

int theme_index_from_cvar() {
    if (!detour_Cvar_Get::oCvar_Get) return 0;
    cvar_t* theme = detour_Cvar_Get::oCvar_Get(kThemeCvar, "0", 0, nullptr);
    int idx = theme ? static_cast<int>(theme->value + 0.5f) : 0;
    if (idx < 0 || idx >= kCrosshairThemeCount) idx = 0;
    return idx;
}

void apply_legacy_pick(int index) {
    if (index < 0 || index >= kCrosshairLegacyCatalogCount) return;
    const CrosshairLegacyPick& row = kCrosshairLegacyCatalog[index];
    if (!detour_Cvar_Set2::oCvar_Set2) return;
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%d", row.theme_index);
    g_suppress_theme_callback = true;
    detour_Cvar_Set2::oCvar_Set2(const_cast<char*>(kThemeCvar), buf, true);
    g_suppress_theme_callback = false;
    std::snprintf(buf, sizeof(buf), "%d", row.slot);
    detour_Cvar_Set2::oCvar_Set2(const_cast<char*>("crosshair"), buf, true);
    invalidate_crosshair_pic();
    more_crosshairs_refresh_menu_preview();
}

void migrate_saved_crosshair(void) {
    if (!detour_Cvar_Get::oCvar_Get || !detour_Cvar_Set2::oCvar_Set2) return;

    cvar_t* pick = detour_Cvar_Get::oCvar_Get(kPickCvar, "", 0, nullptr);
    if (pick && pick->string && pick->string[0]) {
        const int index = static_cast<int>(pick->value + 0.5f);
        if (index >= 0 && index < kCrosshairLegacyCatalogCount) {
            apply_legacy_pick(index);
            return;
        }
    }

    cvar_t* crosshair = detour_Cvar_Get::oCvar_Get("crosshair", "0", 0, nullptr);
    if (!crosshair) return;
    const int value = static_cast<int>(crosshair->value + 0.5f);
    if (value >= 5 && value < kCrosshairLegacyCatalogCount) {
        apply_legacy_pick(value);
        return;
    }
    if (value > 4) {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "%d", 4);
        detour_Cvar_Set2::oCvar_Set2(const_cast<char*>("crosshair"), buf, true);
        invalidate_crosshair_pic();
        more_crosshairs_refresh_menu_preview();
    }
}

void crosshair_theme_change(cvar_t* cvar) {
    if (!cvar || g_suppress_theme_callback) return;
    invalidate_crosshair_pic();
    more_crosshairs_refresh_menu_preview();
}

void crosshair_slot_change(cvar_t* cvar) {
    if (!cvar) return;
    invalidate_crosshair_pic();
    more_crosshairs_refresh_menu_preview();
}

void remove_legacy_active_pak(void) {
    cvar_t* basedir = findCvar(const_cast<char*>("basedir"));
    const char* root = basedir && basedir->string && basedir->string[0] ? basedir->string : ".";
    const std::string path = std::string(root) + "/sof_buddy/active_crosshair.pak";
    if (GetFileAttributesA(path.c_str()) == INVALID_FILE_ATTRIBUTES) return;
    DeleteFileA(path.c_str());
}

bool is_crosshair_menu_image_path(const char* filename) {
    if (!filename) return false;
    if (std::strstr(filename, "sbxch_") != nullptr) return true;
    if (std::strncmp(filename, "pics/menus/status/ch", 20) != 0) return false;
    const char* num = filename + 20;
    if (!std::isdigit(static_cast<unsigned char>(*num))) return false;
    const int slot = *num - '0';
    if (slot < 0 || slot > 4) return false;
    const char next = num[1];
    return next == '\0' || std::strcmp(num + 1, ".m32") == 0;
}

}  // namespace

void* more_crosshairs_gl_findimage_fixup(void* result, char* filename, int imagetype, char mimap,
                                         char allowPicmip) {
    if (result || !filename || !is_crosshair_menu_image_path(filename)) return result;
    if (imagetype == kGlImageSprite) return result;
    if (!detour_GL_FindImage::oGL_FindImage) return result;
    return detour_GL_FindImage::oGL_FindImage(filename, kGlImageSprite, mimap, allowPicmip);
}

void more_crosshairs_refresh_menu_preview(void) {
    if (!detour_Cvar_Get::oCvar_Get || !detour_Cvar_Set2::oCvar_Set2) return;

    cvar_t* crosshair = detour_Cvar_Get::oCvar_Get("crosshair", "0", 0, nullptr);
    const int slot = clamp_crosshair_slot(crosshair ? crosshair->value : 0.0f);
    const int theme = theme_index_from_cvar();

    char path[56];
    if (theme == 0)
        std::snprintf(path, sizeof(path), "status/ch%d", slot);
    else
        std::snprintf(path, sizeof(path), "status/sbxch_%d_%d", theme, slot);

    char rmf[128];
    std::snprintf(rmf, sizeof(rmf), "<center><image \"%s\" scale 4><hbr>", path);
    detour_Cvar_Set2::oCvar_Set2(const_cast<char*>(kPreviewRmfCvar), rmf, true);
}

void more_crosshairs_PostCvarInit(void) {
    if (!detour_Cvar_Get::oCvar_Get) return;

    remove_legacy_active_pak();
    detour_Cvar_Get::oCvar_Get(kPreviewRmfCvar, "", 0, nullptr);

    g_suppress_theme_callback = true;
    detour_Cvar_Get::oCvar_Get(kThemeCvar, "0", CVAR_SOFBUDDY_ARCHIVE, crosshair_theme_change);
    g_suppress_theme_callback = false;

    detour_Cvar_Get::oCvar_Get("crosshair", "0", 0, crosshair_slot_change);

    migrate_saved_crosshair();
    more_crosshairs_refresh_menu_preview();
}

#endif
