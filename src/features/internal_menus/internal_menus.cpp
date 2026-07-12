#include "feature_config.h"

#if FEATURE_INTERNAL_MENUS

#include "sof_compat.h"
#include "features.h"
#include "util.h"
#include "shared.h"
#include "generated_detours.h"
#include "generated_registrations.h"

#if FEATURE_HTTP_MAPS
#include "features/http_maps/shared.h"
#endif

#include <windows.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

std::map<std::string, std::map<std::string, std::vector<uint8_t>>> g_menu_internal_files;

static std::string stem_from_filename(const std::string& filename) {
    const size_t dot = filename.find('.');
    return (dot == std::string::npos) ? filename : filename.substr(0, dot);
}

static std::string normalize_menu_file_path(const char* menu_file) {
    if (!menu_file || !menu_file[0]) {
        return "";
    }
    std::string name(menu_file);
    std::replace(name.begin(), name.end(), '\\', '/');
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    while (name.rfind("./", 0) == 0) {
        name.erase(0, 2);
    }
    while (!name.empty() && name[0] == '/') {
        name.erase(0, 1);
    }
    if (name.rfind("menus/", 0) == 0) {
        name.erase(0, 6);
    }
    if (name.rfind("menu/", 0) == 0) {
        name.erase(0, 5);
    }
    if (name.size() > 4 && name.compare(name.size() - 4, 4, ".rmf") == 0) {
        name.erase(name.size() - 4);
    }
    return name;
}

