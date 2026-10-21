#include "sofbuddy_cfg.h"

#include "sof_compat.h"
#include "util.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr const char* kCfgRelativePath = "/base/sofbuddy.cfg";

// SoFPlus menu settings file owner (see spf_cl_menu_settings_save in SoFPlus:
// it runs this exact four-cvar save command). SoFPlus loads
// sofplus/data/spm_settings.cfg at startup via sofplus_cl_init.
constexpr char kSoFPlusMasterCvar[] = "_sp_cl_master_gamespy";
constexpr char kSoFPlusCpuCoolCvar[] = "_sp_cl_cpu_cool";
constexpr char kSoFPlusSettingsSaveCmd[] =
    "sp_sc_cvar_save spm_settings.cfg cl_maxfps cl_showfps _sp_cl_cpu_cool _sp_cl_master_gamespy";

bool sofplus_present() {
    return findCvar(const_cast<char*>(kSoFPlusMasterCvar)) != nullptr ||
           findCvar(const_cast<char*>(kSoFPlusCpuCoolCvar)) != nullptr;
}

// spm_settings.cfg follows USER-initiated changes. Programmatic writers
// (e.g. master_server_fallback's bootstrap fixer) raise the counter so
// their cvar writes don't overwrite the file with non-user values.
int g_spm_save_suppress_depth = 0;

// Saves to spm_settings.cfg are gated until after Qcommon_Init's FINAL
// Cbuf_Execute: Cbuf_AddLateCommands only APPENDS "+command" lines (verified in
// SoF.exe 0x20018740), so SoFPlus's queued "exec sofplus/sofplus.cfg" (which
// restores spm_settings.cfg) runs in the Cbuf_Execute that follows our
// PostCvarInit hook. Any save made before that flush would rewrite the very
// file the restore is about to read, clobbering the user's values.
bool g_spm_saves_ready = false;

void save_sofplus_spm_settings() {
    if (!detour_Cmd_ExecuteString::oCmd_ExecuteString) {
        return;
    }
    // Guard: vanilla SoF has no _sp_cl_* cvars and no sp_sc_cvar_save command.
    if (!sofplus_present()) {
        return;
    }
    if (g_spm_save_suppress_depth > 0) {
        return;
    }
    if (!g_spm_saves_ready) {
        return;
    }
    detour_Cmd_ExecuteString::oCmd_ExecuteString(kSoFPlusSettingsSaveCmd);
}

// Shutdown flush: the restore has long finished, live values are authoritative.
void save_sofplus_spm_settings_on_exit() {
    if (!detour_Cmd_ExecuteString::oCmd_ExecuteString) {
        return;
    }
    if (!sofplus_present()) {
        return;
    }
    if (g_spm_save_suppress_depth > 0) {
        return;
    }
    detour_Cmd_ExecuteString::oCmd_ExecuteString(kSoFPlusSettingsSaveCmd);
}

// SoFPlus installs its own callbacks for these cvars (cl_showfps syncs its cached
// display value, _sp_cl_master_gamespy switches masters). Chain them instead of
// replacing them so both keep working, then persist the spm settings file.
cvarcommand_t g_prev_cl_showfps_command = nullptr;
cvarcommand_t g_prev_master_gamespy_command = nullptr;

void cl_showfps_change_chain(cvar_t* cvar) {
    if (g_prev_cl_showfps_command) {
        g_prev_cl_showfps_command(cvar);
    }
    save_sofplus_spm_settings();
}

void master_gamespy_change_chain(cvar_t* cvar) {
    if (g_prev_master_gamespy_command) {
        g_prev_master_gamespy_command(cvar);
    }
    save_sofplus_spm_settings();
}

void watch_cvar(const char* name, cvarcommand_t chain, cvarcommand_t* prev) {
    cvar_t* cvar = findCvar(const_cast<char*>(name));
    if (!cvar || cvar->command == chain) {
        return;
    }
    *prev = cvar->command;
    cvar->command = chain;
}

using CvarSnapshot = std::vector<std::pair<std::string, std::string>>;

bool g_cfg_exec_queued = false;

void strip_trailing_slashes(std::string& path) {
    while (path.size() > 1 && (path.back() == '/' || path.back() == '\\')) {
        path.pop_back();
    }
}

std::string normalized_basedir() {
    std::string base = ".";
    cvar_t* basedir = findCvar(const_cast<char*>("basedir"));
    if (basedir && basedir->string && basedir->string[0]) {
        base = basedir->string;
    }
    strip_trailing_slashes(base);
    if (base.empty()) {
        base = ".";
    }
    return base;
}

std::string sofbuddy_cfg_path() {
    std::string base = normalized_basedir();
    return base + kCfgRelativePath;
}

void ensure_base_dir_exists() {
    std::string base = normalized_basedir();
    std::string base_dir = base + "/base";

    if (CreateDirectoryA(base_dir.c_str(), nullptr)) {
        return;
    }

    const DWORD err = GetLastError();
    if (err != ERROR_ALREADY_EXISTS) {
        PrintOut(PRINT_BAD, "sofbuddy_cfg: failed to create %s (error=%lu)\n",
                 base_dir.c_str(), static_cast<unsigned long>(err));
    }
}

cvar_t* get_cvar_head() {
    void* list_addr = rvaToAbsExe((void*)0x0024B1D8);
    if (!list_addr) {
        return nullptr;
    }
    return *reinterpret_cast<cvar_t**>(list_addr);
}

