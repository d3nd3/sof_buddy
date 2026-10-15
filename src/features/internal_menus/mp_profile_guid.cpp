#include "feature_config.h"

#if FEATURE_INTERNAL_MENUS

#include "mp_profile_guid.h"
#include "sof_compat.h"
#include "generated_detours.h"
#include "util.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>

namespace {

constexpr const char* kLoginCfgName = "sf_login.cfg";
constexpr const char* kCvarGuid = "_sofbuddy_mp_profile_guid";
constexpr const char* kCvarImportPath = "_sofbuddy_mp_profile_import_path";
constexpr const char* kCvarStatus = "_sb_internal_mp_profile_status";

bool normalize_guid(std::string& s) {
    std::string t;
    for (unsigned char c : s) {
        if (c == '{' || c == '}' || c == ' ') continue;
        if (c == '-') {
            t.push_back('-');
            continue;
        }
        if (!std::isxdigit(c)) return false;
        t.push_back(static_cast<char>(std::tolower(c)));
    }
    s = t;
    if (s.size() == 32) {
        std::string u;
        u.reserve(36);
        for (size_t i = 0; i < 32; ++i) {
            if (i == 8 || i == 12 || i == 16 || i == 20) u.push_back('-');
            u.push_back(s[i]);
        }
        s = u;
    }
    if (s.size() != 36) return false;
    for (size_t i = 0; i < 36; ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (s[i] != '-') return false;
        } else if (!std::isxdigit(static_cast<unsigned char>(s[i]))) {
            return false;
        }
    }
    return true;
}

std::string mask_guid(const std::string& g) {
    if (g.size() < 12) return g.empty() ? "(none)" : "(set)";
    return g.substr(0, 8) + std::string("...") + g.substr(g.size() - 4);
}

void set_status(const char* msg) {
    if (detour_Cvar_Set2::oCvar_Set2)
        detour_Cvar_Set2::oCvar_Set2(const_cast<char*>(kCvarStatus), const_cast<char*>(msg), true);
}

bool user_sf_login_disk_path(std::string& out) {
    char path[MAX_PATH];
    if (!sof_path_in_userdir(kLoginCfgName, path, sizeof(path))) {
        return false;
    }
    out = path;
    return true;
}

bool write_text_file(const char* path, const std::string& body) {
    FILE* f = std::fopen(path, "wb");
    if (!f) return false;
    const bool ok = std::fwrite(body.data(), 1, body.size(), f) == body.size();
    std::fclose(f);
    return ok;
}

std::string cfg_body_for_guid(const std::string& display_guid) {
    return std::string("// SoF Buddy MP profile — keep a copy in Documents\n.login ") + display_guid + "\n";
}

bool ensure_user_cfg(const std::string& display_guid) {
    std::string path;
    if (!user_sf_login_disk_path(path)) {
        return false;
    }
    sof_fs_create_path(path.c_str());
    return write_text_file(path.c_str(), cfg_body_for_guid(display_guid));
}

bool documents_backup_path(std::string& out) {
    const char* profile = std::getenv("USERPROFILE");
    if (!profile || !profile[0]) return false;
    out = std::string(profile) + "\\Documents\\sof_buddy_mp_profile.cfg";
    return true;
}

bool read_text_file(const char* path, std::string& out) {
    out.clear();
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    char buf[512];
    size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0)
        out.append(buf, n);
    std::fclose(f);
    return !out.empty();
}

bool parse_guid_from_cfg_body(const std::string& body, std::string& guid_out) {
    size_t pos = 0;
    while (pos < body.size()) {
        size_t end = body.find('\n', pos);
        if (end == std::string::npos) end = body.size();
        std::string line = body.substr(pos, end - pos);
        pos = end + 1;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t'))
            line.pop_back();
        size_t start = 0;
        while (start < line.size() && (line[start] == ' ' || line[start] == '\t')) ++start;
        if (start >= line.size() || line[start] == '/' || line[start] == '#') continue;
        const char* prefixes[] = {".login ", "login ", "cmd .login ", "cmd login "};
        for (const char* p : prefixes) {
            const size_t plen = std::strlen(p);
            if (line.size() >= start + plen && line.compare(start, plen, p) == 0) {
                guid_out = line.substr(start + plen);
                while (!guid_out.empty() && (guid_out.back() == '\r' || guid_out.back() == ' '))
                    guid_out.pop_back();
                return normalize_guid(guid_out);
            }
        }
    }
    return false;
}

