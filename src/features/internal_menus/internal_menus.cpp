#include "feature_config.h"

#if FEATURE_INTERNAL_MENUS

#include "sof_compat.h"
#include "features.h"
#include "util.h"
#include "sofbuddy_cfg.h"
#include "shared.h"
#include "generated_detours.h"
#include "generated_registrations.h"
#include "detours.h"
#include "runtime_features.h"
#include "mp_profile_guid.h"

#if FEATURE_HTTP_MAPS
#include "features/http_maps/shared.h"
#endif

#if FEATURE_MEDIA_TIMERS
#include "features/media_timers/shared.h"
#endif

#if FEATURE_VSYNC_TOGGLE
#include "features/vsync_toggle/shared.h"
#endif

#if FEATURE_MORE_CROSSHAIRS
#include "features/more_crosshairs/shared.h"
#endif

#if FEATURE_SCALED_MENU
#include "features/scaled_menu/shared.h"
#endif

#include <windows.h>
#include <cstdarg>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <map>
#include <set>
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
    cvar_t* cv = detour_Cvar_Get::oCvar_Get("_sb_internal_menu_last_page", "", CVAR_SOFBUDDY_ARCHIVE, nullptr);
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

static void* s_current_buddy_page = nullptr;

/*
    Remember which menu page was pushed by a SoF Buddy RMF.  F12 uses this
    to tell "a Buddy page is already open" apart from a stock menu, so the
    hotkey can toggle instead of pushing another page (which used to leave a
    stack that needed one Close per F12 press).
*/
void internal_menus_note_menu_page(const char* menu_file, void* page) {
    s_current_buddy_page = nullptr;
    if (!menu_file || !menu_file[0]) return;

    std::string n = normalize_menu_file_path(menu_file);
    if (n.empty()) return;
    if (n.rfind("sof_buddy/", 0) != 0) {
        if (n.find('/') != std::string::npos) return;
        auto it = g_menu_internal_files.find("sof_buddy");
        if (it == g_menu_internal_files.end()) return;
        const std::string rmf = n + ".rmf";
        if (it->second.find(rmf) == it->second.end()) return;
    }
    s_current_buddy_page = page;
}

