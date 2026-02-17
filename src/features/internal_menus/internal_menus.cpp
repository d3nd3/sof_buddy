#include "feature_config.h"

#if FEATURE_INTERNAL_MENUS

#include "sof_compat.h"
#include "features.h"
#include "util.h"
#include "shared.h"
#include "generated_detours.h"

#include <windows.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

std::map<std::string, std::map<std::string, std::vector<uint8_t>>> g_menu_internal_files;

namespace {

constexpr const char* kInternalMenusDiskRoot = "user/menus";

std::string stem_from_filename(const std::string& filename) {
    const size_t dot = filename.find('.');
    return (dot == std::string::npos) ? filename : filename.substr(0, dot);
}

bool ensure_directory_recursive(const std::string& path) {
    if (path.empty()) return false;

    std::string normalized(path);
    std::replace(normalized.begin(), normalized.end(), '\\', '/');

    std::string current;
    size_t start = 0;
    if (normalized.size() >= 2 && normalized[1] == ':') {
        current = normalized.substr(0, 2);
        start = 2;
    }

    while (start < normalized.size() && normalized[start] == '/')
        ++start;

    while (start <= normalized.size()) {
        const size_t slash = normalized.find('/', start);
        const size_t end = (slash == std::string::npos) ? normalized.size() : slash;
        if (end > start) {
            if (!current.empty() && current.back() != '/')
                current.push_back('/');
            current.append(normalized, start, end - start);
            if (!CreateDirectoryA(current.c_str(), nullptr)) {
                const DWORD err = GetLastError();
                if (err != ERROR_ALREADY_EXISTS) {
                    PrintOut(PRINT_BAD, "Internal menus: failed to create directory %s (err=%lu)\n",
                        current.c_str(), static_cast<unsigned long>(err));
                    return false;
                }
            }
        }
        if (slash == std::string::npos) break;
        start = slash + 1;
    }
    return true;
}

bool write_binary_file(const std::string& path, const std::vector<uint8_t>& bytes) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;

    bool ok = true;
    if (!bytes.empty()) {
        const size_t wrote = std::fwrite(bytes.data(), 1, bytes.size(), f);
        ok = (wrote == bytes.size());
    }
    std::fclose(f);
    return ok;
}

void materialize_embedded_menus_to_disk() {
    if (!ensure_directory_recursive(kInternalMenusDiskRoot))
        return;

    int files_written = 0;
    for (auto menu_it = g_menu_internal_files.begin(); menu_it != g_menu_internal_files.end(); ++menu_it) {
        const std::string menu_dir = std::string(kInternalMenusDiskRoot) + "/" + menu_it->first;
        if (!ensure_directory_recursive(menu_dir))
            continue;

        for (auto file_it = menu_it->second.begin(); file_it != menu_it->second.end(); ++file_it) {
            const std::string out_path = menu_dir + "/" + file_it->first;
            if (!write_binary_file(out_path, file_it->second)) {
                PrintOut(PRINT_BAD, "Internal menus: failed to write %s\n", out_path.c_str());
                continue;
            }
            ++files_written;
        }
    }

    PrintOut(PRINT_LOG, "Internal menus: materialized %d RMF files under %s\n",
        files_written, kInternalMenusDiskRoot);
}

void create_loading_cvars() {
    if (!orig_Cvar_Get) return;

    // Runtime loading UI state cvars.
    constexpr int kLoadingCvarFlags = 0;

    orig_Cvar_Get("_sofbuddy_loading_progress", "0", kLoadingCvarFlags, nullptr);
    orig_Cvar_Get("_sofbuddy_loading_current", "", kLoadingCvarFlags, nullptr);
    orig_Cvar_Get("_sofbuddy_tab", "0", 0, nullptr);
    // User preference: keep loading menu input locked (default), or allow interaction.
    orig_Cvar_Get("_sofbuddy_loading_lock_input", "1", CVAR_SOFBUDDY_ARCHIVE, nullptr);
    // Performance profile selector used by Perf Tweaks page.
    orig_Cvar_Get("_sofbuddy_perf_profile", "0", CVAR_SOFBUDDY_ARCHIVE, nullptr);

    for (int i = 1; i <= kInternalMenusLoadingHistoryRows; ++i) {
        char name[48];
        std::snprintf(name, sizeof(name), "_sofbuddy_loading_history_%d", i);
        orig_Cvar_Get(name, "", kLoadingCvarFlags, nullptr);
    }

    for (int i = 1; i <= kInternalMenusLoadingStatusRows; ++i) {
        char name[48];
        std::snprintf(name, sizeof(name), "_sofbuddy_loading_status_%d", i);
        orig_Cvar_Get(name, "", kLoadingCvarFlags, nullptr);
    }

    for (int i = 1; i <= kInternalMenusLoadingFileRows; ++i) {
        char name[48];
        std::snprintf(name, sizeof(name), "_sofbuddy_loading_file_%d", i);
        orig_Cvar_Get(name, "", kLoadingCvarFlags, nullptr);
    }
}