bool should_persist_cvar(const cvar_t* cvar) {
    if (!cvar || !cvar->name || !cvar->name[0]) {
        return false;
    }
    return (cvar->flags & CVAR_SOFBUDDY_ARCHIVE) != 0;
}

std::string escape_cfg_value(const char* value) {
    std::string escaped;
    if (!value) {
        return escaped;
    }
    for (const char* p = value; *p; ++p) {
        const char ch = *p;
        if (ch == '\\' || ch == '"') {
            escaped.push_back('\\');
        }
        escaped.push_back(ch);
    }
    return escaped;
}

CvarSnapshot capture_snapshot() {
    CvarSnapshot snapshot;
    for (cvar_t* var = get_cvar_head(); var; var = var->next) {
        if (!should_persist_cvar(var)) {
            continue;
        }
        const char* value = var->string ? var->string : "";
        snapshot.emplace_back(var->name, value);
    }

    std::sort(snapshot.begin(), snapshot.end(),
              [](const std::pair<std::string, std::string>& a,
                 const std::pair<std::string, std::string>& b) {
                  return a.first < b.first;
              });
    return snapshot;
}

bool write_snapshot_to_disk(const CvarSnapshot& snapshot) {
    ensure_base_dir_exists();
    const std::string path = sofbuddy_cfg_path();

    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) {
        PrintOut(PRINT_BAD, "sofbuddy_cfg: failed to open %s for write\n", path.c_str());
        return false;
    }

    std::fprintf(f, "// Auto-generated by sof_buddy.\n");
    std::fprintf(f, "// Loaded automatically at startup.\n\n");

    for (const auto& entry : snapshot) {
        const std::string escaped = escape_cfg_value(entry.second.c_str());
        std::fprintf(f, "set %s \"%s\"\n", entry.first.c_str(), escaped.c_str());
    }

    std::fclose(f);
    return true;
}

void exec_cfg_lines_from_path(const std::string& path) {
    if (!detour_Cmd_ExecuteString::oCmd_ExecuteString) {
        return;
    }
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        return;
    }
    char line[4096];
    while (std::fgets(line, sizeof(line), f)) {
        char* start = line;
        while (*start == ' ' || *start == '\t') {
            ++start;
        }
        if (*start == '\0' || *start == '\n' || *start == '\r') {
            continue;
        }
        if ((start[0] == '/' && start[1] == '/') || start[0] == '#') {
            continue;
        }
        size_t len = std::strlen(start);
        while (len > 0 && (start[len - 1] == '\n' || start[len - 1] == '\r' || start[len - 1] == ' ' ||
                           start[len - 1] == '\t')) {
            start[--len] = '\0';
        }
        if (!start[0]) {
            continue;
        }
        detour_Cmd_ExecuteString::oCmd_ExecuteString(start);
    }
    std::fclose(f);
}

} // namespace

void sofbuddy_cfg_exec_startup(void) {
    if (g_cfg_exec_queued) {
        return;
    }
    g_cfg_exec_queued = true;

    if (!detour_Cmd_ExecuteString::oCmd_ExecuteString) {
        PrintOut(PRINT_BAD, "sofbuddy_cfg: Cmd_ExecuteString not available for startup exec\n");
        return;
    }

    // Load from the same path we write ({basedir}/base/sofbuddy.cfg). Plain "exec sofbuddy.cfg"
    // walks the FS search path and can pick up a different file after sof_buddy/ is mounted.
    const std::string path = sofbuddy_cfg_path();
    FILE* probe = std::fopen(path.c_str(), "rb");
    if (probe) {
        std::fclose(probe);
        exec_cfg_lines_from_path(path);
        return;
    }
    detour_Cmd_ExecuteString::oCmd_ExecuteString("exec base/sofbuddy.cfg\n");
}

void sofbuddy_cfg_save_now(void) {
    const CvarSnapshot snapshot = capture_snapshot();
    write_snapshot_to_disk(snapshot);
}

void sofbuddy_cfg_save_sofplus_settings(void) {
    save_sofplus_spm_settings();
}

void sofbuddy_cfg_spm_save_disable(void) {
    ++g_spm_save_suppress_depth;
}

void sofbuddy_cfg_spm_save_enable(void) {
    if (g_spm_save_suppress_depth > 0) {
        --g_spm_save_suppress_depth;
    }
}

void sofbuddy_cfg_qcommon_frame_post_callback(int msec) {
    (void)msec;
    g_spm_saves_ready = true;
}

void sofbuddy_cfg_watch_spm_cvars(void) {
    watch_cvar("cl_showfps", cl_showfps_change_chain, &g_prev_cl_showfps_command);
    watch_cvar(kSoFPlusMasterCvar, master_gamespy_change_chain, &g_prev_master_gamespy_command);
}

void sofbuddy_cfg_cl_shutdown_post_callback(void) {
    sofbuddy_cfg_save_now();
    // Final flush of SoFPlus-owned cvars (cl_maxfps, cl_showfps, _sp_cl_cpu_cool,
    // _sp_cl_master_gamespy) to User/sofplus/data/spm_settings.cfg. These are
    // intentionally NOT CVAR_ARCHIVE when SoFPlus is present; change-triggered
    // saves (post-first-frame) already keep the file current, this is insurance.
    save_sofplus_spm_settings_on_exit();
}