namespace {

constexpr const char* kInternalMenusDiskRoot = "user/menus";
void set_runtime_cvar_str(const char* name, const char* value);
void set_runtime_cvar_int(const char* name, int value);

void materialize_embedded_menus_to_disk() {
    // No longer materializing to disk.
}

void sync_sofbuddy_profile_targets_from_selection(bool force) {
    if (!detour_Cvar_Get::oCvar_Get) return;
    cvar_t* perf = detour_Cvar_Get::oCvar_Get("_sofbuddy_perf_profile", "0", CVAR_SOFBUDDY_ARCHIVE, nullptr);
    const int profile = perf ? static_cast<int>(perf->value + 0.5f) : 0;
    set_runtime_cvar_str("_sb_internal_profile_state_label",
                         profile == 3 ? "In-Use:" : "Would-Set:");
    static int last_profile = -9999;
    if (!force && profile == last_profile) return;
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
    set_runtime_cvar_str("_sb_internal_profile_target_fx_maxdebrisonscreen", fx);
    set_runtime_cvar_str("_sb_internal_profile_target_cl_max_debris", debris);
    set_runtime_cvar_str("_sb_internal_profile_target_ghl_shadows", shadows);
    set_runtime_cvar_str("_sb_internal_profile_target_ghl_specular", specular);
    set_runtime_cvar_str("_sb_internal_profile_target_cl_max_quads", quads);
}

bool sofbuddy_profile_cvar_matches(const char* name, float expected) {
    if (!detour_Cvar_Get::oCvar_Get) return false;
    cvar_t* cvar = detour_Cvar_Get::oCvar_Get(name, "0", 0, nullptr);
    return cvar && cvar->value >= expected - 0.01f && cvar->value <= expected + 0.01f;
}

bool sofbuddy_profile_matches(int profile) {
    switch (profile) {
        case 0:
            return sofbuddy_profile_cvar_matches("fx_maxdebrisonscreen", 0) &&
                   sofbuddy_profile_cvar_matches("cl_max_debris", 2) &&
                   sofbuddy_profile_cvar_matches("ghl_shadows", 0) &&
                   sofbuddy_profile_cvar_matches("ghl_specular", 0) &&
                   sofbuddy_profile_cvar_matches("cl_max_quads", 0);
        case 1:
            return sofbuddy_profile_cvar_matches("fx_maxdebrisonscreen", 32) &&
                   sofbuddy_profile_cvar_matches("cl_max_debris", 32) &&
                   sofbuddy_profile_cvar_matches("ghl_shadows", 2) &&
                   sofbuddy_profile_cvar_matches("ghl_specular", 1) &&
                   sofbuddy_profile_cvar_matches("cl_max_quads", 1024);
        case 2:
            return sofbuddy_profile_cvar_matches("fx_maxdebrisonscreen", 0) &&
                   sofbuddy_profile_cvar_matches("cl_max_debris", 2) &&
                   sofbuddy_profile_cvar_matches("ghl_shadows", 2) &&
                   sofbuddy_profile_cvar_matches("ghl_specular", 1) &&
                   sofbuddy_profile_cvar_matches("cl_max_quads", 1024);
        default:
            return false;
    }
}

void sync_sofbuddy_profile_from_cvars() {
    if (!detour_Cvar_Get::oCvar_Get) return;
    int detected = 3;
    for (int profile = 0; profile < 3; ++profile) {
        if (sofbuddy_profile_matches(profile)) {
            detected = profile;
            break;
        }
    }
    cvar_t* perf = detour_Cvar_Get::oCvar_Get("_sofbuddy_perf_profile", "0",
                                               CVAR_SOFBUDDY_ARCHIVE, nullptr);
    if (perf && static_cast<int>(perf->value + 0.5f) != detected)
        set_runtime_cvar_int("_sofbuddy_perf_profile", detected);
    sync_sofbuddy_profile_targets_from_selection(true);
}

void sofbuddy_perf_profile_change(cvar_t* cvar) {
    (void)cvar;
    sync_sofbuddy_profile_targets_from_selection(false);
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

bool g_normalizing_feature_cvar = false;

void feature_cvar_change(cvar_t* cvar) {
    if (!cvar || !cvar->name || g_normalizing_feature_cvar) return;
    const RuntimeFeatures::Feature feature = RuntimeFeatures::FromCvarName(cvar->name);
    if (feature == RuntimeFeatures::Feature::Count) return;

    if (!RuntimeFeatures::Compiled(feature)) {
        if (detour_Cvar_Set2::oCvar_Set2 &&
            static_cast<int>(cvar->value + 0.5f) != 2) {
            g_normalizing_feature_cvar = true;
            detour_Cvar_Set2::oCvar_Set2(cvar->name, const_cast<char*>("2"), true);
            g_normalizing_feature_cvar = false;
        }
        return;
    }
    // Feature hooks are restart-applied; leave the startup mask unchanged.
}

void create_feature_cvars() {
    if (!detour_Cvar_Get::oCvar_Get) return;

#define FEATURE_CVAR(macro, feature_name, label) do { \
    const RuntimeFeatures::Feature feature = RuntimeFeatures::Feature::RUNTIME_##macro; \
    char default_value[2] = { static_cast<char>('0' + RuntimeFeatures::SelectionValue(feature)), '\0' }; \
    cvar_t* cv = detour_Cvar_Get::oCvar_Get("_sofbuddy_feature_" feature_name, default_value, \
                                             CVAR_SOFBUDDY_ARCHIVE, feature_cvar_change); \
    if (!cv) break; \
    cv->flags |= CVAR_SOFBUDDY_ARCHIVE; \
    cv->command = feature_cvar_change; \
    if (static_cast<int>(cv->value + 0.5f) != RuntimeFeatures::SelectionValue(feature) && \
        detour_Cvar_Set2::oCvar_Set2) { \
        g_normalizing_feature_cvar = true; \
        detour_Cvar_Set2::oCvar_Set2(cv->name, default_value, true); \
        g_normalizing_feature_cvar = false; \
    } \
} while (0);
    FEATURE_LIST(FEATURE_CVAR)
#undef FEATURE_CVAR
}

void create_loading_cvars() {
    if (!detour_Cvar_Get::oCvar_Get) return;

    create_feature_cvars();

    // Runtime loading UI state cvars.
    constexpr int kLoadingCvarFlags = 0;

	detour_Cvar_Get::oCvar_Get("_sb_internal_loading_progress", "", kLoadingCvarFlags, nullptr);
	detour_Cvar_Get::oCvar_Get("_sb_internal_loading_current", "", kLoadingCvarFlags, nullptr);
	detour_Cvar_Get::oCvar_Get("_sb_internal_loading_status", "CHECKING", kLoadingCvarFlags, nullptr);
	detour_Cvar_Get::oCvar_Get("_sb_internal_loading_network", "0", kLoadingCvarFlags, nullptr);
	detour_Cvar_Get::oCvar_Get("_sb_internal_tab", "0", 0, nullptr);
    // User preference: allow loading menu input (default), or lock it.
    detour_Cvar_Get::oCvar_Get("_sofbuddy_loading_lock_input", "0", CVAR_SOFBUDDY_ARCHIVE, nullptr);
    detour_Cvar_Get::oCvar_Get("_sofbuddy_loading_show_mapname", "1", CVAR_SOFBUDDY_ARCHIVE, nullptr);
    detour_Cvar_Get::oCvar_Get("_sofbuddy_loading_show_download", "1", CVAR_SOFBUDDY_ARCHIVE, nullptr);
    // Legacy cvar kept for compatibility with older configs and the console command.
    detour_Cvar_Get::oCvar_Get("_sofbuddy_menu_hotkey", "F12", CVAR_SOFBUDDY_ARCHIVE, nullptr);
    // Last SoF Buddy tab/page (e.g. sof_buddy/map_debug) restored when opening via F12 (sofbuddy_menu sof_buddy).
    detour_Cvar_Get::oCvar_Get("_sb_internal_menu_last_page", "", CVAR_SOFBUDDY_ARCHIVE, nullptr);
    // Performance profile selector used by Perf Tweaks page.
    detour_Cvar_Get::oCvar_Get("_sofbuddy_perf_profile", "0", CVAR_SOFBUDDY_ARCHIVE, nullptr);
    detour_Cvar_Get::oCvar_Get("_sb_internal_profile_state_label", "Would-Set:", 0, nullptr);
    detour_Cvar_Get::oCvar_Get("_sb_internal_profile_target_fx_maxdebrisonscreen", "0", 0, nullptr);
    detour_Cvar_Get::oCvar_Get("_sb_internal_profile_target_cl_max_debris", "2", 0, nullptr);
    detour_Cvar_Get::oCvar_Get("_sb_internal_profile_target_ghl_shadows", "Off", 0, nullptr);
    detour_Cvar_Get::oCvar_Get("_sb_internal_profile_target_ghl_specular", "Off", 0, nullptr);
    detour_Cvar_Get::oCvar_Get("_sb_internal_profile_target_cl_max_quads", "0", 0, nullptr);
    cvar_t* perf = detour_Cvar_Get::oCvar_Get("_sofbuddy_perf_profile", "0", CVAR_SOFBUDDY_ARCHIVE, sofbuddy_perf_profile_change);
    sofbuddy_perf_profile_change(perf);

    // Engine registers these with flags 0; OR CVAR_ARCHIVE so F12 menu changes persist in config.cfg.
    // cl_showfps/cl_maxfps are special: with SoFPlus they persist via
    // User/sofplus/data/spm_settings.cfg (saved on every change and on shutdown,
    // see sofbuddy_cfg_save_sofplus_settings), so leave them unarchived to avoid
    // a config.cfg vs spm_settings.cfg split. Without SoFPlus there is no
    // spm_settings.cfg, so archive them for config.cfg persistence instead.
    // NOTE: Cvar_Get only ORs flags when the cvar exists (never clears), so set
    // or clear the bit explicitly after resolving.
    {
        const bool sofplus_present =
            findCvar(const_cast<char*>("_sp_cl_master_gamespy")) != nullptr ||
            findCvar(const_cast<char*>("_sp_cl_cpu_cool")) != nullptr;
        cvar_t* showfps = detour_Cvar_Get::oCvar_Get("cl_showfps", "0", 0, nullptr);
        if (showfps) {
            if (sofplus_present) {
                showfps->flags &= ~CVAR_ARCHIVE;
                // Chain SoFPlus's callbacks (not replace) and save spm on change.
                sofbuddy_cfg_watch_spm_cvars();
            } else {
                showfps->flags |= CVAR_ARCHIVE;
            }
        }
        if (!sofplus_present) {
            // Best-effort only: media_timers owns cl_maxfps creation and its
            // cl_maxfps_change callback, and archives it there too. Only OR the
            // flag on the existing cvar here; never create it with a null
            // callback (Cvar_Get keeps the existing callback, so creating first
            // would orphan media_timers' change handler for this session).
            if (cvar_t* maxfps = findCvar(const_cast<char*>("cl_maxfps")))
                maxfps->flags |= CVAR_ARCHIVE;
        }
    }
    detour_Cvar_Get::oCvar_Get("fx_maxdebrisonscreen", "16", CVAR_ARCHIVE, nullptr);
    detour_Cvar_Get::oCvar_Get("m_yaw", "0.022", CVAR_ARCHIVE, nullptr);
    detour_Cvar_Get::oCvar_Get("m_pitch", "0.022", CVAR_ARCHIVE, nullptr);
    detour_Cvar_Get::oCvar_Get("cl_gun", "1", CVAR_ARCHIVE, nullptr);
    detour_Cvar_Get::oCvar_Get("_sp_cl_vid_fov", "1", CVAR_ARCHIVE, nullptr);
    detour_Cvar_Get::oCvar_Get("cl_quads", "1", CVAR_ARCHIVE, nullptr);
    detour_Cvar_Get::oCvar_Get("cl_freezequads", "0", CVAR_ARCHIVE, nullptr);
}

constexpr int kSofBuddyCenterPanelVirtualWidth = 640;

namespace {

std::string trim_ascii(const char* s) {
    if (!s) return std::string();
    while (*s == ' ' || *s == '\t') ++s;
    const char* end = s + std::strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t')) --end;
    return std::string(s, end);
}

std::string vid_mode_label_from_cvar_string(const char* raw) {
    if (!raw || !raw[0]) return std::string();
    const char* p = raw;
    const char* colon = std::strchr(raw, ':');
    while (colon) {
        const char* next = std::strchr(colon + 1, ':');
        if (!next) {
            p = colon + 1;
            break;
        }
        colon = next;
    }
    return trim_ascii(p);
}

void append_vid_mode_entry(std::string& labels, std::string& values, int index, const std::string& label) {
    if (label.empty()) return;
    if (!labels.empty()) {
        labels.push_back(',');
        values.push_back(',');
    }
    labels += label;
    values += std::to_string(index);
}

bool parse_vid_mode_dimensions(const std::string& label, int& width, int& height) {
    const char* p = label.c_str();
    char* end = nullptr;
    const long parsed_width = std::strtol(p, &end, 10);
    if (end == p || parsed_width <= 0) return false;
    while (*end == ' ' || *end == '\t') ++end;
    if (*end != 'x' && *end != 'X') return false;
    p = end + 1;
    while (*p == ' ' || *p == '\t') ++p;
    const long parsed_height = std::strtol(p, &end, 10);
    if (end == p || parsed_height <= 0) return false;
    width = static_cast<int>(parsed_width);
    height = static_cast<int>(parsed_height);
    return true;
}

struct SupportedDisplayModes {
    bool valid = false;
    std::set<std::pair<int, int>> modes;
};

struct SofVidModeResolution {
    int index;
    int width;
    int height;
};

// SoF.exe accepts gl_mode 3..9 only. Modes 0..2 exist in the table but the
// engine's mode helper rejects them, so they are never offered here.
constexpr SofVidModeResolution kSofVidModes[] = {
    {3, 640, 480},   {4, 800, 600},  {5, 960, 720},
    {6, 1024, 768},  {7, 1152, 864}, {8, 1280, 960},
    {9, 1600, 1200},
};

// Enumerate the resolutions the display driver reports for the current
// monitor. SoF's own vid_mode table (and therefore _sp_cl_vid_mode_N) includes
// old modes such as 960x720 that the monitor may not accept; selecting one
// makes the renderer fall back to a window. Fullscreen should only offer modes
// that EnumDisplaySettings knows about.
SupportedDisplayModes query_supported_display_modes() {
    SupportedDisplayModes result;
    DEVMODEA dm;
    for (DWORD i = 0; i < 4096; ++i) {
        std::memset(&dm, 0, sizeof(dm));
        dm.dmSize = sizeof(dm);
        if (!EnumDisplaySettingsA(nullptr, i, &dm)) break;
        if (dm.dmBitsPerPel != 0 && dm.dmBitsPerPel < 16) continue;
        result.modes.insert(std::make_pair(static_cast<int>(dm.dmPelsWidth),
                                           static_cast<int>(dm.dmPelsHeight)));
    }
    result.valid = !result.modes.empty();
    return result;
}

bool vid_mode_is_supported(const std::string& label, bool filter_modes,
                           const SupportedDisplayModes& supported) {
    if (!filter_modes) return true;
    int width = 0;
    int height = 0;
    // Keep labels we cannot parse rather than hiding modes silently.
    if (!parse_vid_mode_dimensions(label, width, height)) return true;
    return supported.modes.find(std::make_pair(width, height)) != supported.modes.end();
}

bool build_vid_mode_list_from_sp_cvars(std::string& labels, std::string& values,
                                       bool filter_modes,
                                       const SupportedDisplayModes& supported) {
    bool any = false;
    for (const SofVidModeResolution& mode : kSofVidModes) {
        char name[40];
        std::snprintf(name, sizeof(name), "_sp_cl_vid_mode_%d", mode.index);
        cvar_t* cv = findCvar(name);
        if (!cv || !cv->string) continue;
        const std::string label = vid_mode_label_from_cvar_string(cv->string);
        if (label.empty()) continue;
        if (!vid_mode_is_supported(label, filter_modes, supported)) continue;
        append_vid_mode_entry(labels, values, mode.index, label);
        any = true;
    }
    return any;
}

bool build_vid_mode_list_from_info_cvar(std::string& labels, std::string& values,
                                        bool filter_modes,
                                        const SupportedDisplayModes& supported) {
    cvar_t* cv = findCvar(const_cast<char*>("_sp_cl_info_vid_modes"));
    if (!cv || !cv->string || !cv->string[0]) return false;
    bool any = false;
    std::set<int> seen;
    const char* cursor = cv->string;
    while (*cursor) {
        const char* comma = std::strchr(cursor, ',');
        std::string token;
        if (comma) {
            token.assign(cursor, comma - cursor);
            cursor = comma + 1;
        } else {
            token = cursor;
            cursor += std::strlen(cursor);
        }
        token = trim_ascii(token.c_str());
        if (token.empty()) continue;

        int width = 0;
        int height = 0;
        if (!parse_vid_mode_dimensions(token, width, height)) continue;
        for (const SofVidModeResolution& mode : kSofVidModes) {
            if (mode.width != width || mode.height != height) continue;
            if (!vid_mode_is_supported(token, filter_modes, supported)) break;
            if (seen.insert(mode.index).second) {
                append_vid_mode_entry(labels, values, mode.index, token);
                any = true;
            }
            break;
        }
    }
    return any;
}

bool comma_list_contains(const std::string& list, const char* value) {
    if (!value || !value[0]) return false;
    size_t start = 0;
    while (start <= list.size()) {
        size_t end = list.find(',', start);
        if (end == std::string::npos) end = list.size();
        if (list.compare(start, end - start, value) == 0) return true;
        if (end == list.size()) break;
        start = end + 1;
    }
    return false;
}

std::string vid_mode_label_for_index(const char* index) {
    if (!index || !index[0]) return std::string();
    char name[48];
    std::snprintf(name, sizeof(name), "_sp_cl_vid_mode_%.32s", index);
    cvar_t* cv = findCvar(name);
    if (cv && cv->string) {
        const std::string label = vid_mode_label_from_cvar_string(cv->string);
        if (!label.empty()) return label;
    }
    cvar_t* info = findCvar(const_cast<char*>("_sp_cl_info_vid_mode"));
    if (info && info->string) {
        const std::string label = trim_ascii(info->string);
        if (!label.empty()) return label;
    }
    return std::string("gl_mode ") + index;
}

} // namespace

void internal_menus_refresh_vid_mode_list_rmf(void) {
    if (!detour_Cvar_Get::oCvar_Get || !detour_Cvar_Set2::oCvar_Set2) return;

    static cvar_t* rmf_cvar = nullptr;
    if (!rmf_cvar)
        rmf_cvar = detour_Cvar_Get::oCvar_Get("_sb_internal_vid_mode_list_rmf", "", 0, nullptr);

    // Match ref_gl_defaults: default 0, not 3 (community-edition refresh glitch).
    cvar_t* gl_mode = detour_Cvar_Get::oCvar_Get("gl_mode", "0", 0, nullptr);
    const char* active_mode =
        (gl_mode && gl_mode->string && gl_mode->string[0]) ? gl_mode->string : "0";

    // In fullscreen, only offer modes the monitor actually reports. SoF's
    // 960x720 (and other legacy entries) can make ref_gl fall back to a window;
    // once that happens the next fullscreen request may not recover until the
    // mode is changed. Windowed keeps the full list so odd-sized windows still
    // work.
    cvar_t* fullscreen = detour_Cvar_Get::oCvar_Get("vid_fullscreen", "1", 0, nullptr);
    const bool want_fullscreen = fullscreen && fullscreen->value != 0.0f;
    SupportedDisplayModes supported;
    if (want_fullscreen) supported = query_supported_display_modes();
    bool filter_modes = want_fullscreen && supported.valid;
    bool monitor_list_unavailable = want_fullscreen && !supported.valid;

    std::string labels;
    std::string values;
    if (!build_vid_mode_list_from_sp_cvars(labels, values, filter_modes, supported))
        build_vid_mode_list_from_info_cvar(labels, values, filter_modes, supported);

    if (filter_modes && (labels.empty() || values.empty())) {
        // Some drivers / virtual desktops report no usable modes. Showing the
        // full SoF list is better than hiding every resolution.
        filter_modes = false;
        monitor_list_unavailable = true;
        labels.clear();
        values.clear();
        if (!build_vid_mode_list_from_sp_cvars(labels, values, false, supported))
            build_vid_mode_list_from_info_cvar(labels, values, false, supported);
    }

    // Keep the active mode representable even when the filter removed it or
    // gl_mode is outside SoF's valid 3..9 range.
    if (!values.empty() && !comma_list_contains(values, active_mode)) {
        const std::string current_label = vid_mode_label_for_index(active_mode);
        if (!current_label.empty()) {
            if (!labels.empty()) {
                labels.push_back(',');
                values.push_back(',');
            }
            labels += current_label;
            labels += " (current)";
            values += active_mode;
        }
    }

    if (labels.empty() || values.empty()) {
        detour_Cvar_Set2::oCvar_Set2(const_cast<char*>("_sb_internal_vid_mode_list_rmf"),
                                     const_cast<char*>(""), true);
        set_runtime_cvar_str("_sb_internal_vid_mode_filter_note", "");
        return;
    }

    // Selecting a row only stages the choice. Applying every click ran
    // vid_restart, which rebuilt this page before the next click and made the
    // list appear to reset (the first click applied 640x480, then the next
    // click looked stuck). Keep the live gl_mode untouched until Apply.
    detour_Cvar_Get::oCvar_Get("_sb_internal_vid_mode_pending", active_mode, 0, nullptr);
    set_runtime_cvar_str("_sb_internal_vid_mode_pending", active_mode);

    const char* filter_note = "Mode list: all modes (windowed)";
    std::string tip = "Windowed: all modes. ";
    if (filter_modes) {
        filter_note = "Mode list: fullscreen-supported";
        tip = "Fullscreen: monitor-supported modes only. ";
    } else if (monitor_list_unavailable) {
        filter_note = "Mode list: all modes (no monitor data)";
        tip = "Fullscreen: all modes (monitor list unavailable). ";
    }
    tip += "Cycle, then Apply Resolution.";
    detour_Cvar_Get::oCvar_Get("_sb_internal_vid_mode_filter_note", "", 0, nullptr);
    set_runtime_cvar_str("_sb_internal_vid_mode_filter_note", filter_note);

    std::string rmf = "<list \"";
    rmf += labels;
    rmf += "\" match \"";
    rmf += values;
    rmf += "\" cvar _sb_internal_vid_mode_pending atext \"Resolution : \" noshade tip \"";
    rmf += tip;
    rmf += "\"><hbr>"
           "<text \"Apply Resolution\" key mouse1 \"sofbuddy_apply_vid_mode\" border 8 1 cyan regular align center "
           "tip \"Restart video at the resolution selected above. Browsing the list does not change the game until you press this.\"><hbr>";
    detour_Cvar_Set2::oCvar_Set2(const_cast<char*>("_sb_internal_vid_mode_list_rmf"),
                                 const_cast<char*>(rmf.c_str()), true);
}

void create_layout_cvars() {
    if (!detour_Cvar_Get::oCvar_Get) return;

    constexpr int kLayoutCvarFlags = 0;
    detour_Cvar_Get::oCvar_Get("_sb_internal_vid_mode_list_rmf", "", kLayoutCvarFlags, nullptr);
    detour_Cvar_Get::oCvar_Get("_sb_internal_vid_mode_pending", "0", kLayoutCvarFlags, nullptr);
    detour_Cvar_Get::oCvar_Get("_sb_internal_vid_mode_filter_note", "", kLayoutCvarFlags, nullptr);
    detour_Cvar_Get::oCvar_Get("_sb_internal_menu_vid_w", "640", kLayoutCvarFlags, nullptr);
    detour_Cvar_Get::oCvar_Get("_sb_internal_menu_vid_h", "480", kLayoutCvarFlags, nullptr);
    detour_Cvar_Get::oCvar_Get("_sb_internal_center_panel_px", "640", kLayoutCvarFlags, nullptr);
    detour_Cvar_Get::oCvar_Get("_sb_internal_update_release_list_frame_rmf",
        "<frame release_list 0 128 page sof_buddy/release_list_content border 0 0 clear backfill clear>",
        kLayoutCvarFlags, nullptr);
    detour_Cvar_Get::oCvar_Get("_sb_internal_tabs_row1_prefix_px", "28", kLayoutCvarFlags, nullptr);
    detour_Cvar_Get::oCvar_Get("_sb_internal_tabs_row2_prefix_px", "28", kLayoutCvarFlags, nullptr);
    detour_Cvar_Get::oCvar_Get("_sb_internal_tabs_wide", "1", kLayoutCvarFlags, nullptr);
    detour_Cvar_Get::oCvar_Get("_sb_internal_tabs_row1_prefix_rmf", "<blank 28 1>", kLayoutCvarFlags, nullptr);
    detour_Cvar_Get::oCvar_Get("_sb_internal_tabs_row1_prefix_narrow_rmf", "<blank 124 1>", kLayoutCvarFlags, nullptr);
    detour_Cvar_Get::oCvar_Get("_sofbuddy_menu_tip_font", "0", CVAR_SOFBUDDY_ARCHIVE, nullptr);
    // 0 = original SoF cursor, 1 = SoFPlus cursor. Both are served by the
    // FS_LoadFile cursor selector, so the SoFPlus art works without SoFPlus.
    // When the SoFPlus DLL is loaded its cursor is the natural default. Existing
    // configs from the first cursor build stored the old Vanilla default, so move
    // that untouched value to SoFPlus once; an explicit later choice is kept.
    const bool sofplus_loaded = sofbuddy_sofplus_present();
    const char* cursor_default = sofplus_loaded ? "1" : "0";
    cvar_t* cursor_style = detour_Cvar_Get::oCvar_Get(
        "_sofbuddy_cursor_style", cursor_default, CVAR_SOFBUDDY_ARCHIVE, nullptr);
    cvar_t* cursor_default_migrated = detour_Cvar_Get::oCvar_Get(
        "_sb_internal_cursor_style_sofplus_default", "0", CVAR_SOFBUDDY_ARCHIVE, nullptr);
    if (cursor_default_migrated && cursor_default_migrated->value == 0.0f &&
        sofplus_loaded && detour_Cvar_Set2::oCvar_Set2) {
        if (cursor_style && cursor_style->value == 0.0f)
            detour_Cvar_Set2::oCvar_Set2(const_cast<char*>("_sofbuddy_cursor_style"),
                                         const_cast<char*>("1"), true);
        detour_Cvar_Set2::oCvar_Set2(
            const_cast<char*>("_sb_internal_cursor_style_sofplus_default"),
            const_cast<char*>("1"), true);
    }
    detour_Cvar_Get::oCvar_Get("_sb_internal_tabs_row2_prefix_rmf", "<blank 28 1>", kLayoutCvarFlags, nullptr);
    detour_Cvar_Get::oCvar_Get("_sb_internal_tabs_row1_suffix_rmf", "<blank 28 1>", kLayoutCvarFlags, nullptr);
    detour_Cvar_Get::oCvar_Get("_sb_internal_tabs_row2_suffix_rmf", "<blank 28 1>", kLayoutCvarFlags, nullptr);

    // Menu color theme preset index (see kMenuThemes).
    detour_Cvar_Get::oCvar_Get("_sofbuddy_menu_theme", "21", CVAR_SOFBUDDY_ARCHIVE, nullptr);
    detour_Cvar_Get::oCvar_Get("_sofbuddy_snd_quiet_alt_tab", "1", CVAR_SOFBUDDY_ARCHIVE, nullptr);

    // SoF.exe menu init (RVA 0xC8400): Cvar_Get("tip_duration", "2500", …). Buddy default 15000 ms.
    cvar_t* tip_duration = detour_Cvar_Get::oCvar_Get("tip_duration", "2500", 0, nullptr);
    if (tip_duration && tip_duration->string && detour_Cvar_Set2::oCvar_Set2) {
        const char* s = tip_duration->string;
        const double duration = std::atof(s);
        if (!std::strcmp(s, "2500") || !std::strcmp(s, "5000") ||
            (duration >= 5480.0 && duration <= 5520.0) ||
            !std::strcmp(s, "10000"))
            detour_Cvar_Set2::oCvar_Set2(const_cast<char*>("tip_duration"), const_cast<char*>("15000"), true);
    }
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

// <cinclude> rows read these at menu parse time; keep them aligned with runtime feature state.
static void sync_internal_feature_include_cvars(void) {
    if (!detour_Cvar_Get::oCvar_Get || !detour_Cvar_Set2::oCvar_Set2) return;
#if FEATURE_ENTITY_VISUALIZER
    detour_Cvar_Get::oCvar_Get("_sb_internal_feature_entity_visualizer", "0", 0, nullptr);
    set_runtime_cvar_int("_sb_internal_feature_entity_visualizer",
                           RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_ENTITY_VISUALIZER)
                               ? 1
                               : 0);
#endif
#if FEATURE_CL_MAXFPS_SINGLEPLAYER
    detour_Cvar_Get::oCvar_Get("_sb_internal_feature_cl_maxfps_singleplayer", "0", 0, nullptr);
    const bool sp_maxfps =
        RuntimeFeatures::Compiled(RuntimeFeatures::Feature::RUNTIME_FEATURE_CL_MAXFPS_SINGLEPLAYER) &&
        RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_CL_MAXFPS_SINGLEPLAYER);
    set_runtime_cvar_int("_sb_internal_feature_cl_maxfps_singleplayer", sp_maxfps ? 1 : 0);
#endif
#if FEATURE_SCALED_MENU
    detour_Cvar_Get::oCvar_Get("_sb_internal_feature_scaled_menu", "0", 0, nullptr);
    set_runtime_cvar_int("_sb_internal_feature_scaled_menu",
                         RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_SCALED_MENU) ? 1 : 0);
