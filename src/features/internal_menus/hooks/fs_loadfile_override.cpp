// Intercepts FS_LoadFile so menu file requests can be served from embedded RMF
// in g_menu_internal_files instead of disk. If the path matches an internal menu,
// we allocate with the engine's Z_Malloc, copy the embedded bytes, and return;
// otherwise we call the original FS_LoadFile.

#include "feature_config.h"
#if FEATURE_INTERNAL_MENUS
#include "sof_compat.h"
#include "../shared.h"
#include "generated_detours.h"
#include "util.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <climits>
#include <windows.h>
#include <string>
#include <vector>

#if FEATURE_MORE_CROSSHAIRS
#include "features/more_crosshairs/shared.h"
#endif
#include "update_command.h"

namespace {

std::string normalize_path(const char* path) {
    if (!path || !path[0]) return "";
    std::string s(path);
    std::replace(s.begin(), s.end(), '\\', '/');
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    while (s.size() >= 2 && s.compare(0, 2, "./") == 0) s.erase(0, 2);
    while (!s.empty() && s[0] == '/') s.erase(0, 1);
    if (s.compare(0, 5, "base/") == 0) s.erase(0, 5);
    return s;
}

bool switchable_font_path(const std::string& path) {
    return path == "pics/menus/fonts/font1.fnt" || path == "pics/menus/fonts/font1.m32" ||
           path == "pics/menus/fonts/font2.fnt" || path == "pics/menus/fonts/font2.m32" ||
           path == "pics/menus/fonts/font4.fnt" || path == "pics/menus/fonts/font4.m32";
}

std::string game_base_dir() {
    cvar_t* basedir = findCvar(const_cast<char*>("basedir"));
    std::string base = basedir && basedir->string && basedir->string[0] ? basedir->string : ".";
    while (base.size() > 1 && (base.back() == '/' || base.back() == '\\')) base.pop_back();
    return base;
}

std::vector<std::string> sofplus_font_paks() {
    std::vector<std::string> result;
    const std::string directory = game_base_dir() + "/base/";
    WIN32_FIND_DATAA entry = {};
    HANDLE search = FindFirstFileA((directory + "*.pak").c_str(), &entry);
    if (search == INVALID_HANDLE_VALUE) return result;
    do {
        if (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        std::string name = entry.cFileName;
        std::string lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (lower.find("sofplus-font") != std::string::npos) result.push_back(directory + name);
    } while (FindNextFileA(search, &entry));
    FindClose(search);
    std::sort(result.begin(), result.end());
    return result;
}

std::vector<std::string> vanilla_font_paks() {
    std::vector<std::string> result;
    const std::string directory = game_base_dir() + "/base/";
    WIN32_FIND_DATAA entry = {};
    HANDLE search = FindFirstFileA((directory + "pak*.pak").c_str(), &entry);
    if (search == INVALID_HANDLE_VALUE) return result;
    do {
        if (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        std::string name = entry.cFileName;
        std::string lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (lower.size() <= 7 || lower.compare(0, 3, "pak") != 0 ||
            lower.compare(lower.size() - 4, 4, ".pak") != 0)
            continue;
        bool numbered = true;
        for (size_t i = 3; i < lower.size() - 4; ++i)
            if (!std::isdigit(static_cast<unsigned char>(lower[i]))) numbered = false;
        if (numbered) result.push_back(directory + name);
    } while (FindNextFileA(search, &entry));
    FindClose(search);
    std::sort(result.begin(), result.end());
    return result;
}

bool try_pak_toc(FILE* file, uint32_t toc_pos, uint32_t directory_length,
                 const std::string& wanted, std::vector<unsigned char>& data) {
    for (uint32_t pos = 0; pos < directory_length; pos += 64) {
        char name[57] = {};
        uint32_t offset = 0;
        uint32_t length = 0;
        if (std::fseek(file, static_cast<long>(toc_pos + pos), SEEK_SET) != 0) return false;
        if (std::fread(name, 1, 56, file) != 56 ||
            std::fread(&offset, sizeof(offset), 1, file) != 1 ||
            std::fread(&length, sizeof(length), 1, file) != 1)
            return false;
        if (normalize_path(name) == wanted && length > 0 && length <= INT_MAX) {
            auto read_at = [&](uint32_t file_off) {
                if (std::fseek(file, static_cast<long>(file_off), SEEK_SET) != 0) return false;
                data.resize(length);
                if (std::fread(data.data(), 1, length, file) != length) return false;
                return data.size() >= 4 && *reinterpret_cast<const uint32_t*>(data.data()) == 4u;
            };
            if (read_at(offset)) return true;
            // Older buddy-built paks stored blob-relative offsets (missing +12 PACK header).
            if (offset < 12u && read_at(offset + 12u)) return true;
        }
    }
    return false;
}

bool load_pak_entry(const std::string& pak_path, const std::string& wanted, std::vector<unsigned char>& data) {
    FILE* file = std::fopen(pak_path.c_str(), "rb");
    if (!file) return false;
    char magic[4] = {};
    uint32_t directory_offset = 0;
    uint32_t directory_length = 0;
    const bool header_ok = std::fread(magic, 1, sizeof(magic), file) == sizeof(magic) &&
                           std::fread(&directory_offset, sizeof(directory_offset), 1, file) == 1 &&
                           std::fread(&directory_length, sizeof(directory_length), 1, file) == 1;
    if (!header_ok || std::memcmp(magic, "PACK", 4) != 0 ||
        directory_length == 0 || directory_length % 64 != 0 ||
        directory_length > 64 * 1024 * 1024) {
        std::fclose(file);
        return false;
    }
    std::fseek(file, 0, SEEK_END);
    const long file_size = std::ftell(file);
    // Older generator builds stored header+directory as the TOC offset instead of
    // header+payloads; the directory then physically sits at the end of the file.
    std::vector<uint32_t> candidates;
    candidates.push_back(directory_offset);
    if (file_size > 0 && static_cast<uint64_t>(file_size) >= directory_length &&
        static_cast<uint64_t>(file_size) - directory_length != directory_offset)
        candidates.push_back(static_cast<uint32_t>(file_size - directory_length));

    bool found = false;
    for (uint32_t toc_pos : candidates) {
        if (try_pak_toc(file, toc_pos, directory_length, wanted, data)) {
            found = true;
            break;
        }
    }
    std::fclose(file);
    return found;
}

int selected_font_mode() {
    cvar_t* pack = findCvar(const_cast<char*>("_sofbuddy_font_pack"));
    if (!pack) return 0;
    const int mode = static_cast<int>(pack->value);
    return mode >= 1 && mode <= 2 ? mode : 0;
}

bool load_selected_font(char* path, void** buffer, int& length) {
    const std::string wanted = normalize_path(path);
    if (!switchable_font_path(wanted)) return false;
    const int mode = selected_font_mode();
    if (mode == 0) return false;
    const bool sofplus = mode == 1;
    std::vector<unsigned char> data;
    const std::vector<std::string> paks = sofplus ? sofplus_font_paks() : vanilla_font_paks();
    for (const std::string& pak : paks)
        if (load_pak_entry(pak, wanted, data)) {
            if (!detour_Z_Malloc::oZ_Malloc) return false;
            void* copy = detour_Z_Malloc::oZ_Malloc(static_cast<int>(data.size() + 1));
            if (!copy) return false;
            std::memcpy(copy, data.data(), data.size());
            static_cast<unsigned char*>(copy)[data.size()] = 0;
            *buffer = copy;
            length = static_cast<int>(data.size());
            static int logged_mode = -1;
            if (logged_mode != mode) {
                PrintOut(PRINT_LOG, "scaled_ui_base: using %s font assets from %s\n",
                         sofplus ? "Force SoFPlus" : "Force Vanilla", pak.c_str());
                logged_mode = mode;
            }
            return true;
        }
    static int warned_mode = -1;
    if (warned_mode != mode) {
        PrintOut(PRINT_BAD, "scaled_ui_base: %s font PAK not found in basedir/base; using the engine font fallback.\n",
                 sofplus ? "SoFPlus" : "vanilla");
        warned_mode = mode;
    }
    return false;
}

#if FEATURE_MORE_CROSSHAIRS
#include "crosshair_catalog.inc"
#include "crosshair_placeholder.inc"

static void collapse_duplicate_pics_menus(std::string& s) {
    static const char kDup[] = "pics/menus/pics/menus/";
    const size_t dup_len = sizeof(kDup) - 1;
    while (s.size() >= dup_len && s.compare(0, dup_len, kDup) == 0) s.erase(0, 11);
}

static std::string crosshair_fs_path(const std::string& wanted) {
    std::string s = wanted;
    collapse_duplicate_pics_menus(s);
    if (s.compare(0, 7, "status/") == 0) return "pics/menus/" + s;
    if (s.compare(0, 13, "menus/status/") == 0) return "pics/" + s;
    return s;
}

static bool parse_preview_crosshair_path(const std::string& wanted, int& theme, int& slot) {
    static const char kPrefix[] = "pics/menus/status/sbxch_";
    const size_t prefix_len = sizeof(kPrefix) - 1;
    if (wanted.size() <= prefix_len || wanted.compare(0, prefix_len, kPrefix) != 0) return false;
    const char* rest = wanted.c_str() + prefix_len;
    char* end = nullptr;
    const long t = std::strtol(rest, &end, 10);
    if (!end || *end != '_') return false;
    const long s = std::strtol(end + 1, &end, 10);
    if (s < 0 || s > 4) return false;
    if (*end != '\0' && std::strcmp(end, ".m32") != 0) return false;
    theme = static_cast<int>(t);
    slot = static_cast<int>(s);
    return true;
}

static void patch_m32_internal_name(std::vector<unsigned char>& data, const char* vpath)
{
	if (data.size() < 60 || !vpath) return;
	*reinterpret_cast<uint32_t*>(data.data()) = 4u;
	std::memset(data.data() + 4, 0, 56);
	std::strncpy(reinterpret_cast<char*>(data.data() + 4), vpath, 55);
}

static std::string gl_image_key_from_load_path(const char* path) {
    std::string s = normalize_path(path);
    if (s.size() >= 4 && s.compare(s.size() - 4, 4, ".m32") == 0) s.resize(s.size() - 4);
    collapse_duplicate_pics_menus(s);
    if (s.compare(0, 7, "status/") == 0) return "pics/menus/" + s;
    if (s.compare(0, 13, "menus/status/") == 0) return "pics/" + s;
    return s;
}

static void crosshair_gl_image_name(const std::string& key, char* out, size_t out_sz) {
	std::snprintf(out, out_sz, "%s", key.c_str());
}

static bool path_is_sof_buddy_gamedir(const std::string& dir) {
    std::string norm = dir;
    std::replace(norm.begin(), norm.end(), '\\', '/');
    std::transform(norm.begin(), norm.end(), norm.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    while (norm.size() > 1 && norm.back() == '/') norm.pop_back();
    std::string buddy = game_base_dir() + "/sof_buddy";
    std::replace(buddy.begin(), buddy.end(), '\\', '/');
    std::transform(buddy.begin(), buddy.end(), buddy.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return norm == buddy;
}

static void stock_crosshair_gamedirs(std::vector<std::string>& dirs) {
    const std::string root = game_base_dir();
    cvar_t* gd = findCvar(const_cast<char*>("gamedir"));
    const char* rel = gd && gd->string && gd->string[0] ? gd->string : "base";
    const std::string primary = root + "/" + rel;
    const std::string base = root + "/base";
    if (!path_is_sof_buddy_gamedir(primary)) dirs.push_back(primary);
    if (primary != base && !path_is_sof_buddy_gamedir(base)) dirs.push_back(base);
}

static void append_stock_paks_only(const std::string& dir, std::vector<std::string>& out) {
    auto already = [&](const std::string& p) {
        return std::find(out.begin(), out.end(), p) != out.end();
    };
    for (int i = 9; i >= 0; --i) {
        char path[MAX_PATH];
        std::snprintf(path, sizeof(path), "%s/pak%d.pak", dir.c_str(), i);
        if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES && !already(path)) out.push_back(path);
    }
}

static bool copy_crosshair_m32(std::vector<unsigned char>& data, const char* gl_image_name, void** buffer,
                               int& length) {
    patch_m32_internal_name(data, gl_image_name);
    if (!detour_Z_Malloc::oZ_Malloc) return false;
    void* copy = detour_Z_Malloc::oZ_Malloc(static_cast<int>(data.size() + 1));
    if (!copy) return false;
    std::memcpy(copy, data.data(), data.size());
    static_cast<unsigned char*>(copy)[data.size()] = 0;
    *buffer = copy;
    length = static_cast<int>(data.size());
    return true;
}

static bool serve_stock_standard_crosshair(int slot, const char* gl_image_name, void** buffer, int& length) {
    char wanted[64];
    std::snprintf(wanted, sizeof(wanted), "pics/menus/status/ch%d.m32", slot);
    const std::string wanted_norm = normalize_path(wanted);
    std::vector<std::string> dirs;
    stock_crosshair_gamedirs(dirs);
    std::vector<unsigned char> data;
    for (const std::string& dir : dirs) {
        const std::string loose = dir + "/" + wanted_norm;
        FILE* file = std::fopen(loose.c_str(), "rb");
        if (file) {
            std::fseek(file, 0, SEEK_END);
            const long sz = std::ftell(file);
            if (sz > 0 && sz <= INT_MAX) {
                std::fseek(file, 0, SEEK_SET);
                data.resize(static_cast<size_t>(sz));
                if (std::fread(data.data(), 1, data.size(), file) == data.size() &&
                    copy_crosshair_m32(data, gl_image_name, buffer, length)) {
                    std::fclose(file);
                    return true;
                }
            }
            std::fclose(file);
            data.clear();
        }
        std::vector<std::string> paks;
        append_stock_paks_only(dir, paks);
        for (const std::string& pak : paks) {
            if (load_pak_entry(pak, wanted_norm, data) && copy_crosshair_m32(data, gl_image_name, buffer, length))
                return true;
            data.clear();
        }
    }
    return false;
}

static bool serve_crosshair_placeholder(const char* gl_image_name, void** buffer, int& length) {
    if (kCrosshairNoneM32Size < 60 || !gl_image_name) return false;
    std::vector<unsigned char> data(kCrosshairNoneM32, kCrosshairNoneM32 + kCrosshairNoneM32Size);
    patch_m32_internal_name(data, gl_image_name);
    if (!detour_Z_Malloc::oZ_Malloc) return false;
    void* copy = detour_Z_Malloc::oZ_Malloc(static_cast<int>(data.size() + 1));
    if (!copy) return false;
    std::memcpy(copy, data.data(), data.size());
    static_cast<unsigned char*>(copy)[data.size()] = 0;
    *buffer = copy;
    length = static_cast<int>(data.size());
    return true;
}

static bool serve_crosshair_pack(int theme, int slot, const char* gl_image_name, void** buffer,
                                 int& length) {
    if (slot < 0 || slot > 4) return false;
    const char* pak = nullptr;
    const int8_t* ext_slots = nullptr;
    if (more_crosshairs_theme_external_pak(theme, &pak, &ext_slots)) {
        const int pak_slot = ext_slots ? ext_slots[slot] : -1;
        if (!pak || !pak[0] || pak_slot < 0) return serve_crosshair_placeholder(gl_image_name, buffer, length);
        char wanted_entry[64];
        std::snprintf(wanted_entry, sizeof(wanted_entry), "pics/menus/status/ch%d.m32", pak_slot);
        std::vector<unsigned char> data;
        if (!load_pak_entry(pak, wanted_entry, data) || data.empty())
            return serve_crosshair_placeholder(gl_image_name, buffer, length);
        return copy_crosshair_m32(data, gl_image_name, buffer, length);
    }
    if (theme == 0) return serve_stock_standard_crosshair(slot, gl_image_name, buffer, length);
    if (theme < 0 || theme >= kCrosshairThemeCount) theme = 0;
    const CrosshairThemeEntry& entry = kCrosshairThemes[theme];
    if (!entry.pak_relpath || !entry.pak_relpath[0]) {
        if (theme != 0) return serve_crosshair_placeholder(gl_image_name, buffer, length);
        return false;
    }

    const int pak_slot = kCrosshairThemePakSlot[theme][slot];
    if (pak_slot < 0) return serve_crosshair_placeholder(gl_image_name, buffer, length);

    const std::string pak_path = game_base_dir() + "/sof_buddy/" + entry.pak_relpath;
    char wanted_entry[64];
    std::snprintf(wanted_entry, sizeof(wanted_entry), "pics/menus/status/ch%d.m32", pak_slot);
    std::vector<unsigned char> data;
    if (!load_pak_entry(pak_path, wanted_entry, data) || data.empty())
        return serve_crosshair_placeholder(gl_image_name, buffer, length);
    patch_m32_internal_name(data, gl_image_name);
    if (!detour_Z_Malloc::oZ_Malloc) return false;
    void* copy = detour_Z_Malloc::oZ_Malloc(static_cast<int>(data.size() + 1));
    if (!copy) return false;
    std::memcpy(copy, data.data(), data.size());
    static_cast<unsigned char*>(copy)[data.size()] = 0;
    *buffer = copy;
    length = static_cast<int>(data.size());
    return true;
}

// Theme packs: engine `crosshair` stays 0..4; `_sofbuddy_crosshair_theme` picks the
// pack. Menu preview uses pics/menus/status/sbxch_{theme}_{slot} so GL image cache
// refreshes when the theme changes.
bool load_crosshair_art(char* path, void** buffer, int& length) {
    if (!path) return false;
    const std::string gl_key = gl_image_key_from_load_path(path);
    const std::string wanted = crosshair_fs_path(normalize_path(path));
    char gl_name[72];
    crosshair_gl_image_name(gl_key, gl_name, sizeof(gl_name));

    int theme = 0;
    int slot = 0;
    if (parse_preview_crosshair_path(wanted, theme, slot))
        return serve_crosshair_pack(theme, slot, gl_name, buffer, length);

    static const char kPrefix[] = "pics/menus/status/ch";
    const size_t prefix_len = sizeof(kPrefix) - 1;
    if (wanted.size() <= prefix_len || std::memcmp(wanted.c_str(), kPrefix, prefix_len) != 0)
        return false;

    const char* num = wanted.c_str() + prefix_len;
    if (!std::isdigit(static_cast<unsigned char>(*num))) return false;
    char* end = nullptr;
    const long value = std::strtol(num, &end, 10);
    const size_t value_digits = static_cast<size_t>(end - num);
    const char* tail = wanted.c_str() + prefix_len + value_digits;
    const bool plain = *tail == '\0';
    const bool m32 = std::strcmp(tail, ".m32") == 0;
    if (!plain && !m32) return false;
    if (value < 0 || value > 4) return false;
    if (detour_Cvar_Get::oCvar_Get) {
        cvar_t* theme_cvar = detour_Cvar_Get::oCvar_Get("_sofbuddy_crosshair_theme", "0", 0, nullptr);
        if (theme_cvar) theme = static_cast<int>(theme_cvar->value + 0.5f);
    }
    if (theme != 0) return false;
    return serve_stock_standard_crosshair(static_cast<int>(value), gl_name, buffer, length);
}
#endif

// Normalize path to a key: lowercase, forward slashes, strip "menus/", "menu/", ".rmf".
std::string path_to_menu_key(const char* path) {
    if (!path || !path[0]) return "";
    std::string s(path);
    std::replace(s.begin(), s.end(), '\\', '/');
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    while (s.size() >= 2 && s.compare(0, 2, "./") == 0) s.erase(0, 2);
    while (!s.empty() && s[0] == '/') s.erase(0, 1);
    if (s.size() >= 6 && s.compare(0, 6, "menus/") == 0) s.erase(0, 6);
    if (s.size() >= 5 && s.compare(0, 5, "menu/") == 0) s.erase(0, 5);
    if (s.size() > 4 && s.compare(s.size() - 4, 4, ".rmf") == 0) s.erase(s.size() - 4);
    return s;
}

// From path get the filename we use as key in g_menu_internal_files[menu_name], e.g. "loading.rmf".
std::string path_to_filename(const char* path) {
    std::string key = path_to_menu_key(path);
    if (key.empty()) return "";
    size_t slash = key.rfind('/');
    std::string stem = (slash == std::string::npos) ? key : key.substr(slash + 1);
    return stem + ".rmf";
}

// From path get the menu name (first part before last slash), e.g. "loading" from "menus/loading/loading.rmf".
std::string path_to_menu_name(const char* path) {
    std::string key = path_to_menu_key(path);
    if (key.empty()) return "";
    size_t slash = key.rfind('/');
    return (slash == std::string::npos) ? key : key.substr(0, slash);
}

static void trim_inplace(std::string& s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
}

// Engine includecvar runs at page-cache time; expand when serving embedded menus so notes/lists stay current.
static void expand_rmf_includecvars(std::string& content) {
    if (!detour_Cvar_Get::oCvar_Get) return;
    static const char kTag[] = "<includecvar ";
    const size_t tag_len = sizeof(kTag) - 1;
    for (;;) {
        const size_t pos = content.find(kTag);
        if (pos == std::string::npos) break;
        size_t name_start = pos + tag_len;
        const size_t end = content.find('>', name_start);
        if (end == std::string::npos) break;
        std::string name = content.substr(name_start, end - name_start);
        trim_inplace(name);
        std::string insert;
        if (!name.empty()) {
            cvar_t* cv = detour_Cvar_Get::oCvar_Get(name.c_str(), "", 0, nullptr);
            if (cv && cv->string) insert = cv->string;
        }
        content.replace(pos, end - pos + 1, insert);
    }
}
} // namespace

int internal_menus_fs_loadfile_override_callback(char* path, void** buffer, bool override_pak, detour_FS_LoadFile::tFS_LoadFile original) {
    if (!path || !buffer) return original(path, buffer, override_pak);
    int font_length = 0;
    if (load_selected_font(path, buffer, font_length)) return font_length;
    int crosshair_length = 0;
    if (load_crosshair_art(path, buffer, crosshair_length)) return crosshair_length;
    if (g_menu_internal_files.empty()) return original(path, buffer, override_pak);

    std::string menu_name = path_to_menu_name(path);
    std::string filename = path_to_filename(path);
    if (menu_name.empty() || filename.empty()) return original(path, buffer, override_pak);

    // SP (deathmatch==0): vanilla pak loading.rmf — same rule as stock <exinclude deathmatch> in loading.rmf.
    if (menu_name == "loading" && internal_menus_use_vanilla_loading_menu())
        return original(path, buffer, override_pak);

    // Look up in embedded menu map; if missing, let the engine load from disk/pak.
    auto menu_it = g_menu_internal_files.find(menu_name);
    if (menu_it == g_menu_internal_files.end()) return original(path, buffer, override_pak);
    auto file_it = menu_it->second.find(filename);
    if (file_it == menu_it->second.end()) return original(path, buffer, override_pak);
    const std::vector<uint8_t>& vec = file_it->second;
    if (vec.empty()) return original(path, buffer, override_pak);

    std::string content(vec.begin(), vec.end());
    content.erase(std::remove(content.begin(), content.end(), '\r'), content.end());
    const char* inset = internal_menus_get_content_inset_rmf();
    const char* inset_tall = internal_menus_get_content_inset_tall_rmf();
    const char* tabs_prefix = internal_menus_get_tabs_row_prefix_rmf();
    const char* theme_tints = internal_menus_get_theme_tints_rmf();
    const char* theme_list = internal_menus_get_theme_list_rmf();
    for (std::string::size_type pos = 0; (pos = content.find("{content_inset_tall}", pos)) != std::string::npos; )
        content.replace(pos, 20, inset_tall), pos += std::strlen(inset_tall);
    for (std::string::size_type pos = 0; (pos = content.find("{content_inset}", pos)) != std::string::npos; )
        content.replace(pos, 15, inset), pos += std::strlen(inset);
    for (std::string::size_type pos = 0; (pos = content.find("{tabs_row_prefix}", pos)) != std::string::npos; )
        content.replace(pos, 17, tabs_prefix), pos += std::strlen(tabs_prefix);
    for (std::string::size_type pos = 0; (pos = content.find("{theme_tints}", pos)) != std::string::npos; )
        content.replace(pos, 13, theme_tints), pos += std::strlen(theme_tints);
    for (std::string::size_type pos = 0; (pos = content.find("{menu_theme_list}", pos)) != std::string::npos; )
        content.replace(pos, 17, theme_list), pos += std::strlen(theme_list);
    const char* update_notes = sofbuddy_update_prompt_notes_rmf();
    for (std::string::size_type pos = 0; (pos = content.find("{update_prompt_notes}", pos)) != std::string::npos; )
        content.replace(pos, 21, update_notes ? update_notes : ""), pos += std::strlen(update_notes ? update_notes : "");
#if FEATURE_MORE_CROSSHAIRS
    const char* crosshair_list = ::more_crosshairs_get_list_rmf();
    for (std::string::size_type pos = 0; (pos = content.find("{crosshair_list}", pos)) != std::string::npos; )
        content.replace(pos, 16, crosshair_list), pos += std::strlen(crosshair_list);
    const char* crosshair_preview = ::more_crosshairs_get_preview_rmf();
    for (std::string::size_type pos = 0; (pos = content.find("{crosshair_preview}", pos)) != std::string::npos; )
        content.replace(pos, 19, crosshair_preview), pos += std::strlen(crosshair_preview);
#endif
    for (std::string::size_type pos = 0; (pos = content.find("tall}", pos)) != std::string::npos; )
        content.replace(pos, 5, "");
    expand_rmf_includecvars(content);

    const int size = static_cast<int>(content.size() + 1);
    if (!detour_Z_Malloc::oZ_Malloc) return original(path, buffer, override_pak);
    void* copy = detour_Z_Malloc::oZ_Malloc(size);
    if (!copy) return original(path, buffer, override_pak);
    std::memcpy(copy, content.c_str(), content.size() + 1);
    *buffer = copy;
    return static_cast<int>(content.size());
}
#endif
