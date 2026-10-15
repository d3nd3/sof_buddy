#include "sndrst_pak_mount.h"
#include "generated_detours.h"
#include "util.h"
#include <cctype>
#include <cstdio>
#include <string>
#include <windows.h>

namespace {

std::string trim_trailing_slashes(std::string path) {
    while (path.size() > 1 && (path.back() == '/' || path.back() == '\\')) path.pop_back();
    return path;
}

void normalize_path_for_compare(std::string& path) {
    for (char& c : path) {
        if (c == '\\') c = '/';
        else c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    while (path.size() >= 2 && path.compare(0, 2, "./") == 0) path.erase(0, 2);
}

bool paths_equal_insensitive(const char* a, const char* b) {
    if (!a || !b) return false;
    std::string left(a);
    std::string right(b);
    normalize_path_for_compare(left);
    normalize_path_for_compare(right);
    return left == right;
}

bool path_is_basedir_base(const char* dir) {
    cvar_t* basedir = findCvar(const_cast<char*>("basedir"));
    if (!basedir || !basedir->string || !basedir->string[0] || !dir) return false;
    char expected[MAX_PATH];
    std::snprintf(expected, sizeof(expected), "%s/base", basedir->string);
    return paths_equal_insensitive(dir, expected);
}

bool dir_has_pak(const std::string& dir) {
    WIN32_FIND_DATAA entry = {};
    HANDLE search = FindFirstFileA((dir + "/*.pak").c_str(), &entry);
    if (search == INVALID_HANDLE_VALUE) return false;
    FindClose(search);
    return true;
}

bool g_buddy_assets_mounted = false;

}  // namespace

void sofbuddy_mount_sndrst_pak(void) {
    if (g_buddy_assets_mounted) return;
    if (!detour_FS_AddGameDirectory::oFS_AddGameDirectory) return;

    cvar_t* basedir = findCvar(const_cast<char*>("basedir"));
    const char* root = basedir && basedir->string && basedir->string[0] ? basedir->string : ".";
    const std::string buddy = trim_trailing_slashes(root) + "/sof_buddy";
    if (!dir_has_pak(buddy)) return;

    static char dir[MAX_PATH];
    std::snprintf(dir, sizeof(dir), "%s", buddy.c_str());
    detour_FS_AddGameDirectory::oFS_AddGameDirectory(dir);
    g_buddy_assets_mounted = true;
    PrintOut(PRINT_DEV, "Mounted SoF Buddy asset paks from %s\n", dir);
}

void fs_addgamedirectory_override_callback(char* dir,
                                           detour_FS_AddGameDirectory::tFS_AddGameDirectory original) {
    if (original) {
        original(dir);
    }
    // Mount after <basedir>/base so user (-game) stay above buddy on the search path.
    if (path_is_basedir_base(dir)) {
        sofbuddy_mount_sndrst_pak();
    }
}