constexpr int kSofBuddyCenterPanelVirtualWidth = 560;
constexpr int kSofBuddyDefaultRow1ContentWidth = 392;
constexpr int kSofBuddyDefaultRow2ContentWidth = 472;

cvar_t* _sofbuddy_sb_tabs_row1_content_px = nullptr;
cvar_t* _sofbuddy_sb_tabs_row2_content_px = nullptr;
cvar_t* _sofbuddy_sb_tabs_center_bias_px = nullptr;
cvar_t* _sofbuddy_sb_tabs_row1_bias_px = nullptr;
cvar_t* _sofbuddy_sb_tabs_row2_bias_px = nullptr;

void create_layout_cvars() {
    if (!orig_Cvar_Get) return;

    constexpr int kLayoutCvarFlags = 0;
    orig_Cvar_Get("_sofbuddy_menu_vid_w", "640", kLayoutCvarFlags, nullptr);
    orig_Cvar_Get("_sofbuddy_menu_vid_h", "480", kLayoutCvarFlags, nullptr);
    orig_Cvar_Get("_sofbuddy_sb_center_panel_px", "560", kLayoutCvarFlags, nullptr);
    orig_Cvar_Get("_sofbuddy_sb_tabs_row1_prefix_px", "84", kLayoutCvarFlags, nullptr);
    orig_Cvar_Get("_sofbuddy_sb_tabs_row2_prefix_px", "116", kLayoutCvarFlags, nullptr);
    orig_Cvar_Get("_sofbuddy_sb_tabs_row1_prefix_rmf", "<blank 84 1>", kLayoutCvarFlags, nullptr);
    orig_Cvar_Get("_sofbuddy_sb_tabs_row2_prefix_rmf", "<blank 116 1>", kLayoutCvarFlags, nullptr);
    _sofbuddy_sb_tabs_row1_content_px = orig_Cvar_Get("_sofbuddy_sb_tabs_row1_content_px", "392", kLayoutCvarFlags, nullptr);
    _sofbuddy_sb_tabs_row2_content_px = orig_Cvar_Get("_sofbuddy_sb_tabs_row2_content_px", "472", kLayoutCvarFlags, nullptr);
    _sofbuddy_sb_tabs_center_bias_px = orig_Cvar_Get("_sofbuddy_sb_tabs_center_bias_px", "0", kLayoutCvarFlags, nullptr);
    _sofbuddy_sb_tabs_row1_bias_px = orig_Cvar_Get("_sofbuddy_sb_tabs_row1_bias_px", "0", kLayoutCvarFlags, nullptr);
    _sofbuddy_sb_tabs_row2_bias_px = orig_Cvar_Get("_sofbuddy_sb_tabs_row2_bias_px", "0", kLayoutCvarFlags, nullptr);
}

void set_runtime_cvar_int(const char* name, int value) {
    if (!orig_Cvar_Set2 || !name) return;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d", value);
    orig_Cvar_Set2(const_cast<char*>(name), buf, true);
}

void set_runtime_cvar_str(const char* name, const char* value) {
    if (!orig_Cvar_Set2 || !name || !value) return;
    orig_Cvar_Set2(const_cast<char*>(name), const_cast<char*>(value), true);
}