#endif
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
    std::snprintf(cmd, sizeof(cmd), "bind %s \"sofbuddy_f12_menu\"", key.c_str());
    detour_Cmd_ExecuteString::oCmd_ExecuteString(cmd);
    PrintOut(PRINT_DEV, "Internal menus: bound %s to sofbuddy_f12_menu\n", key.c_str());
}

void Cmd_SoFBuddy_F12_Menu_f() {
    auto* menu_system_slot =
        reinterpret_cast<void**>(rvaToAbsExe(reinterpret_cast<void*>(0x00406554)));
    void* menu_system = menu_system_slot ? *menu_system_slot : nullptr;
    void* top_page = menu_system
                         ? *reinterpret_cast<void**>(static_cast<char*>(menu_system) + 4)
                         : nullptr;

    // Toggle: a second F12 on a Buddy page closes the whole Buddy stack.
    if (top_page && top_page == s_current_buddy_page) {
        internal_menus_close_all_pages();
        return;
    }

    if (detour_Cmd_ExecuteString::oCmd_ExecuteString)
        detour_Cmd_ExecuteString::oCmd_ExecuteString("sofbuddy_menu sof_buddy");
}

void Cmd_SoFBuddy_Apply_Menu_Hotkey_f() {
    apply_menu_hotkey_binding();
}

