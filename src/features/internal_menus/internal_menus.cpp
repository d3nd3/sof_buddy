#include "feature_config.h"

#if FEATURE_INTERNAL_MENUS

#include "sof_compat.h"
#include "features.h"
#include "util.h"
#include "shared.h"
#include "generated_detours.h"
#include "generated_registrations.h"

#include <windows.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

std::map<std::string, std::map<std::string, std::vector<uint8_t>>> g_menu_internal_files;

namespace {

constexpr const char* kInternalMenusDiskRoot = "user/menus";

void materialize_embedded_menus_to_disk() {
    // No longer materializing to disk.
}

std::string stem_from_filename(const std::string& filename) {
    const size_t dot = filename.find('.');
    return (dot == std::string::npos) ? filename : filename.substr(0, dot);
}

void create_loading_cvars() {
    if (!orig_Cvar_Get) return;

    // Runtime loading UI state cvars.
    constexpr int kLoadingCvarFlags = 0;

	orig_Cvar_Get("_sofbuddy_loading_progress", "", kLoadingCvarFlags, nullptr);
	orig_Cvar_Get("_sofbuddy_loading_current", "", kLoadingCvarFlags, nullptr);
	orig_Cvar_Get("_sofbuddy_loading_status", "CHECKING", kLoadingCvarFlags, nullptr);
	orig_Cvar_Get("_sofbuddy_tab", "0", 0, nullptr);
    // User preference: keep loading menu input locked (default), or allow interaction.
    orig_Cvar_Get("_sofbuddy_loading_lock_input", "1", CVAR_SOFBUDDY_ARCHIVE, nullptr);
    // User preference: key used to open SoF Buddy menu.
    orig_Cvar_Get("_sofbuddy_menu_hotkey", "F12", CVAR_SOFBUDDY_ARCHIVE, nullptr);
    // Performance profile selector used by Perf Tweaks page.
    orig_Cvar_Get("_sofbuddy_perf_profile", "0", CVAR_SOFBUDDY_ARCHIVE, nullptr);

}

constexpr int kSofBuddyCenterPanelVirtualWidth = 592;
constexpr int kSofBuddyDefaultRow1ContentWidth = 400;
constexpr int kSofBuddyDefaultRow2ContentWidth = 520;

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
    orig_Cvar_Get("_sofbuddy_sb_center_panel_px", "592", kLayoutCvarFlags, nullptr);
    orig_Cvar_Get("_sofbuddy_sb_tabs_row1_prefix_px", "96", kLayoutCvarFlags, nullptr);
    orig_Cvar_Get("_sofbuddy_sb_tabs_row2_prefix_px", "36", kLayoutCvarFlags, nullptr);
    orig_Cvar_Get("_sofbuddy_sb_tabs_row1_prefix_rmf", "<blank 96 1>", kLayoutCvarFlags, nullptr);
    orig_Cvar_Get("_sofbuddy_sb_tabs_row2_prefix_rmf", "<blank 36 1>", kLayoutCvarFlags, nullptr);
    _sofbuddy_sb_tabs_row1_content_px = orig_Cvar_Get("_sofbuddy_sb_tabs_row1_content_px", "400", kLayoutCvarFlags, nullptr);
    _sofbuddy_sb_tabs_row2_content_px = orig_Cvar_Get("_sofbuddy_sb_tabs_row2_content_px", "520", kLayoutCvarFlags, nullptr);
    _sofbuddy_sb_tabs_center_bias_px = orig_Cvar_Get("_sofbuddy_sb_tabs_center_bias_px", "0", kLayoutCvarFlags, nullptr);
    _sofbuddy_sb_tabs_row1_bias_px = orig_Cvar_Get("_sofbuddy_sb_tabs_row1_bias_px", "0", kLayoutCvarFlags, nullptr);
    _sofbuddy_sb_tabs_row2_bias_px = orig_Cvar_Get("_sofbuddy_sb_tabs_row2_bias_px", "0", kLayoutCvarFlags, nullptr);

    // SoF default for tip_duration is 2500ms; bump to 5000ms if untouched.
    cvar_t* tip_duration = orig_Cvar_Get("tip_duration", "2500", 0, nullptr);
    if (tip_duration && tip_duration->string && std::strcmp(tip_duration->string, "2500") == 0 && orig_Cvar_Set2)
        orig_Cvar_Set2(const_cast<char*>("tip_duration"), const_cast<char*>("5000"), true);
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

std::string sanitize_menu_hotkey_token(const char* raw) {
    if (!raw) return "F12";

    std::string key(raw);
    key.erase(std::remove_if(key.begin(), key.end(), [](unsigned char c) {
        return std::isspace(c) != 0;
    }), key.end());

    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });

    if (key.empty()) return "F12";
    if (key.size() > 31) key.resize(31);

    for (size_t i = 0; i < key.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(key[i]);
        const bool ok = (std::isalnum(c) != 0) || c == '_';
        if (!ok) return "F12";
    }
    return key;
}