void apply_sofbuddy_perf_profile_common(const char* profile_value,
                                        const char* cl_max_debris,
                                        const char* fx_maxdebrisonscreen,
                                        const char* ghl_light_method,
                                        const char* cl_quads,
                                        const char* cl_freezequads,
                                        const char* ghl_shadows,
                                        const char* gl_modulate,
                                        const char* ghl_light_multiply,
                                        const char* gl_dlightintensity,
                                        const char* _sofbuddy_sleep,
                                        const char* ghl_mip,
                                        const char* gl_pictip,
                                        const char* gl_picmip) {
    set_runtime_cvar_str("_sofbuddy_perf_profile", profile_value);
    set_runtime_cvar_str("cl_max_debris", cl_max_debris);
    set_runtime_cvar_str("fx_maxdebrisonscreen", fx_maxdebrisonscreen);
    set_runtime_cvar_str("ghl_light_method", ghl_light_method);
    set_runtime_cvar_str("cl_quads", cl_quads);
    set_runtime_cvar_str("cl_freezequads", cl_freezequads);
    set_runtime_cvar_str("ghl_shadows", ghl_shadows);
    set_runtime_cvar_str("gl_modulate", gl_modulate);
    set_runtime_cvar_str("ghl_light_multiply", ghl_light_multiply);
    set_runtime_cvar_str("gl_dlightintensity", gl_dlightintensity);
    set_runtime_cvar_str("_sofbuddy_sleep", _sofbuddy_sleep);
    set_runtime_cvar_str("ghl_mip", ghl_mip);
    set_runtime_cvar_str("gl_pictip", gl_pictip);
    set_runtime_cvar_str("gl_picmip", gl_picmip);
    if (orig_Cmd_ExecuteString)
        orig_Cmd_ExecuteString("refresh");
}

void Cmd_SoFBuddy_Apply_Profile_Comp_f() {
    apply_sofbuddy_perf_profile_common("0", "2", "0", "0", "0", "1", "0", "2", "2", "2", "0", "0", "0", "5");
}

void Cmd_SoFBuddy_Apply_Profile_Visual_f() {
    apply_sofbuddy_perf_profile_common("1", "128", "128", "2", "1", "0", "2", "1", "1", "2", "1", "0", "0", "-10");
}