static void Cmd_SoFBuddy_CursorReset_f() {
    if (!detour_Cvar_Set2::oCvar_Set2) return;
    detour_Cvar_Set2::oCvar_Set2(
        const_cast<char*>("_sofbuddy_cursor_style"),
        const_cast<char*>(sofbuddy_sofplus_present() ? "1" : "0"), true);
}

void Cmd_SoFBuddy_Apply_Profile_f() {
    if (!detour_Cvar_Get::oCvar_Get) return;
    cvar_t* perf = detour_Cvar_Get::oCvar_Get("_sofbuddy_perf_profile", "0", CVAR_SOFBUDDY_ARCHIVE, sofbuddy_perf_profile_change);
    const int profile = perf ? static_cast<int>(perf->value + 0.5f) : 0;
    apply_sofbuddy_perf_profile_value(profile);
}

static float internal_menus_design_scale(void) {
#if FEATURE_SCALED_MENU
    if (RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_SCALED_MENU))
        return scaled_menu_screen_scale();
#endif
    return 1.0f;
}

void update_layout_cvars(bool trigger_reloadall_if_changed) {
    if (!detour_Cvar_Get::oCvar_Get || !detour_Cvar_Set2::oCvar_Set2) return;

    int vid_w = (viddef_width && *viddef_width > 0) ? *viddef_width : 0;
    int vid_h = (viddef_height && *viddef_height > 0) ? *viddef_height : 0;
    if (vid_w <= 0) vid_w = (current_vid_w > 0) ? current_vid_w : 640;
    if (vid_h <= 0) vid_h = (current_vid_h > 0) ? current_vid_h : 480;

    const int center_panel_px = (vid_w * kSofBuddyCenterPanelVirtualWidth + 320) / 640;
    const float s = internal_menus_design_scale();
    const float layout_scale = (std::isfinite(s) && s > 1e-6f) ? s : 1.0f;
    // `<blank>` widths are authored 640x480 units, and the Scaled Menu parse hook
    // multiplies them by menuScale at load. Center the scaled row on screen first,
    // then convert that pixel prefix back to authored units so it is not scaled twice
    // (which pushed the sixth tab past the screen edge and wrapped it at 2x).
    const auto centered_tab_prefix = [&](float row_width_units) {
        const float row_width_px = row_width_units * layout_scale;
        return std::max(0, static_cast<int>(std::lround(
            (static_cast<float>(vid_w) - row_width_px) / (2.0f * layout_scale))));
    };
    const int tabs_prefix_px = centered_tab_prefix(584.0f);
    const int tabs_prefix_narrow_px = centered_tab_prefix(392.0f);
    const bool tabs_wide = (vid_w * 3 > vid_h * 4);
    // Use the real video height for the integer slider bound. SoFPlus can expose a
    // virtual 16:9 menu height through viddef_height while pages are parsed; using it
    // made scale_int_max 1, which gave the slider min == max (0/0 -> NaN and the knob
    // warped off the left edge of the screen).
    const int real_vid_h = (current_vid_h > 0) ? current_vid_h : vid_h;
    const int scale_int_max =
        std::max(1, static_cast<int>(std::floor(static_cast<float>(real_vid_h) / 480.0f)));
    const int nav_units = tabs_wide ? 42 : 56;
    const float frame_canvas = (vid_w > 0) ? (static_cast<float>(vid_w) / 640.0f) : 1.0f;
    int release_list_units = 128;
    if (frame_canvas > 1e-6f) {
        const int screen_units = static_cast<int>(std::floor(static_cast<float>(vid_h) / frame_canvas));
        release_list_units = screen_units - nav_units - 190;
        if (release_list_units < 40) release_list_units = 40;
    }

    char row1_rmf[64];
    char row2_rmf[64];
    char row1_narrow_rmf[64];
    char release_list_frame_rmf[192];
    std::snprintf(row1_rmf, sizeof(row1_rmf), "<blank %d 1>", tabs_prefix_px);
    std::snprintf(row2_rmf, sizeof(row2_rmf), "<blank %d 1>", tabs_prefix_px);
    std::snprintf(row1_narrow_rmf, sizeof(row1_narrow_rmf), "<blank %d 1>", tabs_prefix_narrow_px);
    std::snprintf(release_list_frame_rmf, sizeof(release_list_frame_rmf),
        "<frame release_list 0 %d page sof_buddy/release_list_content border 0 0 clear backfill clear>",
        release_list_units);

    set_runtime_cvar_int("_sb_internal_menu_vid_w", vid_w);
    set_runtime_cvar_int("_sb_internal_menu_vid_h", vid_h);
    set_runtime_cvar_int("_sb_internal_center_panel_px", center_panel_px);
    set_runtime_cvar_int("_sb_internal_tabs_wide", tabs_wide ? 1 : 0);
    set_runtime_cvar_int("_sb_internal_scale_int_max", scale_int_max);
    set_runtime_cvar_int("_sb_internal_tabs_row1_prefix_px", tabs_prefix_px);
    set_runtime_cvar_int("_sb_internal_tabs_row2_prefix_px", tabs_prefix_px);
    set_runtime_cvar_str("_sb_internal_tabs_row1_prefix_rmf", row1_rmf);
    set_runtime_cvar_str("_sb_internal_tabs_row2_prefix_rmf", row2_rmf);
    set_runtime_cvar_str("_sb_internal_tabs_row1_suffix_rmf", row1_rmf);
    set_runtime_cvar_str("_sb_internal_tabs_row2_suffix_rmf", row2_rmf);
    set_runtime_cvar_str("_sb_internal_tabs_row1_prefix_narrow_rmf", row1_narrow_rmf);
    set_runtime_cvar_str("_sb_internal_update_release_list_frame_rmf", release_list_frame_rmf);

    (void)trigger_reloadall_if_changed;
}

} // namespace