void apply_menu_hotkey_binding() {
    if (!orig_Cvar_Get || !orig_Cmd_ExecuteString) return;

    cvar_t* key_cvar = orig_Cvar_Get("_sofbuddy_menu_hotkey", "F12", CVAR_SOFBUDDY_ARCHIVE, nullptr);
    const std::string key = sanitize_menu_hotkey_token((key_cvar && key_cvar->string) ? key_cvar->string : "F12");

    if (orig_Cvar_Set2 && key_cvar && key_cvar->string && key != key_cvar->string)
        orig_Cvar_Set2(const_cast<char*>("_sofbuddy_menu_hotkey"), const_cast<char*>(key.c_str()), true);

    char cmd[128];
    std::snprintf(cmd, sizeof(cmd), "bind %s \"sofbuddy_menu sof_buddy\"", key.c_str());
    orig_Cmd_ExecuteString(cmd);
    PrintOut(PRINT_DEV, "Internal menus: bound %s to sofbuddy_menu sof_buddy\n", key.c_str());
}

void Cmd_SoFBuddy_Apply_Menu_Hotkey_f() {
    apply_menu_hotkey_binding();
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

} // namespace

bool internal_menus_should_lock_loading_input(void) {
    if (!orig_Cvar_Get) return true;
    cvar_t* c = orig_Cvar_Get("_sofbuddy_loading_lock_input", "1", CVAR_SOFBUDDY_ARCHIVE, nullptr);
    if (!c) return true;
    return c->value != 0.0f;
}

void loading_set_current(const char* map_name) {
    if (!map_name || !orig_Cvar_Set2) return;
    orig_Cvar_Set2(const_cast<char*>("_sofbuddy_loading_current"), const_cast<char*>(map_name), true);
}

namespace {
    void (*g_SCR_UpdateScreen)(bool) = nullptr;
}
void internal_menus_call_SCR_UpdateScreen(bool force) {
    if (g_SCR_UpdateScreen) g_SCR_UpdateScreen(force);
}
void internal_menus_EarlyStartup(void) {
    internal_menus_load_library();
    materialize_embedded_menus_to_disk();
    g_SCR_UpdateScreen = reinterpret_cast<void (*)(bool)>(rvaToAbsExe(reinterpret_cast<void*>(0x15FA0)));
    WriteNops(rvaToAbsExe(reinterpret_cast<void*>(0x13AAA)), 2);
    WriteNops(rvaToAbsExe(reinterpret_cast<void*>(0x13AC7)), 5);
    WriteNops(rvaToAbsExe(reinterpret_cast<void*>(0x13ACE)), 5);
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
    orig_Cmd_AddCommand(const_cast<char*>("sofbuddy_apply_menu_hotkey"), Cmd_SoFBuddy_Apply_Menu_Hotkey_f);
    orig_Cmd_AddCommand(const_cast<char*>("sofbuddy_apply_profile_comp"), Cmd_SoFBuddy_Apply_Profile_Comp_f);
    orig_Cmd_AddCommand(const_cast<char*>("sofbuddy_apply_profile_visual"), Cmd_SoFBuddy_Apply_Profile_Visual_f);

    apply_menu_hotkey_binding();

    if (orig_Cmd_ExecuteString)
        orig_Cmd_ExecuteString("set rate 20000");
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

    const bool is_sofbuddy_menu = (menu_to_push.rfind("sof_buddy/", 0) == 0);

    // SoF Buddy wrappers use <stm nopush>, so we do not need to force a killmenu
    // when switching tabs/pages there. Keep legacy replace behavior for other menus.
    if (!is_sofbuddy_menu && orig_Cmd_ExecuteString)
        orig_Cmd_ExecuteString("killmenu");

    const bool is_loading_menu = (menu_to_push == "loading/loading") ||
                                 (menu_to_push.rfind("loading/", 0) == 0);
    const bool lock_input = is_loading_menu ? internal_menus_should_lock_loading_input() : false;

    detour_M_PushMenu::oM_PushMenu(menu_to_push.c_str(), "", lock_input);
}

void internal_menus_OnVidChanged(void) {
    update_layout_cvars(true);
}

void loading_show_ui(void) {
    if (detour_M_PushMenu::oM_PushMenu) {
        const bool lock_input = internal_menus_should_lock_loading_input();
        detour_M_PushMenu::oM_PushMenu("loading/loading", "", lock_input);
    }
}

#endif
