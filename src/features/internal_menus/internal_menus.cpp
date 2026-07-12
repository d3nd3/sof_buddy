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
// Menu color themes: (bg, fg, secondary/accent, dim, panel) as ABGR (0xAABBGGRR).
// Palettes sourced from popular editor themes (Dracula, Nord, One Dark, Tokyo Night, etc.).
struct MenuTheme { unsigned bg, fg, sec, dim, panel; };
static const MenuTheme kMenuThemes[] = {
    {0xff050913, 0xfff3f7ff, 0xff43ffd0, 0xff2b3b55, 0xff101b2b}, // 0 Dark
    {0xff000000, 0xffffffff, 0xff4ad5ff, 0xffa0a0a0, 0xff161616}, // 1 Hi Contrast
    {0xff00050a, 0xff00b0ff, 0xff7fd2ff, 0xff106aa0, 0xff000f1a}, // 2 Amber
    {0xff001100, 0xff66ff33, 0xffc4ff9e, 0xff4f8a2f, 0xff002600}, // 3 Green
    {0xff362b00, 0xffa1a193, 0xff98a12a, 0xff756e58, 0xff423607}, // 4 Solar Dark
    {0xffe3f6fd, 0xff423607, 0xffd28b26, 0xffa1a193, 0xffd5e8ee}, // 5 Solar Light
    {0xfff0f5f5, 0xff1a1a1a, 0xff875f00, 0xff707070, 0xffe0e6e6}, // 6 Paper
    {0xff000000, 0xff00ffff, 0xffffff00, 0xff00b0b0, 0xff101010}, // 7 Hi Yellow
    {0xff362a28, 0xfff2f8f8, 0xfffde98b, 0xffa47262, 0xff5a4744}, // 8 Dracula
    {0xff342c28, 0xffbfb2ab, 0xffefaf61, 0xff70635c, 0xff2b2521}, // 9 One Dark
    {0xff40342e, 0xfff4efec, 0xffd0c088, 0xff6a564c, 0xff52423b}, // 10 Nord
    {0xff222827, 0xfff2f8f8, 0xff2ee2a6, 0xff5e7175, 0xff1c1f1e}, // 11 Monokai
    {0xff261b1a, 0xfff5cac0, 0xfff7a27a, 0xff895f56, 0xff1e1616}, // 12 Tokyo Night
    {0xff2e1e1e, 0xfff4d6cd, 0xfffab489, 0xff86706c, 0xff251818}, // 13 Catppuccin
    {0xff282828, 0xffb2dbeb, 0xff98a583, 0xff748392, 0xff21201d}, // 14 Gruvbox
    {0xff3b352d, 0xffaac6d3, 0xff80c0a7, 0xff78847a, 0xff2e2a23}, // 15 Everforest
    {0xff17110d, 0xffd9d1c9, 0xffffa658, 0xff9e948b, 0xff221b16}, // 16 GitHub Dark
    {0xff140e0a, 0xffb6bdbf, 0xffe6ba39, 0xff736a62, 0xff1a130f}, // 17 Ayu Dark
    {0xff3e2d29, 0xffcdaca6, 0xffffaa82, 0xff956e67, 0xff33221f}, // 18 Palenight
    {0xff241719, 0xfff4dee0, 0xffe7a7c4, 0xff866a6e, 0xff2e1d1f}, // 19 Rose Pine
    {0xff271601, 0xffebded6, 0xffffaa82, 0xff777763, 0xff42290b}, // 20 Night Owl
    {0xff493519, 0xffffffff, 0xff00c6ff, 0xffff8800, 0xff382712}, // 21 Cobalt2
    {0xff3f3f3f, 0xffccdcdc, 0xff809070, 0xffafaf9f, 0xff333333}, // 22 Zenburn
    {0xff2f1b24, 0xfff1eff0, 0xffdb7eff, 0xffbd8b84, 0xff23141a}, // 23 Synthwave
    {0xff08020d, 0xff41ff00, 0xff33cc00, 0xff118f00, 0xff001a00}, // 24 Matrix
    {0xff000000, 0xffe0e0e0, 0xff76e600, 0xff888888, 0xff0a0a0a}, // 25 OLED
    {0xffffffff, 0xff2f2924, 0xffda6909, 0xff766d65, 0xfffaf8f6}, // 26 GitHub Light
    {0xfffafafa, 0xff423a38, 0xfff27840, 0xffa7a1a0, 0xfff0f0f0}, // 27 One Light
    {0xfff5f1ef, 0xff694f4c, 0xfff5661e, 0xffb0a09c, 0xffefe9e6}, // 28 Cat Latte
    {0xfff4efec, 0xff40342e, 0xffac815e, 0xff6a564c, 0xfff0e9e5}, // 29 Nord Snow
    {0xffd8ecf4, 0xff223443, 0xff13458b, 0xff556a7a, 0xffcfe3eb}, // 30 Sepia
};
// Short names for <list> labels — engine comma-split buffer is only 240 bytes (SoF.exe sub_200C92D0).
static const char* kMenuThemeListLabels[] = {
    "Dark", "HiCon", "Amber", "Green", "SolDrk", "SolLit", "Paper", "HiYel",
    "Dracula", "OneDark", "Nord", "Monokai", "Tokyo", "Catpp", "Gruvbox", "Evrfrst",
    "GHDrk", "AyuDark", "Palenight", "Rose", "NOwl", "Cobalt2", "Zenburn", "Synth",
    "Matrix", "OLED", "GHLit", "OneLight", "Latte", "NSnow", "Sepia",
};
static_assert(sizeof(kMenuThemeListLabels) / sizeof(kMenuThemeListLabels[0]) ==
              sizeof(kMenuThemes) / sizeof(kMenuThemes[0]), "theme list/name count mismatch");

const char* internal_menus_get_theme_list_labels_rmf(void) {
    static char buf[256];
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
        t.fg, t.bg, t.bg, t.panel,
        t.sec, t.dim, t.fg, t.sec,
        t.dim, t.fg);
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