void update_layout_cvars(bool trigger_reloadall_if_changed) {
    if (!orig_Cvar_Get || !orig_Cvar_Set2) return;

    const int vid_w = (current_vid_w > 0) ? current_vid_w : 640;
    const int vid_h = (current_vid_h > 0) ? current_vid_h : 480;

    // center_panel uses frame units (560 / 640 of screen width).
    const int center_panel_px = (vid_w * kSofBuddyCenterPanelVirtualWidth + 320) / 640;
    int row1_content_px = _sofbuddy_sb_tabs_row1_content_px ? static_cast<int>(_sofbuddy_sb_tabs_row1_content_px->value + 0.5f) : kSofBuddyDefaultRow1ContentWidth;
    int row2_content_px = _sofbuddy_sb_tabs_row2_content_px ? static_cast<int>(_sofbuddy_sb_tabs_row2_content_px->value + 0.5f) : kSofBuddyDefaultRow2ContentWidth;
    const int center_bias_px = _sofbuddy_sb_tabs_center_bias_px ? static_cast<int>(_sofbuddy_sb_tabs_center_bias_px->value + 0.5f) : 0;
    const int row1_bias_px = _sofbuddy_sb_tabs_row1_bias_px ? static_cast<int>(_sofbuddy_sb_tabs_row1_bias_px->value + 0.5f) : 0;
    const int row2_bias_px = _sofbuddy_sb_tabs_row2_bias_px ? static_cast<int>(_sofbuddy_sb_tabs_row2_bias_px->value + 0.5f) : 0;

    row1_content_px = std::max(64, std::min(center_panel_px, row1_content_px));
    row2_content_px = std::max(64, std::min(center_panel_px, row2_content_px));

    const int row1_prefix_px = std::max(0, ((center_panel_px - row1_content_px) / 2) + center_bias_px + row1_bias_px);
    const int row2_prefix_px = std::max(0, ((center_panel_px - row2_content_px) / 2) + center_bias_px + row2_bias_px);

    char row1_rmf[64];
    char row2_rmf[64];
    std::snprintf(row1_rmf, sizeof(row1_rmf), "<blank %d 1>", row1_prefix_px);
    std::snprintf(row2_rmf, sizeof(row2_rmf), "<blank %d 1>", row2_prefix_px);

    set_runtime_cvar_int("_sofbuddy_menu_vid_w", vid_w);
    set_runtime_cvar_int("_sofbuddy_menu_vid_h", vid_h);
    set_runtime_cvar_int("_sofbuddy_sb_center_panel_px", center_panel_px);
    set_runtime_cvar_int("_sofbuddy_sb_tabs_row1_prefix_px", row1_prefix_px);
    set_runtime_cvar_int("_sofbuddy_sb_tabs_row2_prefix_px", row2_prefix_px);
    set_runtime_cvar_str("_sofbuddy_sb_tabs_row1_prefix_rmf", row1_rmf);
    set_runtime_cvar_str("_sofbuddy_sb_tabs_row2_prefix_rmf", row2_rmf);

    static int last_vid_w = -1;
    static int last_vid_h = -1;
    const bool changed = (vid_w != last_vid_w) || (vid_h != last_vid_h);
    last_vid_w = vid_w;
    last_vid_h = vid_h;

    // Some runtimes do not expose a reloadall command; avoid issuing unknown commands.
    (void)trigger_reloadall_if_changed;
    (void)changed;
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

std::string sanitize_loading_status_line(const char* msg) {
    auto sanitize_loading_line = [](const char* text, size_t max_chars) -> std::string {
        if (!text) return "";

        std::string cleaned;
        cleaned.reserve(96);

        bool prev_space = false;
        for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text); *p; ++p) {
            unsigned char c = *p;
            if (c == '\r' || c == '\n' || c == '\t' || c < 32) c = ' ';
            const bool is_space = (c == ' ');
            if (is_space) {
                if (prev_space) continue;
                prev_space = true;
            } else {
                prev_space = false;
            }
            cleaned.push_back(static_cast<char>(c));
        }

        while (!cleaned.empty() && cleaned.front() == ' ')
            cleaned.erase(cleaned.begin());
        while (!cleaned.empty() && cleaned.back() == ' ')
            cleaned.pop_back();

        if (max_chars > 3 && cleaned.size() > max_chars)
            cleaned = cleaned.substr(0, max_chars - 3) + "...";

        return cleaned;
    };

    // Keep lines short enough to avoid wrap/merge artifacts in narrow loading panes.
    constexpr size_t kMaxStatusChars = 36;
    return sanitize_loading_line(msg, kMaxStatusChars);
}

} // namespace

bool internal_menus_should_lock_loading_input(void) {
    if (!orig_Cvar_Get) return true;
    cvar_t* c = orig_Cvar_Get("_sofbuddy_loading_lock_input", "1", CVAR_SOFBUDDY_ARCHIVE, nullptr);
    if (!c) return true;
    return c->value != 0.0f;
}

void internal_menus_EarlyStartup(void) {
    internal_menus_load_library();
    materialize_embedded_menus_to_disk();
}

void internal_menus_PostCvarInit(void) {
    
    if (!orig_Cvar_Get) {
        PrintOut(PRINT_BAD, "Internal menus: missing Cvar_Get in PostCvarInit\n");
        return;
    }

    create_loading_cvars();
    create_layout_cvars();
    update_layout_cvars(false);

    if (!orig_Cmd_AddCommand) {
        PrintOut(PRINT_BAD, "Internal menus: missing Cmd_AddCommand, cannot register sofbuddy_menu\n");
        return;
    }

    orig_Cmd_AddCommand(const_cast<char*>("sofbuddy_menu"), Cmd_SoFBuddy_Menu_f);
    orig_Cmd_AddCommand(const_cast<char*>("sofbuddy_apply_profile_comp"), Cmd_SoFBuddy_Apply_Profile_Comp_f);
    orig_Cmd_AddCommand(const_cast<char*>("sofbuddy_apply_profile_visual"), Cmd_SoFBuddy_Apply_Profile_Visual_f);

    if (orig_Cmd_ExecuteString) {
        orig_Cmd_ExecuteString("bind F12 \"sofbuddy_menu sof_buddy\"");
        PrintOut(PRINT_GOOD, "Internal menus: bound F12 to sofbuddy_menu sof_buddy\n");
    }
}

