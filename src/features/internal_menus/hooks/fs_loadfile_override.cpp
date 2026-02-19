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
#include <cstring>
#include <string>

namespace {

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
    if (!path || !buffer || g_menu_internal_files.empty()) return original(path, buffer, override_pak);

    std::string menu_name = path_to_menu_name(path);
    std::string filename = path_to_filename(path);
    if (menu_name.empty() || filename.empty()) return original(path, buffer, override_pak);

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
    for (std::string::size_type pos = 0; (pos = content.find("{content_inset_tall}", pos)) != std::string::npos; )
        content.replace(pos, 20, inset_tall), pos += std::strlen(inset_tall);
    for (std::string::size_type pos = 0; (pos = content.find("{content_inset}", pos)) != std::string::npos; )
        content.replace(pos, 15, inset), pos += std::strlen(inset);
    for (std::string::size_type pos = 0; (pos = content.find("tall}", pos)) != std::string::npos; )
        content.replace(pos, 5, "");

    const int size = static_cast<int>(content.size() + 1);
    static void* (*Z_Malloc)(int) = nullptr;
    if (!Z_Malloc) Z_Malloc = (void*(*)(int))rvaToAbsExe((void*)0x0001F120);
    if (!Z_Malloc) return original(path, buffer, override_pak);
    void* copy = Z_Malloc(size);
    if (!copy) return original(path, buffer, override_pak);
    std::memcpy(copy, content.c_str(), content.size() + 1);
    *buffer = copy;
    return static_cast<int>(content.size());
}
#endif
