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
constexpr unsigned kCbufAddTextRva = 0x00018180;
constexpr int kGlImagePic = 3;  // ref_gl R_RegisterPic / Draw_Pic

using Cbuf_AddText_t = void(__cdecl*)(const char*);

Cbuf_AddText_t cbuf_add_text() {
    return reinterpret_cast<Cbuf_AddText_t>(rvaToAbsExe(reinterpret_cast<void*>(kCbufAddTextRva)));
}

bool g_suppress_theme_callback = false;
bool g_suppress_pick_callback = false;
bool g_suppress_slot_callback = false;
bool g_crosshair_menu_refresh_guard = false;

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

void apply_theme_and_slot(int theme, int slot) {
    if (!detour_Cvar_Set2::oCvar_Set2) return;
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%d", theme);
    g_suppress_theme_callback = true;
    detour_Cvar_Set2::oCvar_Set2(const_cast<char*>(kThemeCvar), buf, true);
    g_suppress_theme_callback = false;
    std::snprintf(buf, sizeof(buf), "%d", slot);
    g_suppress_slot_callback = true;
    detour_Cvar_Set2::oCvar_Set2(const_cast<char*>("crosshair"), buf, true);
    g_suppress_slot_callback = false;
    invalidate_crosshair_pic();
}

int menu_index_for_selection(int theme, int slot) {
    for (int i = 0; i < kCrosshairMenuCount; ++i) {
        if (kCrosshairMenu[i].theme_index == theme && kCrosshairMenu[i].slot == slot) return i;
    }
    for (int i = 0; i < kCrosshairMenuCount; ++i) {
        if (kCrosshairMenu[i].theme_index == theme) return i;
    }
    return 0;
}

void sync_pick_cvar_silent() {
    if (!detour_Cvar_Set2::oCvar_Set2) return;
    cvar_t* crosshair = detour_Cvar_Get::oCvar_Get("crosshair", "0", 0, nullptr);
    const int theme = theme_index_from_cvar();
    const int slot = clamp_crosshair_slot(crosshair ? crosshair->value : 0.0f);
    const int idx = menu_index_for_selection(theme, slot);
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%d", idx);
    g_suppress_pick_callback = true;
    detour_Cvar_Set2::oCvar_Set2(const_cast<char*>(kPickCvar), buf, true);
    g_suppress_pick_callback = false;
}

static void preview_menu_image_path(int theme, int slot, char* path, size_t path_sz) {
    if (theme == 0)
        std::snprintf(path, path_sz, "status/ch%d", slot);
    else
        std::snprintf(path, path_sz, "status/sbxch_%d_%d", theme, slot);
}

void update_preview_cvar() {
    if (!detour_Cvar_Set2::oCvar_Set2) return;
    cvar_t* crosshair = detour_Cvar_Get::oCvar_Get("crosshair", "0", 0, nullptr);
    const int theme = theme_index_from_cvar();
    const int slot = clamp_crosshair_slot(crosshair ? crosshair->value : 0.0f);
    char path[72];
    preview_menu_image_path(theme, slot, path, sizeof(path));
    char rmf[160];
    std::snprintf(rmf, sizeof(rmf), "<center><image \"%s\" scale 4><hbr>", path);
    detour_Cvar_Set2::oCvar_Set2(const_cast<char*>(kPreviewRmfCvar), rmf, true);
}

static void schedule_misc_menu_refresh() {
    Cbuf_AddText_t cbuf = cbuf_add_text();
    if (g_crosshair_menu_refresh_guard || !cbuf) return;
    cbuf("\nsofbuddy_menu sof_buddy/misc\n");
}

void apply_menu_pick(int index) {
    if (index < 0 || index >= kCrosshairMenuCount) return;
    const CrosshairMenuPick& row = kCrosshairMenu[index];
    if (row.theme_index == theme_index_from_cvar()) {
        cvar_t* crosshair = detour_Cvar_Get::oCvar_Get("crosshair", "0", 0, nullptr);
        if (crosshair && clamp_crosshair_slot(crosshair->value) == row.slot) return;
    }
    apply_theme_and_slot(row.theme_index, row.slot);
    update_preview_cvar();
    schedule_misc_menu_refresh();
}

void apply_legacy_pick(int index) {
    if (index < 0 || index >= kCrosshairLegacyCatalogCount) return;
    const CrosshairLegacyPick& row = kCrosshairLegacyCatalog[index];
    apply_theme_and_slot(row.theme_index, row.slot);
    sync_pick_cvar_silent();
    update_preview_cvar();
}

