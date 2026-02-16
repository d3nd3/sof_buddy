#include "feature_config.h"

#if FEATURE_INTERNAL_MENUS

#include "sof_compat.h"
#include "util.h"
#include "shared.h"
#include "generated_detours.h"

#include <windows.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

std::map<std::string, std::map<std::string, std::vector<uint8_t>>> g_menu_internal_files;
cvar_t* _sofbuddy_menu_internal = nullptr;
bool g_sofbuddy_menu_calling = false;
bool g_internal_menus_external_loading_push = false;

int __cdecl hk_FS_LoadFile(char* path, void** buffer);

namespace {

constexpr uintptr_t kFSLoadFileCallsites[] = {
    0x000EAD19,
    0x000EAD40,
    0x000EAF92,
};

using FS_LoadFile_t = int (__cdecl *)(char* path, void** buffer);
using Z_Malloc_t = void* (__cdecl *)(int size);

FS_LoadFile_t orig_FS_LoadFile = nullptr;

Z_Malloc_t get_Z_Malloc() {
    return reinterpret_cast<Z_Malloc_t>(rvaToAbsExe((void*)0x0001F120));
}

std::string normalize_path(const char* path) {
    std::string out = path ? path : "";
    std::replace(out.begin(), out.end(), '\\', '/');
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

std::string filename_from_path(const std::string& normalized_path) {
    const size_t slash = normalized_path.rfind('/');
    std::string name = (slash == std::string::npos) ? normalized_path : normalized_path.substr(slash + 1);
    if (name.find('.') == std::string::npos) name += ".rmf";
    return name;
}

std::string parent_from_path(const std::string& normalized_path) {
    const size_t slash = normalized_path.rfind('/');
    if (slash == std::string::npos) return "";
    const std::string dir = normalized_path.substr(0, slash);
    const size_t prev = dir.rfind('/');
    return (prev == std::string::npos) ? dir : dir.substr(prev + 1);
}

std::string stem_from_filename(const std::string& filename) {
    const size_t dot = filename.find('.');
    return (dot == std::string::npos) ? filename : filename.substr(0, dot);
}

bool is_loading_filename(const std::string& filename) {
    return filename == "loading.rmf" ||
           filename == "loading_header.rmf" ||
           filename == "loading_status.rmf" ||
           filename == "loading_files.rmf" ||
           filename == "loading_recent.rmf";
}

const std::vector<uint8_t>* find_loading_asset(const std::string& filename) {
    auto menu_it = g_menu_internal_files.find("loading");
    if (menu_it == g_menu_internal_files.end()) return nullptr;

    auto file_it = menu_it->second.find(filename);
    if (file_it != menu_it->second.end()) return &file_it->second;

    // Allow unknown loading sub-requests to resolve to the main loading page.
    file_it = menu_it->second.find("loading.rmf");
    return (file_it != menu_it->second.end()) ? &file_it->second : nullptr;
}

const std::vector<uint8_t>* find_internal_asset(const std::string& normalized_path, const std::string& filename) {
    if (!_sofbuddy_menu_internal || _sofbuddy_menu_internal->value == 0.0f) return nullptr;

    const std::string parent = parent_from_path(normalized_path);
    const std::string menu_name = (parent.empty() || parent == "menu") ? stem_from_filename(filename) : parent;

    auto menu_it = g_menu_internal_files.find(menu_name);
    if (menu_it == g_menu_internal_files.end()) return nullptr;

    auto file_it = menu_it->second.find(filename);
    if (file_it == menu_it->second.end()) return nullptr;
    return &file_it->second;
}

int serve_embedded_bytes(const std::vector<uint8_t>& bytes, void** out_buffer) {
    if (bytes.empty()) {
        if (out_buffer) *out_buffer = nullptr;
        return 0;
    }

    void* mem = get_Z_Malloc()(static_cast<int>(bytes.size() + 1));
    if (!mem) return -1;

    std::memcpy(mem, bytes.data(), bytes.size());
    static_cast<char*>(mem)[bytes.size()] = '\0';
    if (out_buffer) *out_buffer = mem;
    return static_cast<int>(bytes.size());
}

int call_original_fs(char* path, void** buffer) {
    return orig_FS_LoadFile ? orig_FS_LoadFile(path, buffer) : -1;
}

void set_menu_internal_flag(bool enabled) {
    if (!orig_Cvar_Set2 || !_sofbuddy_menu_internal) return;
    orig_Cvar_Set2(const_cast<char*>("sofbuddy_menu_internal"), const_cast<char*>(enabled ? "1" : "0"), true);
}

class ScopedInternalMenuPush {
public:
    ScopedInternalMenuPush() {
        g_sofbuddy_menu_calling = true;
        set_menu_internal_flag(true);
    }

    ~ScopedInternalMenuPush() {
        set_menu_internal_flag(false);
        g_sofbuddy_menu_calling = false;
    }
};

bool patch_fs_callsite(uintptr_t rva) {
    void* callsite = rvaToAbsExe((void*)rva);
    auto* bytes = static_cast<unsigned char*>(callsite);
    if (bytes[0] != 0xE8) {
        PrintOut(PRINT_BAD, "Internal menus: FS_LoadFile callsite 0x%p is not E8\n", callsite);
        return false;
    }

    if (!orig_FS_LoadFile) {
        const int rel = *reinterpret_cast<int*>(bytes + 1);
        orig_FS_LoadFile = reinterpret_cast<FS_LoadFile_t>(bytes + 5 + rel);
    }

    WriteE8Call(callsite, reinterpret_cast<void*>(&hk_FS_LoadFile));
    return true;
}

void create_loading_cvars() {
    if (!orig_Cvar_Get) return;

    // Runtime UI state only. Do not persist to sofbuddy.cfg.
    constexpr int kLoadingCvarFlags = 0;

    orig_Cvar_Get("_sofbuddy_loading_progress", "0", kLoadingCvarFlags, nullptr);
    orig_Cvar_Get("_sofbuddy_loading_current", "", kLoadingCvarFlags, nullptr);
    orig_Cvar_Get("_sofbuddy_tab", "0", 0, nullptr);

    for (int i = 1; i <= 5; ++i) {
        char name[48];
        std::snprintf(name, sizeof(name), "_sofbuddy_loading_history_%d", i);
        orig_Cvar_Get(name, "", kLoadingCvarFlags, nullptr);
    }

    for (int i = 1; i <= 4; ++i) {
        char name[48];
        std::snprintf(name, sizeof(name), "_sofbuddy_loading_status_%d", i);
        orig_Cvar_Get(name, "", kLoadingCvarFlags, nullptr);
    }

    for (int i = 1; i <= 8; ++i) {
        char name[48];
        std::snprintf(name, sizeof(name), "_sofbuddy_loading_file_%d", i);
        orig_Cvar_Get(name, "", kLoadingCvarFlags, nullptr);
    }
}

void push_rolling_text_cvars(const char* base_name, int count, const char* newest) {
    if (!orig_Cvar_Get || !orig_Cvar_Set2 || !base_name || count <= 0 || !newest) return;

    std::vector<std::string> prev_values(static_cast<size_t>(count));
    char names[16][64] = {};
    if (count > 16) count = 16;

    for (int i = 0; i < count; ++i) {
        std::snprintf(names[i], sizeof(names[i]), "%s%d", base_name, i + 1);
        cvar_t* c = orig_Cvar_Get(names[i], "", 0, nullptr);
        prev_values[static_cast<size_t>(i)] = (c && c->string) ? c->string : "";
    }

    for (int i = count - 1; i > 0; --i)
        orig_Cvar_Set2(names[i], const_cast<char*>(prev_values[static_cast<size_t>(i - 1)].c_str()), true);

    orig_Cvar_Set2(names[0], const_cast<char*>(newest), true);
}

} // namespace

int __cdecl hk_FS_LoadFile(char* path, void** buffer) {
    const std::string normalized = normalize_path(path);
    const std::string filename = filename_from_path(normalized);

    const std::vector<uint8_t>* bytes = nullptr;
    if (is_loading_filename(filename)) {
        bytes = find_loading_asset(filename);
    }
    if (!bytes) {
        bytes = find_internal_asset(normalized, filename);
    }

    if (!bytes) return call_original_fs(path, buffer);

    const int ret = serve_embedded_bytes(*bytes, buffer);
    return (ret >= 0) ? ret : call_original_fs(path, buffer);
}

void internal_menus_EarlyStartup(void) {
    
    int patched = 0;
    for (const uintptr_t rva : kFSLoadFileCallsites)
        patched += patch_fs_callsite(rva) ? 1 : 0;

    internal_menus_load_library();
    PrintOut(PRINT_LOG, "Internal menus: FS_LoadFile hooks ready (%d/%d, orig=%p)\n",
        patched, static_cast<int>(sizeof(kFSLoadFileCallsites) / sizeof(kFSLoadFileCallsites[0])),
        reinterpret_cast<void*>(orig_FS_LoadFile));
}

void internal_menus_PostCvarInit(void) {
    
    if (!orig_Cvar_Get) {
        PrintOut(PRINT_BAD, "Internal menus: missing Cvar_Get in PostCvarInit\n");
        return;
    }

    _sofbuddy_menu_internal = orig_Cvar_Get("sofbuddy_menu_internal", "0", 0, nullptr);
    create_loading_cvars();

    if (!orig_Cmd_AddCommand) {
        PrintOut(PRINT_BAD, "Internal menus: missing Cmd_AddCommand, cannot register sofbuddy_menu\n");
        return;
    }

    orig_Cmd_AddCommand(const_cast<char*>("sofbuddy_menu"), Cmd_SoFBuddy_Menu_f);

    if (orig_Cmd_ExecuteString) {
        orig_Cmd_ExecuteString("bind F12 \"sofbuddy_menu sof_buddy\"");
        PrintOut(PRINT_GOOD, "Internal menus: bound F12 to sofbuddy_menu sof_buddy\n");
    }
}

void Cmd_SoFBuddy_Menu_f(void) {
    if (!orig_Cmd_Argv || !orig_Cmd_Argc || !detour_M_PushMenu::oM_PushMenu) return;
    if (orig_Cmd_Argc() < 2) return;

    const char* name = orig_Cmd_Argv(1);
    if (!name || !name[0]) return;

    std::string requested(name);
    std::replace(requested.begin(), requested.end(), '\\', '/');
    if (requested.find("..") != std::string::npos) return;

    bool exists = false;
    std::string menu_to_push = requested;

    const size_t slash = requested.rfind('/');
    if (slash == std::string::npos) {
        auto menu_it = g_menu_internal_files.find(requested);
        if (menu_it != g_menu_internal_files.end()) {
            std::string default_file = requested;
            if (default_file.find('.') == std::string::npos) default_file += ".rmf";

            auto default_it = menu_it->second.find(default_file);
            if (default_it != menu_it->second.end()) {
                exists = true;
            } else {
                auto sb_main_it = menu_it->second.find("sb_main.rmf");
                if (sb_main_it != menu_it->second.end()) {
                    exists = true;
                    menu_to_push = requested + "/sb_main";
                }
            }
        }
    } else {
        const std::string menu_name = requested.substr(0, slash);
        std::string file_name = requested.substr(slash + 1);
        if (menu_name.empty() || file_name.empty()) return;
        if (file_name.find('.') == std::string::npos) file_name += ".rmf";

        auto menu_it = g_menu_internal_files.find(menu_name);
        if (menu_it != g_menu_internal_files.end())
            exists = (menu_it->second.find(file_name) != menu_it->second.end());
    }

    if (!exists) return;

    ScopedInternalMenuPush scoped_internal_call;
    detour_M_PushMenu::oM_PushMenu(menu_to_push.c_str(), "", false);
}

void loading_push_history(const char* map_name) {
    if (!map_name || !map_name[0]) return;
    push_rolling_text_cvars("_sofbuddy_loading_history_", 5, map_name);
}

void loading_set_current(const char* map_name) {
    if (!map_name || !orig_Cvar_Set2) return;
    orig_Cvar_Set2(const_cast<char*>("_sofbuddy_loading_current"), const_cast<char*>(map_name), true);
}

void loading_push_status(const char* msg) {
    if (!msg || !msg[0]) return;
    PrintOut(PRINT_GOOD, "%s\n", msg);
    push_rolling_text_cvars("_sofbuddy_loading_status_", 4, msg);
}

void loading_set_files(const char* const* filenames, int count) {
    if (!orig_Cvar_Set2) return;

    for (int i = 1; i <= 8; ++i) {
        char name[48];
        std::snprintf(name, sizeof(name), "_sofbuddy_loading_file_%d", i);
        const char* value = (filenames && i <= count && filenames[i - 1]) ? filenames[i - 1] : "";
        orig_Cvar_Set2(name, const_cast<char*>(value), true);
    }
}

#endif