bool documents_read_guid(std::string& guid_out) {
    guid_out.clear();
    std::string path;
    if (!documents_backup_path(path)) return false;
    std::string body;
    if (!read_text_file(path.c_str(), body)) return false;
    return parse_guid_from_cfg_body(body, guid_out);
}

bool documents_write_guid(const std::string& display_guid) {
    std::string path;
    if (!documents_backup_path(path)) return false;
    return write_text_file(path.c_str(), cfg_body_for_guid(display_guid));
}

void set_guid_cvar(const std::string& guid) {
    if (detour_Cvar_Set2::oCvar_Set2)
        detour_Cvar_Set2::oCvar_Set2(const_cast<char*>(kCvarGuid), const_cast<char*>(guid.c_str()), true);
}

void exec_login_cfg() {
    if (detour_Cmd_ExecuteString::oCmd_ExecuteString)
        detour_Cmd_ExecuteString::oCmd_ExecuteString("exec sf_login.cfg");
}

bool apply_guid_persisted(const std::string& guid, bool exec_cfg) {
    if (!documents_write_guid(guid)) return false;
    if (!ensure_user_cfg(guid)) return false;
    set_guid_cvar(guid);
    if (exec_cfg) exec_login_cfg();
    set_status((std::string("Active: ") + mask_guid(guid)).c_str());
    return true;
}

void refresh_menu_status(void) {
    if (!detour_Cvar_Get::oCvar_Get) return;
    std::string guid;
    if (documents_read_guid(guid)) {
        set_status((std::string("Active: ") + mask_guid(guid) + " (Documents)").c_str());
        return;
    }
    cvar_t* cv = detour_Cvar_Get::oCvar_Get(kCvarGuid, "", CVAR_SOFBUDDY_ARCHIVE, nullptr);
    if (cv && cv->string && cv->string[0]) {
        std::string g(cv->string);
        if (normalize_guid(g))
            set_status("In sofbuddy.cfg only — Save and Apply to write Documents backup.");
        else
            set_status("Invalid GUID in config — re-enter or Import.");
    } else {
        set_status("No profile GUID — ask a server admin to mint one.");
    }
}

} // namespace

void mp_profile_guid_autosync(void) {
    if (!detour_Cvar_Get::oCvar_Get) return;
    std::string guid;
    if (documents_read_guid(guid)) {
        set_guid_cvar(guid);
        ensure_user_cfg(guid);
        exec_login_cfg();
        set_status((std::string("Active: ") + mask_guid(guid)).c_str());
        return;
    }
    cvar_t* cv = detour_Cvar_Get::oCvar_Get(kCvarGuid, "", CVAR_SOFBUDDY_ARCHIVE, nullptr);
    if (cv && cv->string && cv->string[0]) {
        std::string g(cv->string);
        if (normalize_guid(g)) {
            ensure_user_cfg(g);
            exec_login_cfg();
            set_status("Loaded from sofbuddy.cfg — Save and Apply to back up to Documents.");
        }
    } else {
        set_status("No profile GUID — ask a server admin to mint one.");
    }
}

void mp_profile_guid_on_menu_open(void) {
    if (!detour_Cvar_Get::oCvar_Get || !detour_Cvar_Set2::oCvar_Set2) return;
    std::string guid;
    if (documents_read_guid(guid))
        set_guid_cvar(guid);
    refresh_menu_status();
}