void Cmd_SoFBuddy_Menu_f(void) {
    if (!orig_Cmd_Argv || !orig_Cmd_Argc || !detour_M_PushMenu::oM_PushMenu) return;
    if (orig_Cmd_Argc() < 2) return;

    // Keep injected RMF prefixes in sync with the current video mode before opening.
    update_layout_cvars(false);

    const char* name = orig_Cmd_Argv(1);
    if (!name || !name[0]) return;

    std::string requested(name);
    std::replace(requested.begin(), requested.end(), '\\', '/');
    if (requested.find("..") != std::string::npos) return;

    bool exists = false;
    std::string menu_to_push;

    const size_t slash = requested.rfind('/');
    if (slash == std::string::npos) {
        auto menu_it = g_menu_internal_files.find(requested);
        if (menu_it != g_menu_internal_files.end()) {
            std::string default_file = requested;
            if (default_file.find('.') == std::string::npos) default_file += ".rmf";

            auto default_it = menu_it->second.find(default_file);
            if (default_it != menu_it->second.end()) {
                exists = true;
                menu_to_push = requested + "/" + stem_from_filename(default_file);
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
        if (menu_it != g_menu_internal_files.end()) {
            exists = (menu_it->second.find(file_name) != menu_it->second.end());
            if (exists)
                menu_to_push = menu_name + "/" + stem_from_filename(file_name);
        }
    }

    if (!exists) return;

    // Replace current page instead of stacking SoF Buddy pages.
    if (orig_Cmd_ExecuteString)
        orig_Cmd_ExecuteString("killmenu");

    const bool is_loading_menu = (menu_to_push == "loading/loading") ||
                                 (menu_to_push.rfind("loading/", 0) == 0);
    const bool lock_input = is_loading_menu ? internal_menus_should_lock_loading_input() : false;

    detour_M_PushMenu::oM_PushMenu(menu_to_push.c_str(), "", lock_input);
}

void internal_menus_OnVidChanged(void) {
    update_layout_cvars(true);
}

void loading_push_history(const char* map_name) {
    if (!map_name || !map_name[0]) return;

    std::string label(map_name);
    std::replace(label.begin(), label.end(), '\\', '/');
    if (label.compare(0, 5, "maps/") == 0)
        label.erase(0, 5);
    if (label.size() >= 4) {
        const std::string tail = label.substr(label.size() - 4);
        if (tail == ".bsp" || tail == ".BSP")
            label.erase(label.size() - 4);
    }

    std::string cleaned = sanitize_loading_status_line(label.c_str());
    if (cleaned.empty()) return;

    if (orig_Cvar_Get) {
        cvar_t* current = orig_Cvar_Get("_sofbuddy_loading_history_1", "", 0, nullptr);
        if (current && current->string && cleaned == current->string)
            return;
    }

    push_rolling_text_cvars("_sofbuddy_loading_history_", kInternalMenusLoadingHistoryRows, cleaned.c_str());
}

void loading_set_current(const char* map_name) {
    if (!map_name || !orig_Cvar_Set2) return;
    orig_Cvar_Set2(const_cast<char*>("_sofbuddy_loading_current"), const_cast<char*>(map_name), true);
}

void loading_push_status(const char* msg) {
    std::string status = sanitize_loading_status_line(msg);
    if (status.empty()) return;

    if (orig_Cvar_Get) {
        cvar_t* current = orig_Cvar_Get("_sofbuddy_loading_status_1", "", 0, nullptr);
        if (current && current->string && status == current->string)
            return;
    }

    PrintOut(PRINT_GOOD, "%s\n", status.c_str());
    push_rolling_text_cvars("_sofbuddy_loading_status_", kInternalMenusLoadingStatusRows, status.c_str());
}

void loading_set_files(const char* const* filenames, int count) {
    if (!orig_Cvar_Set2) return;

    for (int i = 1; i <= kInternalMenusLoadingFileRows; ++i) {
        char name[48];
        std::snprintf(name, sizeof(name), "_sofbuddy_loading_file_%d", i);
        const char* value = (filenames && i <= count && filenames[i - 1]) ? filenames[i - 1] : "";
        orig_Cvar_Set2(name, const_cast<char*>(value), true);
    }
}

#endif
