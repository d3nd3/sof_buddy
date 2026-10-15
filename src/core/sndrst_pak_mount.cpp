#include "sndrst_pak_mount.h"
#include "generated_detours.h"
#include "util.h"
#include <cstdio>
#include <string>
#include <windows.h>

namespace {

std::string trim_trailing_slashes(std::string path) {
    while (path.size() > 1 && (path.back() == '/' || path.back() == '\\')) path.pop_back();
    return path;
}

}  // namespace

static bool dir_has_pak(const std::string& dir) {
    WIN32_FIND_DATAA entry = {};
    HANDLE search = FindFirstFileA((dir + "/*.pak").c_str(), &entry);
    if (search == INVALID_HANDLE_VALUE) return false;
    FindClose(search);
    return true;
}

void sofbuddy_mount_sndrst_pak(void) {
    if (!detour_FS_AddGameDirectory::oFS_AddGameDirectory) return;

    cvar_t* basedir = findCvar(const_cast<char*>("basedir"));
    const char* root = basedir && basedir->string && basedir->string[0] ? basedir->string : ".";
    const std::string buddy = trim_trailing_slashes(root) + "/sof_buddy";
    if (!dir_has_pak(buddy)) return;

    static char dir[MAX_PATH];
    std::snprintf(dir, sizeof(dir), "%s", buddy.c_str());
    detour_FS_AddGameDirectory::oFS_AddGameDirectory(dir);
    PrintOut(PRINT_DEV, "Mounted SoF Buddy asset paks from %s\n", dir);
}