void internal_menus_RefDllLoaded(char const* name) {
    (void)name;
    if (!detour_Cvar_Get::oCvar_Get) return;
    detour_Cvar_Get::oCvar_Get("gl_dynamic", "1", CVAR_ARCHIVE, nullptr);
}

void internal_menus_GameDllLoaded(void* game_export) {
    (void)game_export;
    if (!detour_Cvar_Get::oCvar_Get) return;
    detour_Cvar_Get::oCvar_Get("mskins_expression_limit", "0", CVAR_ARCHIVE, nullptr);
    detour_Cvar_Get::oCvar_Get("mskins_variety_limit", "0", CVAR_ARCHIVE, nullptr);
}

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
    if (n == "sof_buddy/update_prompt" || n == "sof_buddy/update_uptodate" ||
        n == "sof_buddy/update_downgrade" || n == "sof_buddy/feature_prompt" ||
        n == "sof_buddy/gl_modulate_warning") {
        return;
    }
    set_runtime_cvar_str("_sb_internal_menu_last_page", n.c_str());
}

static int internal_menus_content_inset_px(void) {
    int vid_w = (viddef_width && *viddef_width > 0) ? *viddef_width : 0;
    int vid_h = (viddef_height && *viddef_height > 0) ? *viddef_height : 0;
    if (vid_w <= 0) vid_w = (current_vid_w > 0) ? current_vid_w : 640;
    if (vid_h <= 0) vid_h = (current_vid_h > 0) ? current_vid_h : 480;
    int target_w = (vid_h * 4) / 3;
    if (target_w > vid_w) target_w = vid_w;
    const int gap = (vid_w - target_w) / 2;
    if (gap <= 0) return 0;
    const float s = internal_menus_design_scale();
    if (!std::isfinite(s) || s <= 1e-6f) return gap;
    return static_cast<int>(std::lround(static_cast<float>(gap) / s));
}

