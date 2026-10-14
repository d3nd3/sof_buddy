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
        directory_length > 64 * 1024 * 1024 ||
        std::fseek(file, static_cast<long>(directory_offset), SEEK_SET) != 0) {
        std::fclose(file);
        return false;
    }
    for (uint32_t pos = 0; pos < directory_length; pos += 64) {
        char name[57] = {};
        uint32_t offset = 0;
        uint32_t length = 0;
        if (std::fread(name, 1, 56, file) != 56 ||
            std::fread(&offset, sizeof(offset), 1, file) != 1 ||
            std::fread(&length, sizeof(length), 1, file) != 1)
            break;
        if (normalize_path(name) != wanted || length == 0 || length > INT_MAX ||
            std::fseek(file, static_cast<long>(offset), SEEK_SET) != 0)
            continue;
        data.resize(length);
        const bool read_ok = std::fread(data.data(), 1, length, file) == length;
        std::fclose(file);
        return read_ok;
    }
    std::fclose(file);
    return false;
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