static void Cmd_MpProfileApply_f(void) {
    if (!detour_Cvar_Get::oCvar_Get) return;
    cvar_t* cv = detour_Cvar_Get::oCvar_Get(kCvarGuid, "", CVAR_SOFBUDDY_ARCHIVE, nullptr);
    if (!cv || !cv->string || !cv->string[0]) {
        PrintOut(PRINT_BAD, "Enter your MP profile GUID first.\n");
        set_status("Missing GUID — contact an admin to mint one.");
        return;
    }
    std::string guid(cv->string);
    if (!normalize_guid(guid)) {
        PrintOut(PRINT_BAD, "Invalid GUID format (use xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx).\n");
        set_status("Invalid GUID format.");
        return;
    }
    std::string previous;
    if (documents_read_guid(previous) && previous != guid) {
        std::string doc_path;
        if (documents_backup_path(doc_path)) {
            const auto dot = doc_path.rfind(".cfg");
            const std::string prev_path =
                (dot != std::string::npos) ? doc_path.substr(0, dot) + "_previous.cfg" : doc_path + "_previous";
            write_text_file(prev_path.c_str(), cfg_body_for_guid(previous));
            PrintOut(PRINT_DEV, "Previous profile backed up to %s\n", prev_path.c_str());
        }
    }
    if (!apply_guid_persisted(guid, true)) {
        PrintOut(PRINT_BAD, "Could not write Documents backup or %s\n", kLoginCfgName);
        set_status("Save failed — check Documents path and user folder.");
        return;
    }
    PrintOut(PRINT_DEV, "MP profile saved to Documents and %s (under user dir)\n", kLoginCfgName);
}

static void Cmd_MpProfileExport_f(void) {
    std::string guid;
    if (!documents_read_guid(guid)) {
        cvar_t* cv = detour_Cvar_Get::oCvar_Get(kCvarGuid, "", CVAR_SOFBUDDY_ARCHIVE, nullptr);
        if (cv && cv->string && cv->string[0]) {
            guid = cv->string;
            normalize_guid(guid);
        }
    }
    if (guid.empty()) {
        PrintOut(PRINT_BAD, "Nothing to export — set a GUID first.\n");
        return;
    }
    if (!apply_guid_persisted(guid, false)) {
        PrintOut(PRINT_BAD, "Export failed.\n");
        return;
    }
    std::string path;
    documents_backup_path(path);
    PrintOut(PRINT_DEV, "Exported to %s (refreshed %s).\n", path.c_str(), kLoginCfgName);
    set_status("Backup written to Documents.");
}

static void Cmd_MpProfileImport_f(void) {
    if (!detour_Cvar_Get::oCvar_Get) return;
    cvar_t* path_cv = detour_Cvar_Get::oCvar_Get(kCvarImportPath, "", CVAR_SOFBUDDY_ARCHIVE, nullptr);
    std::string path = (path_cv && path_cv->string) ? path_cv->string : "";
    if (path.empty()) {
        if (!documents_backup_path(path)) {
            PrintOut(PRINT_BAD, "Set import path or place backup at Documents\\sof_buddy_mp_profile.cfg\n");
            return;
        }
    }
    std::string body;
    if (!read_text_file(path.c_str(), body)) {
        PrintOut(PRINT_BAD, "Could not read: %s\n", path.c_str());
        return;
    }
    std::string guid;
    if (!parse_guid_from_cfg_body(body, guid)) {
        PrintOut(PRINT_BAD, "No .login / login line found in file.\n");
        return;
    }
    set_guid_cvar(guid);
    Cmd_MpProfileApply_f();
    PrintOut(PRINT_DEV, "Imported from %s\n", path.c_str());
}

void mp_profile_guid_register(void) {
    if (!detour_Cvar_Get::oCvar_Get) return;
    detour_Cvar_Get::oCvar_Get(kCvarGuid, "", CVAR_SOFBUDDY_ARCHIVE, nullptr);
    detour_Cvar_Get::oCvar_Get(kCvarImportPath, "", CVAR_SOFBUDDY_ARCHIVE, nullptr);
    detour_Cvar_Get::oCvar_Get(kCvarStatus, "Loading...", 0, nullptr);
    if (detour_Cmd_AddCommand::oCmd_AddCommand) {
        detour_Cmd_AddCommand::oCmd_AddCommand(const_cast<char*>("sofbuddy_mp_profile_apply"), Cmd_MpProfileApply_f);
        detour_Cmd_AddCommand::oCmd_AddCommand(const_cast<char*>("sofbuddy_mp_profile_export"), Cmd_MpProfileExport_f);
        detour_Cmd_AddCommand::oCmd_AddCommand(const_cast<char*>("sofbuddy_mp_profile_import"), Cmd_MpProfileImport_f);
        detour_Cmd_AddCommand::oCmd_AddCommand(const_cast<char*>("sofbuddy_mp_profile_sync"), mp_profile_guid_autosync);
    }
}

#endif