void migrate_saved_crosshair(void) {
    if (!detour_Cvar_Get::oCvar_Get || !detour_Cvar_Set2::oCvar_Set2) return;

    cvar_t* crosshair = detour_Cvar_Get::oCvar_Get("crosshair", "0", 0, nullptr);
    if (crosshair) {
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
        }
    }
    sync_pick_cvar_silent();
}

void crosshair_pick_change(cvar_t* cvar) {
    if (!cvar || g_suppress_pick_callback || g_crosshair_menu_refresh_guard) return;
    apply_menu_pick(static_cast<int>(cvar->value + 0.5f));
}

void crosshair_theme_change(cvar_t* cvar) {
    if (!cvar || g_suppress_theme_callback) return;
    invalidate_crosshair_pic();
}

void crosshair_slot_change(cvar_t* cvar) {
    if (!cvar || g_suppress_slot_callback) return;
    invalidate_crosshair_pic();
}

void remove_legacy_active_pak(void) {
    cvar_t* basedir = findCvar(const_cast<char*>("basedir"));
    const char* root = basedir && basedir->string && basedir->string[0] ? basedir->string : ".";
    const std::string path = std::string(root) + "/sof_buddy/active_crosshair.pak";
    if (GetFileAttributesA(path.c_str()) == INVALID_FILE_ATTRIBUTES) return;
    DeleteFileA(path.c_str());
}

static bool crosshair_slot_suffix(const char* num) {
    if (!num || !std::isdigit(static_cast<unsigned char>(*num))) return false;
    const int slot = *num - '0';
    if (slot < 0 || slot > 4) return false;
    const char next = num[1];
    return next == '\0' || std::strcmp(num + 1, ".m32") == 0;
}

bool is_crosshair_menu_image_path(const char* filename) {
    if (!filename) return false;
    if (std::strstr(filename, "sbxch_") != nullptr) return true;
    const char* ch = std::strstr(filename, "status/ch");
    if (ch) return crosshair_slot_suffix(ch + 7);
    ch = std::strstr(filename, "menus/status/ch");
    if (ch) return crosshair_slot_suffix(ch + 13);
    return false;
}

}  // namespace

void more_crosshairs_gl_findimage_pre(char* filename, int& imagetype, char&, char&) {
    if (!filename || !is_crosshair_menu_image_path(filename)) return;
    if (imagetype != kGlImagePic) imagetype = kGlImagePic;
}

void* more_crosshairs_gl_findimage_fixup(void* result, char* filename, int imagetype, char mimap,
                                         char allowPicmip) {
    if (result || !filename || !is_crosshair_menu_image_path(filename)) return result;
    if (imagetype == kGlImagePic) return result;
    if (!detour_GL_FindImage::oGL_FindImage) return result;
    return detour_GL_FindImage::oGL_FindImage(filename, kGlImagePic, mimap, allowPicmip);
}

const char* more_crosshairs_get_list_rmf(void) {
    static char buf[1024];
    const int n = std::snprintf(
        buf, sizeof(buf),
        "<list \"%s\" match \"%s\" cvar _sofbuddy_crosshair_pick atext \"Crosshair : \" "
        "noshade tip \"S0-S4 stock; f44/jay/k09/mhp/rep/sau/yum + slot.\"><hbr>",
        kCrosshairMenuListLabels, kCrosshairMenuListMatch);
    if (n < 0 || n >= static_cast<int>(sizeof(buf))) return "<hbr>";
    return buf;
}

void more_crosshairs_refresh_menu_preview(void) {
    g_crosshair_menu_refresh_guard = true;
    sync_pick_cvar_silent();
    update_preview_cvar();
    g_crosshair_menu_refresh_guard = false;
}

void more_crosshairs_PostCvarInit(void) {
    if (!detour_Cvar_Get::oCvar_Get) return;

    remove_legacy_active_pak();
    detour_Cvar_Get::oCvar_Get(kPreviewRmfCvar, "", 0, nullptr);

    g_suppress_theme_callback = true;
    detour_Cvar_Get::oCvar_Get(kThemeCvar, "0", CVAR_SOFBUDDY_ARCHIVE, crosshair_theme_change);
    g_suppress_theme_callback = false;

    detour_Cvar_Get::oCvar_Get(kPickCvar, "0", CVAR_SOFBUDDY_ARCHIVE, crosshair_pick_change);
    detour_Cvar_Get::oCvar_Get("crosshair", "0", 0, crosshair_slot_change);

    migrate_saved_crosshair();
    sync_pick_cvar_silent();
    more_crosshairs_refresh_menu_preview();
}

#endif