const char* internal_menus_get_content_inset_rmf(void) {
    static char buf[40];
    std::snprintf(buf, sizeof(buf), "<blank %d 1>", internal_menus_content_inset_px());
    return buf;
}
const char* internal_menus_get_content_inset_tall_rmf(void) {
    // The side inset is a horizontal margin only. Keeping it one pixel high stops
    // the invisible left/right spacer from inflating stm_c's content height, which
    // would make the page scrollable past its real content and shorten the vbar
    // thumb to a fraction of the space the page actually needs.
    static char buf[40];
    std::snprintf(buf, sizeof(buf), "<blank %d 1>", internal_menus_content_inset_px());
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
    {0xff0a0b09, 0xff3d4a4e, 0xffc0dde8, 0xff8fb9c9, 0xff2b2e18, 0xffd8e500, 0xff52686e, 0xffcfd818, 0xff6f888f, 0xff2e9aff}, // 38 SoF1
};
// Short names for <list> labels — engine comma-split buffer is only 240 bytes (SoF.exe sub_200C92D0).
static const char* kMenuThemeListLabels[] = {
    "Dark", "HiCon", "Amber", "Green", "SolDk", "SolLt", "Paper", "HiYel",
    "Dracula", "1Dark", "Nord", "Mono", "Tokyo", "Catpp", "Gruv", "Evrf",
    "GHDrk", "Ayu", "Palen", "Rose", "Nowl", "Cob2", "Zen", "Synth",
    "Matrix", "OLED", "GHLit", "1Lite", "Latte", "NSnow", "Sepia",
    "Kanag", "Horiz", "Oxo", "Moon", "Ember", "Candy", "Frost", "SoF1",
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
        "key mouse1 \"sofbuddy_close_menu;reloadall;sofbuddy_menu sof_buddy/sofbuddy\" "
        "key mouse2 \"sofbuddy_close_menu;reloadall;sofbuddy_menu sof_buddy/sofbuddy\" "
        "noshade tip \"Menu color preset. Tab reloads to apply tints.\"><br>",
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
    static char buf[1024];
    int idx = 0;
    if (detour_Cvar_Get::oCvar_Get) {
        cvar_t* c = detour_Cvar_Get::oCvar_Get("_sofbuddy_menu_theme", "21", CVAR_SOFBUDDY_ARCHIVE, nullptr);
        if (c) idx = static_cast<int>(c->value + 0.5f);
    }
    if (idx < 0 || idx >= static_cast<int>(sizeof(kMenuThemes) / sizeof(kMenuThemes[0]))) idx = 0;
    const MenuTheme& t = kMenuThemes[idx];
    std::snprintf(buf, sizeof(buf),
        "<tint normaltext 0x%08x><tint hilitetext 0x%08x>"
        "<tint sb_bg 0x%08x><tint sb_panel 0x%08x>"
        "<tint sb_accent 0x%08x><tint sb_subtle 0x%08x>"
        "<tint white 0x%08x><tint cyan 0x%08x>"
        "<tint gray 0x%08x><tint orange 0x%08x>"
        "<tint black 0xff000000><tint red 0xff0000ff>"
        "<tint green 0xff00ff00><tint blue 0xffff0000>"
        "<tint purple 0xff00ff00>",
        t.tip, t.hili, t.bg, t.panel,
        t.accent, t.subtle, t.fg, t.heading,
        t.muted, t.active);
    return buf;
}
const char* internal_menus_get_tabs_row_prefix_rmf(void) {
    if (detour_Cvar_Get::oCvar_Get) {
        cvar_t* c = detour_Cvar_Get::oCvar_Get("_sb_internal_tabs_row1_prefix_rmf", "<blank 28 1>", 0, nullptr);
        if (c && c->string && c->string[0]) return c->string;
    }
    return "<blank 28 1>";
}
const char* internal_menus_get_font_rmf(const char* tier) {
    static const char* names[] = {"small", "medium", "title"};
    int idx = 2;
    if (tier && std::strcmp(tier, "small") == 0) idx = 0;
    else if (tier && std::strcmp(tier, "medium") == 0) idx = 1;
    int v = 1;
    if (detour_Cvar_Get::oCvar_Get) {
        cvar_t* c = detour_Cvar_Get::oCvar_Get("_sofbuddy_menu_font_size_style", "0", CVAR_SOFBUDDY_ARCHIVE, nullptr);
        if (c) v = static_cast<int>(c->value + 0.5f);
    }
    int shift = 0;
    if (v == 0) shift = -1;
    else if (v == 2) shift = 1;
    else if (v >= 3) shift = 2;
    idx += shift;
    if (idx < 0) idx = 0;
    if (idx > 2) idx = 2;
    return names[idx];
}
const char* internal_menus_get_tipfont_rmf(void) {
    int v = 0;
    if (detour_Cvar_Get::oCvar_Get) {
        cvar_t* c = detour_Cvar_Get::oCvar_Get("_sofbuddy_menu_tip_font", "0", CVAR_SOFBUDDY_ARCHIVE, nullptr);
        if (c) v = static_cast<int>(c->value + 0.5f);
    }
    if (v <= 0) return "small";
    if (v == 1) return "medium";
    return "title";
}
const char* internal_menus_get_tabs_row_prefix_narrow_rmf(void) {
    if (detour_Cvar_Get::oCvar_Get) {
        cvar_t* c = detour_Cvar_Get::oCvar_Get("_sb_internal_tabs_row1_prefix_narrow_rmf", "<blank 124 1>", 0, nullptr);
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
    set_runtime_cvar_int("_sb_internal_loading_network", internal_menus_deathmatch_mode_active() ? 1 : 0);
}

bool internal_menus_is_mp_loading_context(void) {
    return !internal_menus_use_vanilla_loading_menu();
}

bool internal_menus_should_lock_loading_input(void) {
    if (!detour_Cvar_Get::oCvar_Get) return true;
    cvar_t* c = detour_Cvar_Get::oCvar_Get("_sofbuddy_loading_lock_input", "0", CVAR_SOFBUDDY_ARCHIVE, nullptr);
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
    detour_Cvar_Set2::oCvar_Set2(const_cast<char*>("_sb_internal_loading_current"),
                                 const_cast<char*>(display.c_str()), true);
}

void loading_reset_current_map_unknown(void) {
    loading_set_current("-");
}

void internal_menus_call_SCR_UpdateScreen(bool force) {
    sync_sofbuddy_profile_targets_from_selection(false);
    if (detour_SCR_UpdateScreen::oSCR_UpdateScreen) {
        detour_SCR_UpdateScreen::oSCR_UpdateScreen(force);
    }
}
using ComPrintfFn = void(__cdecl*)(const char*, ...);

static ComPrintfFn s_orig_Com_Printf = nullptr;
static bool s_snd_quiet_detoured = false;

static void __cdecl snd_quiet_Com_Printf(const char* fmt, ...) {
    char msg[4096];
    msg[0] = '\0';
    if (fmt) {
        va_list args;
        va_start(args, fmt);
        std::vsnprintf(msg, sizeof(msg), fmt, args);
        va_end(args);
    }
    bool quiet = false;
    if (detour_Cvar_Get::oCvar_Get) {
        cvar_t* c = detour_Cvar_Get::oCvar_Get("_sofbuddy_snd_quiet_alt_tab", "1", 0, nullptr);
        quiet = c && c->value != 0.0f;
    }
    if (quiet && (std::strstr(msg, "Creating DS buffers") ||
                  std::strstr(msg, "using secondary sound buffer") ||
                  std::strstr(msg, "Destroying DS buffers"))) {
        return;
    }
    if (s_orig_Com_Printf) s_orig_Com_Printf("%s", msg);
}

static void internal_menus_install_sound_quiet_filter(void) {
    if (s_snd_quiet_detoured) return;
    void* addr = rvaToAbsExe(reinterpret_cast<void*>(0x001C6E0));
    if (!addr) return;
    if (GetDetourSystem().ApplyDetourAtAddress(
            addr, reinterpret_cast<void*>(&snd_quiet_Com_Printf),
            reinterpret_cast<void**>(&s_orig_Com_Printf), "Com_Printf", 0)) {
        s_snd_quiet_detoured = true;
    }
}

void internal_menus_EarlyStartup(void) {
    internal_menus_load_library();
    materialize_embedded_menus_to_disk();
    WriteNops(rvaToAbsExe(reinterpret_cast<void*>(0x13AAA)), 2);
    WriteNops(rvaToAbsExe(reinterpret_cast<void*>(0x13AC7)), 5);
    WriteNops(rvaToAbsExe(reinterpret_cast<void*>(0x13ACE)), 5);
}

static bool sofbuddy_menu_command_registered = false;

/*
    Closing a menu stack: the engine's `killmenu` only pops one page
    (menu_system_c::KillPage removes the top entry and returns true while a
    parent remains).  Buddy pages are normally replaced in place, but page
    rebuilds (`reloadall;sofbuddy_menu ...`) push a fresh page on top, so a
    single killmenu can leave the previous F12 page visible behind a live
    panel.  Pop repeatedly until the menu system is gone.
*/
void internal_menus_close_all_pages(void) {
    if (!detour_Cmd_ExecuteString::oCmd_ExecuteString) return;
    auto* menu_system_slot =
        reinterpret_cast<void**>(rvaToAbsExe(reinterpret_cast<void*>(0x00406554)));
    if (!menu_system_slot) return;
    for (int i = 0; i < 32 && *menu_system_slot; ++i) {
        detour_Cmd_ExecuteString::oCmd_ExecuteString(const_cast<char*>("killmenu"));
    }
}

static void Cmd_SoFBuddy_Close_Menu_f(void) {
    internal_menus_close_all_pages();
}

// Copy the staged Graphics resolution into gl_mode, then restart video and
// reopen the page. Keeping the staged value separate means browsing the list
// never changes the resolution (or the archived gl_mode) until Apply.
static void Cmd_SoFBuddy_Apply_Vid_Mode_f(void) {
    if (!detour_Cvar_Get::oCvar_Get || !detour_Cvar_Set2::oCvar_Set2 ||
        !detour_Cmd_ExecuteString::oCmd_ExecuteString)
        return;

    cvar_t* gl_mode = detour_Cvar_Get::oCvar_Get("gl_mode", "0", 0, nullptr);
    cvar_t* pending =
        detour_Cvar_Get::oCvar_Get("_sb_internal_vid_mode_pending", "", 0, nullptr);
    if (!gl_mode || !pending || !pending->string || !pending->string[0]) return;

    const bool changed =
        !gl_mode->string || std::strcmp(gl_mode->string, pending->string) != 0;
    if (!changed) {
        detour_Cmd_ExecuteString::oCmd_ExecuteString("refresh");
        return;
    }

    detour_Cvar_Set2::oCvar_Set2(const_cast<char*>("gl_mode"), pending->string, true);
    detour_Cmd_ExecuteString::oCmd_ExecuteString(
        "vid_restart;sofbuddy_menu sof_buddy/texture");
}

static bool register_sofbuddy_menu_command(void) {
    if (sofbuddy_menu_command_registered) return true;
    if (!detour_Cmd_AddCommand::oCmd_AddCommand) return false;

    detour_Cmd_AddCommand::oCmd_AddCommand(const_cast<char*>("sofbuddy_menu"), Cmd_SoFBuddy_Menu_f);
    detour_Cmd_AddCommand::oCmd_AddCommand(const_cast<char*>("sofbuddy_close_menu"), Cmd_SoFBuddy_Close_Menu_f);
    detour_Cmd_AddCommand::oCmd_AddCommand(const_cast<char*>("sofbuddy_apply_vid_mode"),
                                           Cmd_SoFBuddy_Apply_Vid_Mode_f);
    sofbuddy_menu_command_registered = true;
    return true;
}

void internal_menus_PreCvarInit(void) {
    if (!register_sofbuddy_menu_command()) {
        PrintOut(PRINT_BAD,
                 "Internal menus: Cmd_AddCommand unavailable in PreCvarInit; "
                 "sofbuddy_menu will be registered later\n");
    }
    if (detour_Cmd_ExecuteString::oCmd_ExecuteString)
        detour_Cmd_ExecuteString::oCmd_ExecuteString(
            "bind F12 \"sofbuddy_f12_menu\"");
}

void internal_menus_PostCvarInit(void) {
    if (!detour_Cvar_Get::oCvar_Get) {
        PrintOut(PRINT_BAD, "Internal menus: missing Cvar_Get in PostCvarInit\n");
        return;
    }

    // Build flags consumed by <cinclude> in RMF pages. A menu section may list cvars that
    // only exist when their feature is compiled in; gating on these keeps the page honest
    // instead of rendering blank <ctext> rows for unregistered cvars.
    // NOTE: <cinclude> includes when the cvar is non-zero; <cninclude> is its inverse.
    sync_internal_feature_include_cvars();

    create_loading_cvars();
    create_layout_cvars();
    internal_menus_feature_prompt_init_cvars();
    update_layout_cvars(false);

    if (!detour_Cmd_AddCommand::oCmd_AddCommand) {
        PrintOut(PRINT_BAD, "Internal menus: missing Cmd_AddCommand in PostCvarInit\n");
        return;
    }

    register_sofbuddy_menu_command();
    internal_menus_feature_prompt_init_command();
    detour_Cmd_AddCommand::oCmd_AddCommand(const_cast<char*>("sofbuddy_f12_menu"), Cmd_SoFBuddy_F12_Menu_f);
    internal_menus_install_sound_quiet_filter();
    detour_Cmd_AddCommand::oCmd_AddCommand(const_cast<char*>("sofbuddy_apply_menu_hotkey"), Cmd_SoFBuddy_Apply_Menu_Hotkey_f);
    detour_Cmd_AddCommand::oCmd_AddCommand(const_cast<char*>("sofbuddy_cursor_reset"), Cmd_SoFBuddy_CursorReset_f);
    detour_Cmd_AddCommand::oCmd_AddCommand(const_cast<char*>("sofbuddy_apply_profile"), Cmd_SoFBuddy_Apply_Profile_f);
    mp_profile_guid_register();
    mp_profile_guid_autosync();

    if (detour_Cmd_ExecuteString::oCmd_ExecuteString) {
        detour_Cmd_ExecuteString::oCmd_ExecuteString("bind F12 \"sofbuddy_f12_menu\"");
        detour_Cmd_ExecuteString::oCmd_ExecuteString("set rate 20000");
    }
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

    if (menu_to_push == "sof_buddy/cpu") {
        sync_internal_feature_include_cvars();
        sync_sofbuddy_profile_from_cvars();
    }

#if FEATURE_MORE_CROSSHAIRS
    if (menu_to_push == "sof_buddy/misc")
        more_crosshairs_refresh_menu_preview();
#endif

    if (menu_to_push == "sof_buddy/mp_profile_guid")
        mp_profile_guid_on_menu_open();

#if FEATURE_MEDIA_TIMERS
    // Pre-quantize cl_maxfps before the CPU page parses (legacy path when
    // list_match_fix is off: off-list values would otherwise fall back to
    // index 0/"Auto" and reset to 923). With list_match_fix on, unmatched
    // values are preserved and shown as-is, so skip the forced quantize.
    if (menu_to_push == "sof_buddy/cpu") {
#if FEATURE_LIST_MATCH_FIX
        if (!RuntimeFeatures::Enabled(
                RuntimeFeatures::Feature::RUNTIME_FEATURE_LIST_MATCH_FIX)) {
            sofbuddy_quantize_cl_maxfps();
        }
#else
        sofbuddy_quantize_cl_maxfps();
#endif
    }
#endif

#if FEATURE_VSYNC_TOGGLE
    // Same idea for the live display reading below the Framerate rows.
    if (menu_to_push == "sof_buddy/cpu") {
        sofbuddy_refresh_display_info();
    }
#endif

    if (menu_to_push == "sof_buddy/texture")
        internal_menus_refresh_vid_mode_list_rmf();

    if (is_loading_menu)
        loading_reset_current_map_unknown();

#if FEATURE_MORE_CROSSHAIRS
    more_crosshairs_set_active_buddy_page(nullptr);
#endif

    if (is_sofbuddy_menu && detour_Cmd_ExecuteString::oCmd_ExecuteString) {
        char menu_cmd[256];
        std::snprintf(menu_cmd, sizeof(menu_cmd), "menu %s", menu_to_push.c_str());
        detour_Cmd_ExecuteString::oCmd_ExecuteString(menu_cmd);
    } else {
        internal_menus_push_menu_hooked(menu_to_push.c_str(), "", lock_input);
    }

#if FEATURE_MORE_CROSSHAIRS
    more_crosshairs_set_active_buddy_page(menu_to_push.c_str());
#endif
}

void internal_menus_push_menu_hooked(const char* menu, const char* parent, bool lock_input) {
    using PushMenuFn = void(__cdecl*)(const char*, const char*, bool);
    auto fn = reinterpret_cast<PushMenuFn>(rvaToAbsExe(reinterpret_cast<void*>(0x00C7630)));
    if (fn) fn(menu, parent, lock_input);
}

void internal_menus_OnVidChanged(void) {
    update_layout_cvars(true);
}

static bool g_loading_menu_shown = false;

bool internal_menus_engine_shows_loading_plaque(void) {
    // SCR_BeginLoadingPlaque returns before "menu loading" on these.
    // The post hook still runs, so without this check we invent a menu
    // vanilla never opens (reconnect typed from the console) and the
    // one-shot close never takes it down.
    auto* shutting = (unsigned char*)rvaToAbsExe((void*)0x1C1F24);
    if (shutting && *shutting) return false;
    auto* key_dest = (int*)rvaToAbsExe((void*)0x1C1F04);
    if (key_dest && *key_dest == 1) return false;
    if (!detour_Cvar_Get::oCvar_Get) return true;
    cvar_t* ded = detour_Cvar_Get::oCvar_Get("dedicated", "0", 0, nullptr);
    if (ded && ded->value != 0.0f) return false;
    cvar_t* dev = detour_Cvar_Get::oCvar_Get("developer", "0", 0, nullptr);
    return !(dev && dev->value != 0.0f);
}

void internal_menus_note_loading_shown(void) { g_loading_menu_shown = true; }

void internal_menus_scr_updatescreen_post(bool force) {
    (void)force;
    internal_menus_feature_prompt_tick();
    if (!g_loading_menu_shown) return;
    auto* state = (int*)rvaToAbsExe((void*)0x1C1F00);
    auto* prepped = (unsigned char*)rvaToAbsExe((void*)0x1E7594);
    if (!state || !prepped || *state != 8 || !*prepped) return;
    auto off = (void(*)())rvaToAbsExe((void*)0xC7380);
    if (off) off();
    g_loading_menu_shown = false;
}

void loading_show_ui(void) {
    if (!RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_INTERNAL_MENUS)) return;
    if (internal_menus_use_vanilla_loading_menu()) return;
    if (detour_M_PushMenu::oM_PushMenu) {
        internal_menus_sync_loading_network_ui();
        const bool lock_input = internal_menus_should_lock_loading_input();
        internal_menus_push_menu_hooked(internal_menus_loading_menu_name(), "", lock_input);
        internal_menus_note_loading_shown();
    }
}

#endif
