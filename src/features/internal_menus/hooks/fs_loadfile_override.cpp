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
        if (normalize_path(name) == wanted && length > 0 && length <= INT_MAX &&
            std::fseek(file, static_cast<long>(offset), SEEK_SET) == 0) {
            data.resize(length);
            return std::fread(data.data(), 1, length, file) == length;
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

static bool serve_crosshair_pack(int theme, int slot, void** buffer, int& length) {
    if (slot < 0 || slot > 4) return false;
    if (theme < 0 || theme >= kCrosshairThemeCount) theme = 0;
    const CrosshairThemeEntry& entry = kCrosshairThemes[theme];
    if (!entry.pak_relpath || !entry.pak_relpath[0]) return false;

    const std::string pak_path = game_base_dir() + "/sof_buddy/" + entry.pak_relpath;
    char wanted_entry[64];
    std::snprintf(wanted_entry, sizeof(wanted_entry), "pics/menus/status/ch%d.m32", slot);
    std::vector<unsigned char> data;
    if (!load_pak_entry(pak_path, wanted_entry, data) || data.empty()) return false;
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
    const std::string wanted = normalize_path(path);

    int theme = 0;
    int slot = 0;
    if (parse_preview_crosshair_path(wanted, theme, slot))
        return serve_crosshair_pack(theme, slot, buffer, length);

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
    return serve_crosshair_pack(theme, static_cast<int>(value), buffer, length);
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
    for (std::string::size_type pos = 0; (pos = content.find("tall}", pos)) != std::string::npos; )
        content.replace(pos, 5, "");

    const int size = static_cast<int>(content.size() + 1);
    if (!detour_Z_Malloc::oZ_Malloc) return original(path, buffer, override_pak);
    void* copy = detour_Z_Malloc::oZ_Malloc(size);
    if (!copy) return original(path, buffer, override_pak);
    std::memcpy(copy, content.c_str(), content.size() + 1);
    *buffer = copy;
    return static_cast<int>(content.size());
}
#endif