static bool try_resolve_stored_last_page(std::string& menu_to_push) {
    if (!detour_Cvar_Get::oCvar_Get) {
        return false;
    }
    cvar_t* cv = detour_Cvar_Get::oCvar_Get("_sofbuddy_menu_last_page", "", CVAR_SOFBUDDY_ARCHIVE, nullptr);
    if (!cv || !cv->string || !cv->string[0]) {
        return false;
    }
    std::string candidate(cv->string);
    std::replace(candidate.begin(), candidate.end(), '\\', '/');
    if (candidate.find("..") != std::string::npos) {
        return false;
    }
    std::transform(candidate.begin(), candidate.end(), candidate.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (candidate.rfind("sof_buddy/", 0) != 0) {
        return false;
    }
    const size_t slash = candidate.find('/');
    if (slash == std::string::npos) {
        return false;
    }
    const std::string menu_name = candidate.substr(0, slash);
    const std::string stem = candidate.substr(slash + 1);
    if (stem.empty()) {
        return false;
    }
    const std::string file_name = stem + ".rmf";
    auto menu_it = g_menu_internal_files.find(menu_name);
    if (menu_it == g_menu_internal_files.end()) {
        return false;
    }
    if (menu_it->second.find(file_name) == menu_it->second.end()) {
        return false;
    }
    menu_to_push = menu_name + "/" + stem_from_filename(file_name);
    return true;
}

namespace {

constexpr const char* kInternalMenusDiskRoot = "user/menus";
void set_runtime_cvar_str(const char* name, const char* value);

void materialize_embedded_menus_to_disk() {
    // No longer materializing to disk.
}

void sync_sofbuddy_profile_targets_from_selection() {
    if (!detour_Cvar_Get::oCvar_Get) return;
    cvar_t* perf = detour_Cvar_Get::oCvar_Get("_sofbuddy_perf_profile", "0", CVAR_SOFBUDDY_ARCHIVE, nullptr);
    const int profile = perf ? static_cast<int>(perf->value + 0.5f) : 0;
    static int last_profile = -9999;
    if (profile == last_profile) return;
    last_profile = profile;
    const char* fx = "0";
    const char* debris = "2";
    const char* shadows = "Off";
    const char* specular = "Off";
    const char* quads = "0";
    if (profile == 1) {
        fx = "32"; debris = "32"; shadows = "Detailed"; specular = "On"; quads = "1024";
    } else if (profile == 2) {
        fx = "0"; debris = "2"; shadows = "Detailed"; specular = "On"; quads = "1024";
    } else if (profile == 3) {
        cvar_t* cv_fx = detour_Cvar_Get::oCvar_Get("fx_maxdebrisonscreen", "0", 0, nullptr);
        cvar_t* cv_debris = detour_Cvar_Get::oCvar_Get("cl_max_debris", "2", 0, nullptr);
        cvar_t* cv_shadows = detour_Cvar_Get::oCvar_Get("ghl_shadows", "0", 0, nullptr);
        cvar_t* cv_specular = detour_Cvar_Get::oCvar_Get("ghl_specular", "0", 0, nullptr);
        cvar_t* cv_quads = detour_Cvar_Get::oCvar_Get("cl_max_quads", "0", 0, nullptr);
        fx = (cv_fx && cv_fx->string) ? cv_fx->string : "0";
        debris = (cv_debris && cv_debris->string) ? cv_debris->string : "2";
        const int sh = cv_shadows ? static_cast<int>(cv_shadows->value + 0.5f) : 0;
        shadows = (sh <= 0) ? "Off" : ((sh == 1) ? "Blob" : "Detailed");
        const int sp = cv_specular ? static_cast<int>(cv_specular->value + 0.5f) : 0;
        specular = (sp == 0) ? "Off" : "On";
        quads = (cv_quads && cv_quads->string) ? cv_quads->string : "0";
    }
    set_runtime_cvar_str("_sofbuddy_profile_target_fx_maxdebrisonscreen", fx);
    set_runtime_cvar_str("_sofbuddy_profile_target_cl_max_debris", debris);
    set_runtime_cvar_str("_sofbuddy_profile_target_ghl_shadows", shadows);
    set_runtime_cvar_str("_sofbuddy_profile_target_ghl_specular", specular);
    set_runtime_cvar_str("_sofbuddy_profile_target_cl_max_quads", quads);
}

void sofbuddy_perf_profile_change(cvar_t* cvar) {
    (void)cvar;
    sync_sofbuddy_profile_targets_from_selection();
}

void apply_sofbuddy_perf_profile_common(const char* fx_maxdebrisonscreen,
                                        const char* cl_max_debris,
                                        const char* ghl_shadows,
                                        const char* ghl_specular,
                                        const char* cl_max_quads) {
    set_runtime_cvar_str("fx_maxdebrisonscreen", fx_maxdebrisonscreen);
    set_runtime_cvar_str("cl_max_debris", cl_max_debris);
    set_runtime_cvar_str("ghl_shadows", ghl_shadows);
    set_runtime_cvar_str("ghl_specular", ghl_specular);
    set_runtime_cvar_str("cl_max_quads", cl_max_quads);
    if (detour_Cmd_ExecuteString::oCmd_ExecuteString)
        detour_Cmd_ExecuteString::oCmd_ExecuteString("refresh");
}

void apply_sofbuddy_perf_profile_value(int profile) {
    switch (profile) {
        case 1: apply_sofbuddy_perf_profile_common("32", "32", "2", "1", "1024"); break;
        case 2: apply_sofbuddy_perf_profile_common("0", "2", "2", "1", "1024"); break;
        case 3: break; // Custom/Current is view-only.
        default: apply_sofbuddy_perf_profile_common("0", "2", "0", "0", "0"); break;
    }
}

void create_loading_cvars() {
    if (!detour_Cvar_Get::oCvar_Get) return;

    // Runtime loading UI state cvars.
    constexpr int kLoadingCvarFlags = 0;

	detour_Cvar_Get::oCvar_Get("_sofbuddy_loading_progress", "", kLoadingCvarFlags, nullptr);
	detour_Cvar_Get::oCvar_Get("_sofbuddy_loading_current", "", kLoadingCvarFlags, nullptr);
	detour_Cvar_Get::oCvar_Get("_sofbuddy_loading_status", "CHECKING", kLoadingCvarFlags, nullptr);
	detour_Cvar_Get::oCvar_Get("_sofbuddy_loading_network", "0", kLoadingCvarFlags, nullptr);
	detour_Cvar_Get::oCvar_Get("_sofbuddy_tab", "0", 0, nullptr);
    // User preference: keep loading menu input locked (default), or allow interaction.
    detour_Cvar_Get::oCvar_Get("_sofbuddy_loading_lock_input", "1", CVAR_SOFBUDDY_ARCHIVE, nullptr);
    detour_Cvar_Get::oCvar_Get("_sofbuddy_loading_show_mapname", "1", CVAR_SOFBUDDY_ARCHIVE, nullptr);
    detour_Cvar_Get::oCvar_Get("_sofbuddy_loading_show_download", "1", CVAR_SOFBUDDY_ARCHIVE, nullptr);
    // User preference: key used to open SoF Buddy menu.
    detour_Cvar_Get::oCvar_Get("_sofbuddy_menu_hotkey", "F12", CVAR_SOFBUDDY_ARCHIVE, nullptr);
    // Last SoF Buddy tab/page (e.g. sof_buddy/map_debug) restored when opening via F12 (sofbuddy_menu sof_buddy).
    detour_Cvar_Get::oCvar_Get("_sofbuddy_menu_last_page", "", CVAR_SOFBUDDY_ARCHIVE, nullptr);
    // Performance profile selector used by Perf Tweaks page.
    detour_Cvar_Get::oCvar_Get("_sofbuddy_perf_profile", "0", CVAR_SOFBUDDY_ARCHIVE, nullptr);
    detour_Cvar_Get::oCvar_Get("_sofbuddy_profile_target_fx_maxdebrisonscreen", "0", 0, nullptr);
    detour_Cvar_Get::oCvar_Get("_sofbuddy_profile_target_cl_max_debris", "2", 0, nullptr);
    detour_Cvar_Get::oCvar_Get("_sofbuddy_profile_target_ghl_shadows", "Off", 0, nullptr);
    detour_Cvar_Get::oCvar_Get("_sofbuddy_profile_target_ghl_specular", "Off", 0, nullptr);
    detour_Cvar_Get::oCvar_Get("_sofbuddy_profile_target_cl_max_quads", "0", 0, nullptr);
    cvar_t* perf = detour_Cvar_Get::oCvar_Get("_sofbuddy_perf_profile", "0", CVAR_SOFBUDDY_ARCHIVE, sofbuddy_perf_profile_change);
    sofbuddy_perf_profile_change(perf);

    // FX_Init registers this with flags 0; OR CVAR_ARCHIVE so menu/config.cfg persist changes.
    detour_Cvar_Get::oCvar_Get("fx_maxdebrisonscreen", "16", CVAR_ARCHIVE, nullptr);
    // CL_InitLocal: cl_showfps/m_yaw lack CVAR_ARCHIVE; OR so OSD and sensitivity persist.
    detour_Cvar_Get::oCvar_Get("cl_showfps", "0", CVAR_ARCHIVE, nullptr);
    detour_Cvar_Get::oCvar_Get("m_yaw", "0.022", CVAR_ARCHIVE, nullptr);
    detour_Cvar_Get::oCvar_Get("m_pitch", "0.022", CVAR_ARCHIVE, nullptr);
}

constexpr int kSofBuddyCenterPanelVirtualWidth = 640;
constexpr int kSofBuddyDefaultRow1ContentWidth = 384;
constexpr int kSofBuddyDefaultRow2ContentWidth = 384;
constexpr int kSofBuddyTabWidthPx = 120;
constexpr int kSofBuddyTabGapPx = 12;
constexpr int kSofBuddyTabLeftNudgePx = 32;

cvar_t* _sofbuddy_sb_tabs_row1_content_px = nullptr;
cvar_t* _sofbuddy_sb_tabs_row2_content_px = nullptr;
cvar_t* _sofbuddy_sb_tabs_center_bias_px = nullptr;
cvar_t* _sofbuddy_sb_tabs_row1_bias_px = nullptr;
cvar_t* _sofbuddy_sb_tabs_row2_bias_px = nullptr;

void create_layout_cvars() {
    if (!detour_Cvar_Get::oCvar_Get) return;

    constexpr int kLayoutCvarFlags = 0;
    detour_Cvar_Get::oCvar_Get("_sofbuddy_menu_vid_w", "640", kLayoutCvarFlags, nullptr);
    detour_Cvar_Get::oCvar_Get("_sofbuddy_menu_vid_h", "480", kLayoutCvarFlags, nullptr);
    detour_Cvar_Get::oCvar_Get("_sofbuddy_sb_center_panel_px", "640", kLayoutCvarFlags, nullptr);
    detour_Cvar_Get::oCvar_Get("_sofbuddy_sb_tabs_row1_prefix_px", "96", kLayoutCvarFlags, nullptr);
    detour_Cvar_Get::oCvar_Get("_sofbuddy_sb_tabs_row2_prefix_px", "36", kLayoutCvarFlags, nullptr);
    detour_Cvar_Get::oCvar_Get("_sofbuddy_sb_tabs_row1_prefix_rmf", "<blank 96 1>", kLayoutCvarFlags, nullptr);
    detour_Cvar_Get::oCvar_Get("_sofbuddy_sb_tabs_row2_prefix_rmf", "<blank 36 1>", kLayoutCvarFlags, nullptr);
    detour_Cvar_Get::oCvar_Get("_sofbuddy_sb_tabs_row1_suffix_rmf", "<blank 96 1>", kLayoutCvarFlags, nullptr);
    detour_Cvar_Get::oCvar_Get("_sofbuddy_sb_tabs_row2_suffix_rmf", "<blank 36 1>", kLayoutCvarFlags, nullptr);
    _sofbuddy_sb_tabs_row1_content_px = detour_Cvar_Get::oCvar_Get("_sofbuddy_sb_tabs_row1_content_px", "384", kLayoutCvarFlags, nullptr);
    _sofbuddy_sb_tabs_row2_content_px = detour_Cvar_Get::oCvar_Get("_sofbuddy_sb_tabs_row2_content_px", "384", kLayoutCvarFlags, nullptr);
    _sofbuddy_sb_tabs_center_bias_px = detour_Cvar_Get::oCvar_Get("_sofbuddy_sb_tabs_center_bias_px", "0", kLayoutCvarFlags, nullptr);
    _sofbuddy_sb_tabs_row1_bias_px = detour_Cvar_Get::oCvar_Get("_sofbuddy_sb_tabs_row1_bias_px", "0", kLayoutCvarFlags, nullptr);
    _sofbuddy_sb_tabs_row2_bias_px = detour_Cvar_Get::oCvar_Get("_sofbuddy_sb_tabs_row2_bias_px", "0", kLayoutCvarFlags, nullptr);

    // Menu color theme preset index (see kMenuThemes).
    detour_Cvar_Get::oCvar_Get("_sofbuddy_menu_theme", "0", CVAR_SOFBUDDY_ARCHIVE, nullptr);

    // SoF default for tip_duration is 2500ms; bump to 5000ms if untouched.
    cvar_t* tip_duration = detour_Cvar_Get::oCvar_Get("tip_duration", "2500", 0, nullptr);
    if (tip_duration && tip_duration->string && std::strcmp(tip_duration->string, "2500") == 0 && detour_Cvar_Set2::oCvar_Set2)
        detour_Cvar_Set2::oCvar_Set2(const_cast<char*>("tip_duration"), const_cast<char*>("5000"), true);
}

void set_runtime_cvar_int(const char* name, int value) {
    if (!detour_Cvar_Set2::oCvar_Set2 || !name) return;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d", value);
    detour_Cvar_Set2::oCvar_Set2(const_cast<char*>(name), buf, true);
}

void set_runtime_cvar_str(const char* name, const char* value) {
    if (!detour_Cvar_Set2::oCvar_Set2 || !name || !value) return;
    detour_Cvar_Set2::oCvar_Set2(const_cast<char*>(name), const_cast<char*>(value), true);
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
    if (!detour_Cvar_Get::oCvar_Get || !detour_Cmd_ExecuteString::oCmd_ExecuteString) return;

    cvar_t* key_cvar = detour_Cvar_Get::oCvar_Get("_sofbuddy_menu_hotkey", "F12", CVAR_SOFBUDDY_ARCHIVE, nullptr);
    const std::string key = sanitize_menu_hotkey_token((key_cvar && key_cvar->string) ? key_cvar->string : "F12");

    if (detour_Cvar_Set2::oCvar_Set2 && key_cvar && key_cvar->string && key != key_cvar->string)
        detour_Cvar_Set2::oCvar_Set2(const_cast<char*>("_sofbuddy_menu_hotkey"), const_cast<char*>(key.c_str()), true);

    char cmd[128];
    std::snprintf(cmd, sizeof(cmd), "bind %s \"sofbuddy_menu sof_buddy\"", key.c_str());
    detour_Cmd_ExecuteString::oCmd_ExecuteString(cmd);
    PrintOut(PRINT_DEV, "Internal menus: bound %s to sofbuddy_menu sof_buddy\n", key.c_str());
}

void Cmd_SoFBuddy_Apply_Menu_Hotkey_f() {
    apply_menu_hotkey_binding();
}

void Cmd_SoFBuddy_Apply_Profile_f() {
    if (!detour_Cvar_Get::oCvar_Get) return;
    cvar_t* perf = detour_Cvar_Get::oCvar_Get("_sofbuddy_perf_profile", "0", CVAR_SOFBUDDY_ARCHIVE, sofbuddy_perf_profile_change);
    const int profile = perf ? static_cast<int>(perf->value + 0.5f) : 0;
    apply_sofbuddy_perf_profile_value(profile);
}

void update_layout_cvars(bool trigger_reloadall_if_changed) {
    if (!detour_Cvar_Get::oCvar_Get || !detour_Cvar_Set2::oCvar_Set2) return;

    int vid_w = (current_vid_w > 0) ? current_vid_w : 0;
    int vid_h = (current_vid_h > 0) ? current_vid_h : 0;
    if (vid_w <= 0 && viddef_width && *viddef_width > 0) vid_w = *viddef_width;
    if (vid_h <= 0 && viddef_height && *viddef_height > 0) vid_h = *viddef_height;
    if (vid_w <= 0) vid_w = 640;
    if (vid_h <= 0) vid_h = 480;

    // center_panel uses frame units (560 / 640 of screen width).
    const int center_panel_px = (vid_w * kSofBuddyCenterPanelVirtualWidth + 320) / 640;
    int row1_content_px = _sofbuddy_sb_tabs_row1_content_px ? static_cast<int>(_sofbuddy_sb_tabs_row1_content_px->value + 0.5f) : kSofBuddyDefaultRow1ContentWidth;
    int row2_content_px = _sofbuddy_sb_tabs_row2_content_px ? static_cast<int>(_sofbuddy_sb_tabs_row2_content_px->value + 0.5f) : kSofBuddyDefaultRow2ContentWidth;
    const int center_bias_px = _sofbuddy_sb_tabs_center_bias_px ? static_cast<int>(_sofbuddy_sb_tabs_center_bias_px->value + 0.5f) : 0;
    const int row1_bias_px = _sofbuddy_sb_tabs_row1_bias_px ? static_cast<int>(_sofbuddy_sb_tabs_row1_bias_px->value + 0.5f) : 0;
    const int row2_bias_px = _sofbuddy_sb_tabs_row2_bias_px ? static_cast<int>(_sofbuddy_sb_tabs_row2_bias_px->value + 0.5f) : 0;

    row1_content_px = std::max(64, std::min(kSofBuddyCenterPanelVirtualWidth, row1_content_px));
    row2_content_px = row1_content_px;

    const int row1_prefix_px = std::max(0, ((kSofBuddyCenterPanelVirtualWidth - row1_content_px) / 2) + center_bias_px + row1_bias_px - kSofBuddyTabLeftNudgePx);
    const int row2_prefix_px = row1_prefix_px + row2_bias_px - row1_bias_px;

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
    set_runtime_cvar_str("_sofbuddy_sb_tabs_row1_suffix_rmf", row1_rmf);
    set_runtime_cvar_str("_sofbuddy_sb_tabs_row2_suffix_rmf", row2_rmf);

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

void internal_menus_remember_menu_page(const char* menu_file) {
    if (!menu_file || !detour_Cvar_Set2::oCvar_Set2) {
        return;
    }
    std::string n = normalize_menu_file_path(menu_file);
    if (n.empty()) {
        return;
    }
    // M_PushMenu sometimes passes only the leaf (e.g. "cpu") with no sof_buddy/ prefix; map to embedded name.
    if (n.rfind("sof_buddy/", 0) != 0) {
        if (n.find('/') == std::string::npos) {
            auto it = g_menu_internal_files.find("sof_buddy");
            if (it != g_menu_internal_files.end()) {
                const std::string rmf = n + ".rmf";
                if (it->second.find(rmf) != it->second.end()) {
                    n = "sof_buddy/" + n;
                }
            }
        }
        if (n.rfind("sof_buddy/", 0) != 0) {
            return;
        }
    }
    if (n == "sof_buddy/update_prompt") {
        return;
    }
    set_runtime_cvar_str("_sofbuddy_menu_last_page", n.c_str());
}

static int internal_menus_content_inset_px(void) {
    int inner_frame_px = 0;
    if (detour_Cvar_Get::oCvar_Get) {
        cvar_t* c = detour_Cvar_Get::oCvar_Get("_sofbuddy_sb_center_panel_px", "640", 0, nullptr);
        if (c && c->value > 0) inner_frame_px = static_cast<int>(c->value + 0.5f);
    }
    if (inner_frame_px <= 0) {
        int vid_w = (current_vid_w > 0) ? current_vid_w : 0;
        if (vid_w <= 0 && viddef_width && *viddef_width > 0) vid_w = *viddef_width;
        if (vid_w <= 0) vid_w = 640;
        inner_frame_px = vid_w;
    }
    const int centered_content_size_px = 640;
    return std::max(28, (inner_frame_px - centered_content_size_px) / 2);
}

static int internal_menus_menu_vid_h_px(void) {
    int vid_h = (current_vid_h > 0) ? current_vid_h : 0;
    if (vid_h <= 0 && viddef_height && *viddef_height > 0) vid_h = *viddef_height;
    if (vid_h <= 0) vid_h = 480;
    const int out = vid_h; // Match full-screen vbar extent across modes.
    static int prev_vid = -999999;
    static int prev_out = -999999;
    if (vid_h != prev_vid || out != prev_out) {
        prev_vid = vid_h;
        prev_out = out;
        PrintOut(PRINT_DEV,
                 "sofbuddy: tall-blank height fixed src=vid vid_h=%d using=%d\n",
                 vid_h, out);
    }
    return out;
}
const char* internal_menus_get_content_inset_rmf(void) {
    static char buf[32];
    std::snprintf(buf, sizeof(buf), "<blank %d 1>", internal_menus_content_inset_px());
    return buf;
}
const char* internal_menus_get_content_inset_tall_rmf(void) {
    static char buf[32];
    std::snprintf(buf, sizeof(buf), "<blank %d %d>", internal_menus_content_inset_px(), internal_menus_menu_vid_h_px());
    return buf;
}
// Menu color themes as ABGR (0xAABBGGRR): bg, panel, fg, tip, hili, accent, subtle, heading, muted, active.
// Active tab uses the orange tint slot but each theme picks a distinct hue (pink/green/gold/cyan/etc.).
struct MenuTheme { unsigned bg, panel, fg, tip, hili, accent, subtle, heading, muted, active; };
static const MenuTheme kMenuThemes[] = {
    {0xff130905, 0xff2b1b10, 0xfffff7f3, 0xffe8d4c8, 0xff05070b, 0xffd0ff43, 0xff553b2b, 0xffffd94d, 0xffaa927a, 0xffc679ff}, // 0 Dark
    {0xff000000, 0xff161616, 0xffffffff, 0xffe0e0e0, 0xff1a1a1a, 0xffffff00, 0xff404040, 0xff00ffff, 0xffa0a0a0, 0xffff00ff}, // 1 HiCon
    {0xff0a0500, 0xff1a0f00, 0xffffb000, 0xffcc8800, 0xff080300, 0xff00d7ff, 0xffa06a10, 0xffffd27f, 0xffaa8844, 0xff6633ff}, // 2 Amber
    {0xff001100, 0xff002600, 0xff33ff66, 0xff28cc4d, 0xff000800, 0xffaaff00, 0xff2f8a4f, 0xff00ffcc, 0xff25663d, 0xffcc00ff}, // 3 Green
    {0xff362b00, 0xff423607, 0xff969483, 0xff837b65, 0xff261e00, 0xffd28b26, 0xff756e58, 0xff98a12a, 0xff524e45, 0xff2f32dc}, // 4 SolDk
    {0xffe3f6fd, 0xffd5e8ee, 0xff423607, 0xff362b00, 0xffc1d6dd, 0xff8236d3, 0xffa1a193, 0xffd28b26, 0xff837b65, 0xffc4716c}, // 5 SolLt
    {0xfff5f5f0, 0xffe6e6e0, 0xff1a1a1a, 0xff333333, 0xffecece8, 0xffcc6600, 0xff999999, 0xffaa8800, 0xff707070, 0xff880066}, // 6 Paper
    {0xff000000, 0xff101010, 0xffffff00, 0xffcccc00, 0xff0a0a0a, 0xff00ffff, 0xff888800, 0xffffffff, 0xffb0b000, 0xff8000ff}, // 7 HiYel
    {0xff362a28, 0xff5a4744, 0xfff2f8f8, 0xffdce0e0, 0xff2c2221, 0xfff993bd, 0xffa47262, 0xfffde98b, 0xff725751, 0xff7bfa50}, // 8 Dracula
    {0xff342c28, 0xff3c322c, 0xffbfb2ab, 0xff978982, 0xff2b2521, 0xffdd78c6, 0xff70635c, 0xffefaf61, 0xff63524b, 0xff7bc0e5}, // 9 1Dark
    {0xff40342e, 0xff52423b, 0xfff4efec, 0xffe9ded8, 0xff5e4c43, 0xffd0c088, 0xff6a564c, 0xffc1a181, 0xff886e61, 0xff6a61bf}, // 10 Nord
    {0xff222827, 0xff323d3e, 0xfff2f8f8, 0xffc2cfcf, 0xff1c1f1e, 0xff2ee2a6, 0xff3e4849, 0xffefd966, 0xff5e7175, 0xff7226f9}, // 11 Mono
    {0xff261b1a, 0xff3b2824, 0xfff5cac0, 0xffcea59a, 0xff1e1616, 0xfff79abb, 0xff684841, 0xfff7a27a, 0xff895f56, 0xff6ace9e}, // 12 Tokyo
    {0xff2e1e1e, 0xff443231, 0xfff4d6cd, 0xffc8ada6, 0xff251818, 0xfff7a6cb, 0xff5a4745, 0xfffab489, 0xff86706c, 0xffe7c2f5}, // 13 Catpp
    {0xff282828, 0xff36383c, 0xffb2dbeb, 0xffa1c4d5, 0xff21201d, 0xff26bbb8, 0xff454950, 0xff98a583, 0xff748392, 0xff2fbdfa}, // 14 Gruv
    {0xff3b352d, 0xff4d483d, 0xffaac6d3, 0xff8499a8, 0xff2e2a23, 0xff80c0a7, 0xff585247, 0xffb3bb7f, 0xff899285, 0xff807ee6}, // 15 Evrf
    {0xff17110d, 0xff221b16, 0xffd9d1c9, 0xff9e948b, 0xff090401, 0xffffa658, 0xff3d3630, 0xffffc079, 0xff81766e, 0xffffa8d2}, // 16 GHDrk
    {0xff140e0a, 0xff291f1a, 0xffb6bdbf, 0xff8f9893, 0xff0c0805, 0xff54b4ff, 0xff443832, 0xffffc259, 0xff736a62, 0xff7871f0}, // 17 Ayu
    {0xff3e2d29, 0xff4d3732, 0xffcdaca6, 0xffb88b82, 0xff32211e, 0xffea92c7, 0xff79554e, 0xffffaa82, 0xff956e67, 0xff8de8c3}, // 18 Palen
    {0xff241719, 0xff2e1d1f, 0xfff4dee0, 0xffaa8c90, 0xff1c1012, 0xffe7a7c4, 0xff3a2326, 0xffd8cf9c, 0xff866a6e, 0xff926feb}, // 19 Rose
    {0xff271601, 0xff42290b, 0xffebded6, 0xffc4b1a2, 0xff221100, 0xffffaa82, 0xff533b1d, 0xffcadb7f, 0xff777763, 0xffea92c7}, // 20 Nowl
    {0xff493519, 0xff382712, 0xffffffff, 0xffe8d8c0, 0xff37210d, 0xff00c6ff, 0xffff8800, 0xff00d93a, 0xff009dff, 0xff8c62ff}, // 21 Cob2
    {0xff3f3f3f, 0xff383838, 0xffccdcdc, 0xffacbcbc, 0xff2f2f2f, 0xff809070, 0xff606060, 0xffd3d08c, 0xffafaf9f, 0xffafdff0}, // 22 Zen
    {0xff2f1b24, 0xff4f2934, 0xfff1eff0, 0xffd8c0c8, 0xff1f1017, 0xffdb7eff, 0xff653446, 0xfff6f936, 0xffbd8b84, 0xff5ddefe}, // 23 Synth
    {0xff020d02, 0xff001a00, 0xff00ff41, 0xff00cc33, 0xff000800, 0xff66ff00, 0xff008f11, 0xffccff00, 0xff00660a, 0xffffff00}, // 24 Matrix
    {0xff000000, 0xff0a0a0a, 0xffe0e0e0, 0xffb0b0b0, 0xff141414, 0xff00e676, 0xff333333, 0xffffcc00, 0xff888888, 0xffff6699}, // 25 OLED
    {0xffffffff, 0xfffaf8f6, 0xff2f2924, 0xff534a42, 0xfff6f3f0, 0xffda6909, 0xffded7d0, 0xffae5005, 0xff766d65, 0xff2e22cf}, // 26 GHLit
    {0xfffafafa, 0xfff0f0f0, 0xff423a38, 0xff776c69, 0xffe6e5e5, 0xffa426a6, 0xffd4d4d4, 0xfff27840, 0xffa0a1a0, 0xff4956e4}, // 27 1Lite
    {0xfff5f1ef, 0xffefe9e6, 0xff694f4c, 0xff856f6c, 0xffe8e0dc, 0xffef3988, 0xffdad0cc, 0xfff5661e, 0xffb0a09c, 0xffcb76ea}, // 28 Latte
    {0xfff4efec, 0xfff0e9e5, 0xff40342e, 0xff6a564c, 0xffe9ded8, 0xffac815e, 0xffdacec8, 0xffc1a181, 0xff918880, 0xff8cbea3}, // 29 NSnow
    {0xfff4ecd8, 0xffebe3cf, 0xff433422, 0xff665544, 0xffe6ddc8, 0xff8b4513, 0xffccc0a8, 0xffaa7722, 0xff7a6a55, 0xff224488}, // 30 Sepia
    {0xff281f1f, 0xff372a2a, 0xffbad7dc, 0xffa6c2c8, 0xff1d1616, 0xffd89c7e, 0xff463636, 0xffb87f95, 0xff697172, 0xff6cbb98}, // 31 Kanag
    {0xff261e1c, 0xff302523, 0xffe1d5cb, 0xffc4b4a8, 0xff1d1716, 0xff6d56e9, 0xff3e302e, 0xffbcb025, 0xff89706b, 0xff95b7fa}, // 32 Horiz
    {0xff161616, 0xff262626, 0xfff8f4f2, 0xffcdc7c1, 0xff0d0d0d, 0xfff5a542, 0xff393939, 0xffffb133, 0xff8d8d8d, 0xffcf6fff}, // 33 Oxo
    {0xff19140f, 0xff32231a, 0xffcfe1e6, 0xffa0b0b8, 0xff120e0a, 0xffafff82, 0xff253040, 0xffe6cf5c, 0xff81766e, 0xff80d5ff}, // 34 Moon
    {0xff0a0a1a, 0xff14142a, 0xffd6e4ff, 0xffa8b8d4, 0xff060612, 0xff4466ff, 0xff20204a, 0xff44aaff, 0xff556688, 0xff4422ff}, // 35 Ember
    {0xff20101a, 0xff301828, 0xfff8e8ff, 0xffd0b8d8, 0xff180812, 0xffb469ff, 0xff482840, 0xffffe500, 0xff887890, 0xff00ffff}, // 36 Candy
    {0xff28160a, 0xff382214, 0xfffff4e8, 0xfff0d4b8, 0xff180e06, 0xffffcc66, 0xff50301e, 0xffffd8a8, 0xffaa8868, 0xffcc88ff}, // 37 Frost
};
// Short names for <list> labels — engine comma-split buffer is only 240 bytes (SoF.exe sub_200C92D0).
static const char* kMenuThemeListLabels[] = {
    "Dark", "HiCon", "Amber", "Green", "SolDk", "SolLt", "Paper", "HiYel",
    "Dracula", "1Dark", "Nord", "Mono", "Tokyo", "Catpp", "Gruv", "Evrf",
    "GHDrk", "Ayu", "Palen", "Rose", "Nowl", "Cob2", "Zen", "Synth",
    "Matrix", "OLED", "GHLit", "1Lite", "Latte", "NSnow", "Sepia",
    "Kanag", "Horiz", "Oxo", "Moon", "Ember", "Candy", "Frost",
};
static_assert(sizeof(kMenuThemeListLabels) / sizeof(kMenuThemeListLabels[0]) ==
              sizeof(kMenuThemes) / sizeof(kMenuThemes[0]), "theme list/name count mismatch");

const char* internal_menus_get_theme_list_labels_rmf(void) {
    static char buf[280];
    char* p = buf;
    const char* end = buf + sizeof(buf) - 1;
    const int n = static_cast<int>(sizeof(kMenuThemeListLabels) / sizeof(kMenuThemeListLabels[0]));
    for (int i = 0; i < n; ++i) {
        const char* label = kMenuThemeListLabels[i];
        const int need = static_cast<int>(std::strlen(label)) + (i ? 1 : 0);
        if (p + need >= end) break;
        if (i) *p++ = ',';
        std::memcpy(p, label, std::strlen(label));
        p += std::strlen(label);
    }
    *p = '\0';
    return buf;
}

const char* internal_menus_get_theme_list_rmf(void) {
    static char buf[768];
    const char* labels = internal_menus_get_theme_list_labels_rmf();
    const char* match = internal_menus_get_theme_list_match_rmf();
    std::snprintf(buf, sizeof(buf),
        "<list \"%s\" match \"%s\" cvari _sofbuddy_menu_theme atext \"Menu Theme : \" "
        "key mouse1 \"reloadall;sofbuddy_menu sof_buddy/sofbuddy\" "
        "key mouse2 \"reloadall;sofbuddy_menu sof_buddy/sofbuddy\" "
        "noshade tip \"Editor-inspired color preset. Tab reloads to apply tints.\"><br>",
        labels, match);
    return buf;
}

const char* internal_menus_get_theme_list_match_rmf(void) {
    static char buf[128];
    char* p = buf;
    const char* end = buf + sizeof(buf) - 1;
    const int n = static_cast<int>(sizeof(kMenuThemes) / sizeof(kMenuThemes[0]));
    for (int i = 0; i < n; ++i) {
        char num[8];
        std::snprintf(num, sizeof(num), "%d", i);
        const int need = static_cast<int>(std::strlen(num)) + (i ? 1 : 0);
        if (p + need >= end) break;
        if (i) *p++ = ',';
        std::memcpy(p, num, std::strlen(num));
        p += std::strlen(num);
    }
    *p = '\0';
    return buf;
}

const char* internal_menus_get_theme_tints_rmf(void) {
    static char buf[512];
    int idx = 0;
    if (detour_Cvar_Get::oCvar_Get) {
        cvar_t* c = detour_Cvar_Get::oCvar_Get("_sofbuddy_menu_theme", "0", CVAR_SOFBUDDY_ARCHIVE, nullptr);
        if (c) idx = static_cast<int>(c->value + 0.5f);
    }
    if (idx < 0 || idx >= static_cast<int>(sizeof(kMenuThemes) / sizeof(kMenuThemes[0]))) idx = 0;
    const MenuTheme& t = kMenuThemes[idx];
    std::snprintf(buf, sizeof(buf),
        "<tint normaltext 0x%08x><tint hilitetext 0x%08x>"
        "<tint sb_bg 0x%08x><tint sb_panel 0x%08x>"
        "<tint sb_accent 0x%08x><tint sb_subtle 0x%08x>"
        "<tint white 0x%08x><tint cyan 0x%08x>"
        "<tint gray 0x%08x><tint orange 0x%08x>",
        t.tip, t.hili, t.bg, t.panel,
        t.accent, t.subtle, t.fg, t.heading,
        t.muted, t.active);
    return buf;
}
const char* internal_menus_get_tabs_row_prefix_rmf(void) {
    if (detour_Cvar_Get::oCvar_Get) {
        cvar_t* c = detour_Cvar_Get::oCvar_Get("_sofbuddy_sb_tabs_row1_prefix_rmf", "<blank 124 1>", 0, nullptr);
        if (c && c->string && c->string[0]) return c->string;
    }
    return "<blank 124 1>";
}

bool internal_menus_deathmatch_mode_active(void) {
    if (!detour_Cvar_Get::oCvar_Get) return false;
    cvar_t* dm = detour_Cvar_Get::oCvar_Get("deathmatch", "0", 0, nullptr);
    return dm && dm->value != 0.0f;
}

bool internal_menus_use_vanilla_loading_menu(void) {
    return !internal_menus_deathmatch_mode_active();
}

bool internal_menus_should_killmenu_before_loading(void) {
    return !internal_menus_use_vanilla_loading_menu();
}

void internal_menus_sync_loading_network_ui(void) {
    // Map/download header uses <exinclude deathmatch> in loading_header.rmf (same timing as vanilla).
    set_runtime_cvar_int("_sofbuddy_loading_network", internal_menus_deathmatch_mode_active() ? 1 : 0);
}

bool internal_menus_is_mp_loading_context(void) {
    return !internal_menus_use_vanilla_loading_menu();
}

bool internal_menus_should_lock_loading_input(void) {
    if (!detour_Cvar_Get::oCvar_Get) return true;
    cvar_t* c = detour_Cvar_Get::oCvar_Get("_sofbuddy_loading_lock_input", "1", CVAR_SOFBUDDY_ARCHIVE, nullptr);
    if (!c) return true;
    return c->value != 0.0f;
}

const char* internal_menus_loading_menu_name(void) {
    // Unlocked input is for console preview during connect while keeping the same loading
    // pages (including loading_files / Disconnect) as the standard loading menu.
    return internal_menus_should_lock_loading_input() ? "loading/loading" : "loading/loading_safe";
}

static std::string loading_display_map_name(const char* map_name) {
    if (!map_name || !map_name[0]) return map_name ? map_name : "";
    std::string n(map_name);
    std::replace(n.begin(), n.end(), '\\', '/');
    while (n.rfind("./", 0) == 0) n.erase(0, 2);
    while (!n.empty() && n[0] == '/') n.erase(0, 1);
    if (n.size() >= 5 &&
        (n[0] == 'm' || n[0] == 'M') &&
        (n[1] == 'a' || n[1] == 'A') &&
        (n[2] == 'p' || n[2] == 'P') &&
        (n[3] == 's' || n[3] == 'S') &&
        n[4] == '/') {
        n.erase(0, 5);
    }
    return n;
}

void loading_set_current(const char* map_name) {
    if (!map_name || !detour_Cvar_Set2::oCvar_Set2) return;
    const std::string display = loading_display_map_name(map_name);
    detour_Cvar_Set2::oCvar_Set2(const_cast<char*>("_sofbuddy_loading_current"),
                                 const_cast<char*>(display.c_str()), true);
}

void loading_reset_current_map_unknown(void) {
    loading_set_current("-");
}

void internal_menus_call_SCR_UpdateScreen(bool force) {
    sync_sofbuddy_profile_targets_from_selection();
    if (detour_SCR_UpdateScreen::oSCR_UpdateScreen) {
        detour_SCR_UpdateScreen::oSCR_UpdateScreen(force);
    }
}
void internal_menus_EarlyStartup(void) {
    internal_menus_load_library();
    materialize_embedded_menus_to_disk();
    WriteNops(rvaToAbsExe(reinterpret_cast<void*>(0x13AAA)), 2);
    WriteNops(rvaToAbsExe(reinterpret_cast<void*>(0x13AC7)), 5);
    WriteNops(rvaToAbsExe(reinterpret_cast<void*>(0x13ACE)), 5);
}

void internal_menus_PostCvarInit(void) {
    if (!detour_Cvar_Get::oCvar_Get) {
        PrintOut(PRINT_BAD, "Internal menus: missing Cvar_Get in PostCvarInit\n");
        return;
    }

    create_loading_cvars();
    create_layout_cvars();
    update_layout_cvars(false);

    if (!detour_Cmd_AddCommand::oCmd_AddCommand) {
        PrintOut(PRINT_BAD, "Internal menus: missing Cmd_AddCommand, cannot register sofbuddy_menu\n");
        return;
    }

    detour_Cmd_AddCommand::oCmd_AddCommand(const_cast<char*>("sofbuddy_menu"), Cmd_SoFBuddy_Menu_f);
    detour_Cmd_AddCommand::oCmd_AddCommand(const_cast<char*>("sofbuddy_apply_menu_hotkey"), Cmd_SoFBuddy_Apply_Menu_Hotkey_f);
    detour_Cmd_AddCommand::oCmd_AddCommand(const_cast<char*>("sofbuddy_apply_profile"), Cmd_SoFBuddy_Apply_Profile_f);

    apply_menu_hotkey_binding();

    if (detour_Cmd_ExecuteString::oCmd_ExecuteString)
        detour_Cmd_ExecuteString::oCmd_ExecuteString("set rate 20000");
}

void Cmd_SoFBuddy_Menu_f(void) {
    if (!detour_Cmd_Argv::oCmd_Argv || !detour_Cmd_Argc::oCmd_Argc || !detour_M_PushMenu::oM_PushMenu) return;
    if (detour_Cmd_Argc::oCmd_Argc() < 2) return;

    // Keep injected RMF prefixes in sync with the current video mode before opening.
    update_layout_cvars(false);

    const char* name = detour_Cmd_Argv::oCmd_Argv(1);
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
            exists = false;
            if (requested == "sof_buddy" && try_resolve_stored_last_page(menu_to_push)) {
                exists = true;
            }
            if (!exists) {
                std::string default_file = requested;
                if (default_file.find('.') == std::string::npos) default_file += ".rmf";

                auto default_it = menu_it->second.find(default_file);
                if (default_it != menu_it->second.end()) {
                    exists = true;
                    menu_to_push = requested + "/" + stem_from_filename(default_file);
                } else {
                    auto main_it = menu_it->second.find("main.rmf");
                    if (main_it != menu_it->second.end()) {
                        exists = true;
                        menu_to_push = requested + "/main";
                    }
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
    if (!is_sofbuddy_menu && detour_Cmd_ExecuteString::oCmd_ExecuteString)
        detour_Cmd_ExecuteString::oCmd_ExecuteString("killmenu");

    if (menu_to_push == "loading/loading")
        menu_to_push = internal_menus_loading_menu_name();

    const bool is_loading_menu = (menu_to_push == "loading/loading") ||
                                 (menu_to_push == "loading/loading_safe") ||
                                 (menu_to_push.rfind("loading/", 0) == 0);
    const bool lock_input = is_loading_menu ? internal_menus_should_lock_loading_input() : false;

    // Persist last tab here (full sof_buddy/... path). Post-hook sometimes sees a leaf-only path and skipped save.
    if (menu_to_push.rfind("sof_buddy/", 0) == 0) {
        internal_menus_remember_menu_page(menu_to_push.c_str());
    }

    if (is_loading_menu)
        loading_reset_current_map_unknown();

    detour_M_PushMenu::oM_PushMenu(menu_to_push.c_str(), "", lock_input);
}

void internal_menus_OnVidChanged(void) {
    update_layout_cvars(true);
}

void loading_show_ui(void) {
    if (internal_menus_use_vanilla_loading_menu()) return;
    if (detour_M_PushMenu::oM_PushMenu) {
        internal_menus_sync_loading_network_ui();
        const bool lock_input = internal_menus_should_lock_loading_input();
        detour_M_PushMenu::oM_PushMenu(internal_menus_loading_menu_name(), "", lock_input);
    }
}

#endif
