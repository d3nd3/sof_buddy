#include "feature_config.h"

#if FEATURE_MORE_CROSSHAIRS

#include "shared.h"
#include "crosshair_catalog.inc"
#include "generated_detours.h"
#include "util.h"
#if FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE
#include "features/scaled_ui_base/caller_from.h"
#include "features/scaled_ui_base/shared.h"
#endif
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <windows.h>

namespace {

struct ExternalPak {
    std::string path;
    int8_t slot_map[5];
    char abbr[8];
};

struct ExternalMenuPick {
    int theme;
    int slot;
    char abbr[8];
};

std::vector<ExternalPak> g_external_paks;
std::vector<ExternalMenuPick> g_external_menu;

static std::string game_root_dir() {
    cvar_t* basedir = findCvar(const_cast<char*>("basedir"));
    std::string root = basedir && basedir->string && basedir->string[0] ? basedir->string : ".";
    while (root.size() > 1 && (root.back() == '/' || root.back() == '\\')) root.pop_back();
    return root;
}

static bool is_stock_pak_name(const char* name) {
    return name && name[0] == 'p' && name[1] == 'a' && name[2] == 'k' && std::isdigit(static_cast<unsigned char>(name[3])) &&
           name[4] == '.' && _stricmp(name + 5, "pak") == 0;
}

static bool is_bundled_crosshair_stem(const char* stem) {
    if (!stem || !stem[0]) return false;
    for (int i = 1; i < kCrosshairThemeCount; ++i) {
        if (_stricmp(kCrosshairThemes[i].name, stem) == 0) return true;
    }
    return false;
}

static bool parse_status_ch_toc_name(const char* name, int& slot_out) {
    if (!name) return false;
    std::string s(name);
    std::replace(s.begin(), s.end(), '\\', '/');
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    static const char kPrefix[] = "pics/menus/status/ch";
    if (s.compare(0, sizeof(kPrefix) - 1, kPrefix) != 0) return false;
    const char* p = s.c_str() + sizeof(kPrefix) - 1;
    if (!std::isdigit(static_cast<unsigned char>(*p))) return false;
    const int slot = *p - '0';
    if (slot < 0 || slot > 4) return false;
    const char* tail = p + 1;
    if (tail[0] != '\0' && std::strcmp(tail, ".m32") != 0) return false;
    slot_out = slot;
    return true;
}

static bool pak_crosshair_slot_map(const char* pak_path, int8_t slot_map[5]) {
    if (!pak_path || !slot_map) return false;
    for (int i = 0; i < 5; ++i) slot_map[i] = -1;
    FILE* file = std::fopen(pak_path, "rb");
    if (!file) return false;
    char magic[4] = {};
    uint32_t toc_off = 0;
    uint32_t dir_len = 0;
    if (std::fread(magic, 1, 4, file) != 4 || std::memcmp(magic, "PACK", 4) != 0 ||
        std::fread(&toc_off, sizeof(toc_off), 1, file) != 1 ||
        std::fread(&dir_len, sizeof(dir_len), 1, file) != 1 || dir_len == 0 || dir_len % 64 != 0) {
        std::fclose(file);
        return false;
    }
    std::fseek(file, 0, SEEK_END);
    const long file_sz = std::ftell(file);
    std::vector<uint32_t> toc_positions;
    toc_positions.push_back(toc_off);
    if (file_sz > 0 && static_cast<uint64_t>(file_sz) >= dir_len &&
        static_cast<uint64_t>(file_sz) - dir_len != toc_off)
        toc_positions.push_back(static_cast<uint32_t>(file_sz - dir_len));
    bool any = false;
    for (uint32_t toc_pos : toc_positions) {
        for (uint32_t pos = 0; pos < dir_len; pos += 64) {
            char name[57] = {};
            if (std::fseek(file, static_cast<long>(toc_pos + pos), SEEK_SET) != 0) break;
            if (std::fread(name, 1, 56, file) != 56) break;
            int slot = -1;
            if (!parse_status_ch_toc_name(name, slot)) continue;
            slot_map[slot] = static_cast<int8_t>(slot);
            any = true;
        }
    }
    std::fclose(file);
    return any;
}

static void rebuild_external_menu_rows() {
    g_external_menu.clear();
    for (size_t pi = 0; pi < g_external_paks.size(); ++pi) {
        const ExternalPak& pak = g_external_paks[pi];
        const int theme = kCrosshairThemeCount + static_cast<int>(pi);
        for (int es = 0; es < 5; ++es) {
            if (pak.slot_map[es] < 0) continue;
            ExternalMenuPick row = {};
            row.theme = theme;
            row.slot = es;
            std::snprintf(row.abbr, sizeof(row.abbr), "%.3s", pak.abbr);
            g_external_menu.push_back(row);
        }
    }
}

static void discover_external_crosshair_paks() {
    g_external_paks.clear();
    g_external_menu.clear();

    const std::string root = game_root_dir();
    cvar_t* gd = findCvar(const_cast<char*>("gamedir"));
    const char* rel = gd && gd->string && gd->string[0] ? gd->string : "base";
    std::vector<std::string> dirs;
    const std::string primary = root + "/" + rel;
    const std::string base = root + "/base";
    dirs.push_back(primary);
    if (primary != base) dirs.push_back(base);

    std::vector<std::string> candidates;
    for (const std::string& dir : dirs) {
        WIN32_FIND_DATAA entry = {};
        HANDLE search = FindFirstFileA((dir + "/*.pak").c_str(), &entry);
        if (search == INVALID_HANDLE_VALUE) continue;
        do {
            if (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            if (is_stock_pak_name(entry.cFileName)) continue;
            const char* dot = std::strrchr(entry.cFileName, '.');
            if (!dot) continue;
            char stem[64];
            const size_t stem_len = static_cast<size_t>(dot - entry.cFileName);
            if (stem_len >= sizeof(stem)) continue;
            std::memcpy(stem, entry.cFileName, stem_len);
            stem[stem_len] = '\0';
            if (is_bundled_crosshair_stem(stem)) continue;
            candidates.push_back(dir + "/" + entry.cFileName);
        } while (FindNextFileA(search, &entry));
        FindClose(search);
    }
    std::sort(candidates.begin(), candidates.end());

    for (const std::string& pak_path : candidates) {
        const char* leaf = std::strrchr(pak_path.c_str(), '/');
        if (!leaf) leaf = pak_path.c_str();
        else ++leaf;
        const char* dot = std::strrchr(leaf, '.');
        if (!dot) continue;
        char stem[64];
        const size_t stem_len = static_cast<size_t>(dot - leaf);
        if (stem_len >= sizeof(stem)) continue;
        std::memcpy(stem, leaf, stem_len);
        stem[stem_len] = '\0';

        int8_t slot_map[5];
        if (!pak_crosshair_slot_map(pak_path.c_str(), slot_map)) continue;
        ExternalPak pak = {};
        pak.path = pak_path;
        for (int i = 0; i < 5; ++i) pak.slot_map[i] = slot_map[i];
        std::snprintf(pak.abbr, sizeof(pak.abbr), "%.3s", stem);
        g_external_paks.push_back(pak);
    }
    rebuild_external_menu_rows();
}

static int max_theme_index() {
    if (g_external_paks.empty()) return kCrosshairThemeCount - 1;
    return kCrosshairThemeCount + static_cast<int>(g_external_paks.size()) - 1;
}

constexpr const char* kThemeCvar = "_sofbuddy_crosshair_theme";
constexpr const char* kPickCvar = "_sofbuddy_crosshair_pick";
constexpr const char* kTintCvar = "_sofbuddy_crosshair_tint";
constexpr const char* kPreviewRmfCvar = "_sb_internal_crosshair_preview_rmf";
constexpr int kCrosshairTintCount = 9;

using glColor4f_fn = void(__stdcall*)(float, float, float, float);
static glColor4f_fn g_gl_color4f = nullptr;
constexpr unsigned kCrosshairPicPtrRva = 0x00225940;
constexpr unsigned kCrosshairWidthRva = 0x00225938;
constexpr unsigned kCrosshairHeightRva = 0x00225EA0;
constexpr unsigned kDrawGetPicSizeRva = 0x004035B4;
constexpr unsigned kClsKeyDestRva = 0x001C1F04;
constexpr int kKeyDestMenu = 3;
constexpr int kGlImagePic = 3;  // ref_gl R_RegisterPic / Draw_Pic

using Draw_GetPicSize_t = void(__cdecl*)(int*, int*, char*);
bool g_suppress_theme_callback = false;
bool g_suppress_pick_callback = false;
bool g_suppress_slot_callback = false;
bool g_crosshair_menu_refresh_guard = false;
static int g_deferred_pick_index = -1;
static bool g_crosshairs_cvars_ready = false;
static char g_active_buddy_page[72] = {};
static char g_menu_preview_pic_path[80] = {};
static bool g_defer_crosshair_pic_refresh = false;

char* crosshair_pic_ptr_buf() {
    return static_cast<char*>(rvaToAbsExe(reinterpret_cast<void*>(kCrosshairPicPtrRva)));
}

int clamp_crosshair_slot(float value) {
    int slot = static_cast<int>(value + 0.5f);
    if (slot < 0) slot = 0;
    if (slot > 4) slot = 4;
    return slot;
}

static void ensure_gl_color4f() {
    if (g_gl_color4f) return;
    HMODULE gl = GetModuleHandleA("opengl32.dll");
    if (!gl) gl = LoadLibraryA("opengl32.dll");
    if (gl) g_gl_color4f = reinterpret_cast<glColor4f_fn>(GetProcAddress(gl, "glColor4f"));
}

static int stock_tint_index() {
    if (!detour_Cvar_Get::oCvar_Get) return 0;
    cvar_t* tint = detour_Cvar_Get::oCvar_Get(kTintCvar, "0", 0, nullptr);
    int idx = tint ? static_cast<int>(tint->value + 0.5f) : 0;
    if (idx < 0) idx = 0;
    if (idx >= kCrosshairTintCount) idx = 0;
    return idx;
}

static void stock_tint_rgb(int idx, float& r, float& g, float& b) {
    r = g = b = 1.0f;
    switch (idx) {
        case 1: r = 1.0f; g = 0.2f; b = 0.2f; break;   // red
        case 2: r = 0.2f; g = 1.0f; b = 0.2f; break;   // green
        case 3: r = 0.35f; g = 0.55f; b = 1.0f; break; // blue
        case 4: r = 1.0f; g = 1.0f; b = 0.2f; break;   // yellow
        case 5: r = 0.2f; g = 1.0f; b = 1.0f; break;   // cyan
        case 6: r = 1.0f; g = 0.2f; b = 1.0f; break;   // magenta
        case 7: r = 1.0f; g = 0.55f; b = 0.1f; break;  // orange
        case 8: r = 0.65f; g = 0.2f; b = 1.0f; break;  // purple
        default: break;
    }
}

int theme_index_from_cvar() {
    if (!detour_Cvar_Get::oCvar_Get) return 0;
    cvar_t* theme = detour_Cvar_Get::oCvar_Get(kThemeCvar, "0", 0, nullptr);
    int idx = theme ? static_cast<int>(theme->value + 0.5f) : 0;
    if (idx < 0) idx = 0;
    if (idx > max_theme_index()) idx = 0;
    return idx;
}

// SCR_DrawCrosshair skips Com_sprintf when crosshair slot is 0; keep crosshair_pic_ptr in sync.
void sync_crosshair_pic_path() {
    char* pic = crosshair_pic_ptr_buf();
    if (!pic) return;
    cvar_t* crosshair = detour_Cvar_Get::oCvar_Get("crosshair", "0", 0, nullptr);
    const int theme = theme_index_from_cvar();
    const int slot = clamp_crosshair_slot(crosshair ? crosshair->value : 0.0f);
    if (theme == 0)
        std::snprintf(pic, 64, "pics/menus/status/ch%d", slot);
    else
        std::snprintf(pic, 64, "pics/menus/status/sbxch_%d_%d", theme, slot);
}

void prime_crosshair_slot_zero_dims() {
    if (theme_index_from_cvar() == 0) return;
    cvar_t* crosshair = detour_Cvar_Get::oCvar_Get("crosshair", "0", 0, nullptr);
    if (clamp_crosshair_slot(crosshair ? crosshair->value : 0.0f) != 0) return;
    char* pic = crosshair_pic_ptr_buf();
    if (!pic || !pic[0]) return;
    auto get_size = reinterpret_cast<Draw_GetPicSize_t>(
        rvaToAbsExe(reinterpret_cast<void*>(kDrawGetPicSizeRva)));
    int* w = static_cast<int*>(rvaToAbsExe(reinterpret_cast<void*>(kCrosshairWidthRva)));
    int* h = static_cast<int*>(rvaToAbsExe(reinterpret_cast<void*>(kCrosshairHeightRva)));
    if (!get_size || !w || !h) return;
    get_size(w, h, pic);
}

void invalidate_crosshair_pic() {
    sync_crosshair_pic_path();
    prime_crosshair_slot_zero_dims();
}

void apply_theme_and_slot(int theme, int slot, bool refresh_pic) {
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
    if (refresh_pic) invalidate_crosshair_pic();
}

int crosshair_menu_total() {
    return kCrosshairMenuCount + static_cast<int>(g_external_menu.size());
}

bool crosshair_menu_row(int index, int* theme, int* slot) {
    if (!theme || !slot || index < 0) return false;
    if (index < kCrosshairMenuCount) {
        *theme = kCrosshairMenu[index].theme_index;
        *slot = kCrosshairMenu[index].slot;
        return true;
    }
    index -= kCrosshairMenuCount;
    if (index >= static_cast<int>(g_external_menu.size())) return false;
    *theme = g_external_menu[index].theme;
    *slot = g_external_menu[index].slot;
    return true;
}

int menu_index_for_selection(int theme, int slot) {
    for (int i = 0; i < kCrosshairMenuCount; ++i) {
        if (kCrosshairMenu[i].theme_index == theme && kCrosshairMenu[i].slot == slot) return i;
    }
    for (size_t i = 0; i < g_external_menu.size(); ++i) {
        if (g_external_menu[i].theme == theme && g_external_menu[i].slot == slot)
            return kCrosshairMenuCount + static_cast<int>(i);
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
    std::snprintf(rmf, sizeof(rmf), "<center>\n<image \"%s\" scale 4>\n<hbr>", path);
    detour_Cvar_Set2::oCvar_Set2(const_cast<char*>(kPreviewRmfCvar), rmf, true);
}

void apply_menu_pick(int index, bool refresh_pic) {
    int theme = 0;
    int slot = 0;
    if (!crosshair_menu_row(index, &theme, &slot)) return;
    if (theme < 0 || theme > max_theme_index()) return;
    if (theme == theme_index_from_cvar()) {
        cvar_t* crosshair = detour_Cvar_Get::oCvar_Get("crosshair", "0", 0, nullptr);
        if (crosshair && clamp_crosshair_slot(crosshair->value) == slot) return;
    }
    apply_theme_and_slot(theme, slot, refresh_pic);
    update_preview_cvar();
}

void crosshair_tint_change(cvar_t*) {
    invalidate_crosshair_pic();
}

void apply_legacy_pick(int index) {
    if (index < 0 || index >= kCrosshairLegacyCatalogCount) return;
    const CrosshairLegacyPick& row = kCrosshairLegacyCatalog[index];
    apply_theme_and_slot(row.theme_index, row.slot, true);
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
    more_crosshairs_apply_pick_index(static_cast<int>(cvar->value + 0.5f));
}

void crosshair_theme_change(cvar_t* cvar) {
    if (!cvar || g_suppress_theme_callback) return;
    invalidate_crosshair_pic();
    update_preview_cvar();
}

void crosshair_slot_change(cvar_t* cvar) {
    if (!cvar || g_suppress_slot_callback) return;
    invalidate_crosshair_pic();
    update_preview_cvar();
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

static bool parse_status_ch_slot(const char* filename, int& slot) {
    if (!filename || std::strstr(filename, "sbxch_")) return false;
    const char* s = std::strstr(filename, "status/ch");
    if (!s) return false;
    const char* num = s + 9;
    if (!crosshair_slot_suffix(num)) return false;
    slot = *num - '0';
    return true;
}

bool is_crosshair_menu_image_path(const char* filename) {
    if (!filename) return false;
    if (std::strstr(filename, "sbxch_") != nullptr) return true;
    int slot = 0;
    return parse_status_ch_slot(filename, slot);
}

static void sync_active_buddy_page_with_key_dest() {
    auto* key_dest = static_cast<int*>(rvaToAbsExe(reinterpret_cast<void*>(kClsKeyDestRva)));
    if (!key_dest || *key_dest != kKeyDestMenu) g_active_buddy_page[0] = '\0';
}

static bool on_gameplay_misc_menu() {
    if (!g_crosshairs_cvars_ready || !detour_Cvar_Get::oCvar_Get) return false;
    if (std::strcmp(g_active_buddy_page, "sof_buddy/misc") == 0) return true;
    cvar_t* page = detour_Cvar_Get::oCvar_Get("_sb_internal_menu_last_page", "", 0, nullptr);
    return page && page->string && std::strcmp(page->string, "sof_buddy/misc") == 0;
}

static bool fill_menu_preview_pic_path() {
    if (!on_gameplay_misc_menu()) return false;
    cvar_t* pick = detour_Cvar_Get::oCvar_Get(kPickCvar, "0", 0, nullptr);
    const int idx = pick ? static_cast<int>(pick->value + 0.5f) : 0;
    int theme = 0;
    int slot = 0;
    if (!crosshair_menu_row(idx, &theme, &slot)) return false;
    char rel[72];
    preview_menu_image_path(theme, slot, rel, sizeof(rel));
    std::snprintf(g_menu_preview_pic_path, sizeof(g_menu_preview_pic_path), "pics/menus/%s", rel);
    return true;
}

static void redirect_menu_preview_image(char*& filename) {
    if (!filename || !is_crosshair_menu_image_path(filename)) return;
#if FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE
    if (g_currentPicCaller == PicCaller::SCR_DrawCrosshair) return;
#endif
    if (!fill_menu_preview_pic_path()) return;
    filename = g_menu_preview_pic_path;
}

void crosshair_gl_findimage_pre(char*& filename, int& imagetype, char&, char&) {
    if (!filename) return;
    redirect_menu_preview_image(filename);
    if (on_gameplay_misc_menu() && is_crosshair_menu_image_path(filename)) {
        if (imagetype != kGlImagePic) imagetype = kGlImagePic;
        return;
    }
    char* pic = crosshair_pic_ptr_buf();
    int slot = 0;
    const bool engine_ch = parse_status_ch_slot(filename, slot) || (pic && filename == pic);
    if (pic && engine_ch) {
        sync_crosshair_pic_path();
        if (theme_index_from_cvar() != 0) filename = pic;
    }
    if (!is_crosshair_menu_image_path(filename)) return;
    if (imagetype != kGlImagePic) imagetype = kGlImagePic;
}

void* crosshair_gl_findimage_fixup(void* result, char* filename, int imagetype, char mimap,
                                   char allowPicmip) {
    if (result || !filename || !is_crosshair_menu_image_path(filename)) return result;
    if (imagetype == kGlImagePic) return result;
    if (!detour_GL_FindImage::oGL_FindImage) return result;
    return detour_GL_FindImage::oGL_FindImage(filename, kGlImagePic, mimap, allowPicmip);
}

}  // namespace

void more_crosshairs_apply_pick_index(int index) {
    if (!g_crosshairs_cvars_ready || g_crosshair_menu_refresh_guard) return;
    if (index < 0 || index >= crosshair_menu_total()) return;
    apply_menu_pick(index, false);
    g_defer_crosshair_pic_refresh = true;
}

void more_crosshairs_gl_findimage_pre(char*& filename, int& imagetype, char& mimap, char& allowPicmip) {
    crosshair_gl_findimage_pre(filename, imagetype, mimap, allowPicmip);
}

void* more_crosshairs_gl_findimage_fixup(void* result, char* filename, int imagetype, char mimap,
                                         char allowPicmip) {
    return crosshair_gl_findimage_fixup(result, filename, imagetype, mimap, allowPicmip);
}

const char* more_crosshairs_get_preview_rmf(void) {
    static char rmf[160];
    cvar_t* crosshair = detour_Cvar_Get::oCvar_Get("crosshair", "0", 0, nullptr);
    const int theme = theme_index_from_cvar();
    const int slot = clamp_crosshair_slot(crosshair ? crosshair->value : 0.0f);
    char path[72];
    preview_menu_image_path(theme, slot, path, sizeof(path));
    std::snprintf(rmf, sizeof(rmf), "<center>\n<image \"%s\" scale 4>\n<hbr>", path);
    return rmf;
}

const char* more_crosshairs_get_list_rmf(void) {
    discover_external_crosshair_paks();
    static char labels[512];
    static char match[256];
    static char buf[1400];
    std::snprintf(labels, sizeof(labels), "%s", kCrosshairMenuListLabels);
    for (const ExternalMenuPick& row : g_external_menu) {
        char piece[24];
        std::snprintf(piece, sizeof(piece), ",%s%d", row.abbr, row.slot);
        if (std::strlen(labels) + std::strlen(piece) < sizeof(labels)) std::strcat(labels, piece);
    }
    int mpos = std::snprintf(match, sizeof(match), "%s", kCrosshairMenuListMatch);
    const int total = crosshair_menu_total();
    for (int idx = kCrosshairMenuCount; idx < total && mpos < static_cast<int>(sizeof(match)) - 8; ++idx)
        mpos += std::snprintf(match + mpos, sizeof(match) - mpos, ",%d", idx);
    const int n = std::snprintf(
        buf, sizeof(buf),
        "<list \"%s\" match \"%s\" cvar _sofbuddy_crosshair_pick atext \"Crosshair : \" "
        "noshade tip \"S0-S4 stock; sof_buddy packs; extra crosshair paks in base/gamedir (not pak0-9, not duplicate catalog names).\"><hbr>",
        labels, match);
    if (n < 0 || n >= static_cast<int>(sizeof(buf))) return "<hbr>";
    return buf;
}

void more_crosshairs_refresh_menu_preview(void) {
    g_crosshair_menu_refresh_guard = true;
    sync_pick_cvar_silent();
    update_preview_cvar();
    g_crosshair_menu_refresh_guard = false;
}

void more_crosshairs_set_active_buddy_page(const char* menu_file) {
    g_active_buddy_page[0] = '\0';
    if (!menu_file || !menu_file[0]) return;
    std::string n(menu_file);
    std::replace(n.begin(), n.end(), '\\', '/');
    std::transform(n.begin(), n.end(), n.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    while (n.rfind("./", 0) == 0) n.erase(0, 2);
    while (!n.empty() && n[0] == '/') n.erase(0, 1);
    if (n.rfind("menus/", 0) == 0) n.erase(0, 6);
    if (n.rfind("menu/", 0) == 0) n.erase(0, 5);
    if (n.size() > 4 && n.compare(n.size() - 4, 4, ".rmf") == 0) n.erase(n.size() - 4);
    if (n.rfind("sof_buddy/", 0) != 0) {
        if (n.find('/') == std::string::npos) n = "sof_buddy/" + n;
        else return;
    }
    std::snprintf(g_active_buddy_page, sizeof(g_active_buddy_page), "%s", n.c_str());
}

const char* more_crosshairs_menu_preview_pic_name(const char* imgname) {
    if (!imgname || !is_crosshair_menu_image_path(imgname)) return nullptr;
#if FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE
    if (g_currentPicCaller == PicCaller::SCR_DrawCrosshair) return nullptr;
#endif
    return fill_menu_preview_pic_path() ? g_menu_preview_pic_path : nullptr;
}

void more_crosshairs_pump_deferred_menu_refresh(void) {
    if (!g_crosshairs_cvars_ready || !g_defer_crosshair_pic_refresh) return;
    g_defer_crosshair_pic_refresh = false;
    invalidate_crosshair_pic();
}

void more_crosshairs_sync_engine_pic_path(void) {
    sync_crosshair_pic_path();
}

bool more_crosshairs_is_menu_preview_path(const char* pic) {
    return is_crosshair_menu_image_path(pic);
}

void more_crosshairs_on_getpicsize(char* pic) {
    char* engine = crosshair_pic_ptr_buf();
    if (engine && pic == engine) {
        sync_crosshair_pic_path();
        prime_crosshair_slot_zero_dims();
        return;
    }
    if (!pic || !is_crosshair_menu_image_path(pic)) return;
    if (detour_GL_FindImage::oGL_FindImage)
        detour_GL_FindImage::oGL_FindImage(pic, kGlImagePic, 0, 0);
}

void more_crosshairs_before_draw_crosshair(void) {
    sync_crosshair_pic_path();
    prime_crosshair_slot_zero_dims();
}

void more_crosshairs_apply_stock_tint_gl(void) {
    if (theme_index_from_cvar() != 0) return;
    const int tint = stock_tint_index();
    if (tint == 0) return;
    ensure_gl_color4f();
    if (!g_gl_color4f) return;
    float r, g, b;
    stock_tint_rgb(tint, r, g, b);
    g_gl_color4f(r, g, b, 1.0f);
}

void more_crosshairs_PostCvarInit(void) {
    if (!detour_Cvar_Get::oCvar_Get) return;

    discover_external_crosshair_paks();
    remove_legacy_active_pak();
    detour_Cvar_Get::oCvar_Get(kPreviewRmfCvar, "", 0, nullptr);

    g_suppress_theme_callback = true;
    detour_Cvar_Get::oCvar_Get(kThemeCvar, "0", CVAR_SOFBUDDY_ARCHIVE, crosshair_theme_change);
    g_suppress_theme_callback = false;

    detour_Cvar_Get::oCvar_Get(kPickCvar, "0", CVAR_SOFBUDDY_ARCHIVE, crosshair_pick_change);
    detour_Cvar_Get::oCvar_Get(kTintCvar, "0", CVAR_SOFBUDDY_ARCHIVE, crosshair_tint_change);
    detour_Cvar_Get::oCvar_Get("crosshair", "0", 0, crosshair_slot_change);

    migrate_saved_crosshair();
    sync_pick_cvar_silent();
    sync_crosshair_pic_path();
    more_crosshairs_refresh_menu_preview();
    g_crosshairs_cvars_ready = true;
}

int more_crosshairs_menu_pick_count(void) {
    return crosshair_menu_total();
}

bool more_crosshairs_menu_pick_at(int index, int* theme, int* slot) {
    return crosshair_menu_row(index, theme, slot);
}

bool more_crosshairs_theme_external_pak(int theme, const char** pak_path, const int8_t** slot_map) {
    if (g_external_paks.empty()) discover_external_crosshair_paks();
    if (theme < kCrosshairThemeCount || !pak_path || !slot_map) return false;
    const int idx = theme - kCrosshairThemeCount;
    if (idx < 0 || idx >= static_cast<int>(g_external_paks.size())) return false;
    const ExternalPak& pak = g_external_paks[static_cast<size_t>(idx)];
    *pak_path = pak.path.c_str();
    *slot_map = pak.slot_map;
    return true;
}

#endif
