#include "feature_config.h"

#if FEATURE_SCALED_MENU

#include "menu_rmf_transform.h"

#include "features/scaled_menu/shared.h"
#include "features/scaled_ui_base/shared.h"
#include "sof_compat.h"
#include "util.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

std::string normalize_menu_path(const char* path) {
    std::string normalized(path ? path : "");
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    for (char& c : normalized)
        if (c == '\\') c = '/';
    return normalized;
}

bool is_ns_menu_path(const char* path) {
    const std::string normalized = normalize_menu_path(path);
    return normalized.find("menus/") != std::string::npos && normalized.size() >= 7 &&
           normalized.compare(normalized.size() - 7, 7, "_ns.rmf") == 0;
}

// Menus whose outer wrapper gets the 16:9 chrome. Everything else (player,
// content pages, etc.) keeps its _ns layout.
bool is_wide_chrome_stem(const std::string& stem) {
    static const char* const kMenus[] = {
        "join",    "keys",     "game",     "lock",     "bots",     "chat",
        "load",    "mail",     "misc",     "motd",     "save",     "audio",
        "stats",   "game2",    "game3",    "scale",    "socks",    "start",
        "video",   "outfit",   "advjoin",  "credits",  "dmflags",  "options",
        "pickbot", "joystick", "objectives", "testmotd", "language", "player",
    };
    for (const char* menu : kMenus) {
        if (stem == menu) return true;
    }
    return false;
}

void screen_dimensions(int& width, int& height) {
    width = current_vid_w;
    height = current_vid_h;
    if (width > 0 && height > 0) return;
    if (!viddef_width)
        viddef_width = static_cast<int*>(rvaToAbsExe(reinterpret_cast<void*>(0x0040365C)));
    if (!viddef_height)
        viddef_height = static_cast<int*>(rvaToAbsExe(reinterpret_cast<void*>(0x00403660)));
    width = viddef_width ? *viddef_width : 0;
    height = viddef_height ? *viddef_height : 0;
}

// Same ratio the frame-height hook uses: vertical design scale over the menu
// canvas. At 1920x1080 this is 640 * (1080/1920) / 480 = 0.75.
float vertical_shrink_ratio() {
    int width = 0;
    int height = 0;
    screen_dimensions(width, height);
    if (width <= 0 || height <= 0) return 1.0f;
    return (static_cast<float>(height) / 480.0f) / (static_cast<float>(width) / 640.0f);
}

// 16:9 centre width: the 4:3-equivalent width of the screen height, expressed in
// the 640-wide menu canvas. At 1920x1080 this is 480.
int wide_main_width() {
    int width = 0;
    int height = 0;
    screen_dimensions(width, height);
    if (width <= 0 || height <= 0) return 480;
    const double main =
        ((static_cast<double>(height) * 4.0 / 3.0) / static_cast<double>(width)) * 640.0;
    int result = static_cast<int>(std::lround(main));
    if (result < 1) result = 1;
    if (result > 640) result = 640;
    return result;
}

bool scale_keyword_number(std::string& text, const char* key, bool float_value, float factor) {
    const size_t key_length = std::strlen(key);
    size_t pos = 0;
    while ((pos = text.find(key, pos)) != std::string::npos) {
        const bool left_ok = pos == 0 || std::isspace(static_cast<unsigned char>(text[pos - 1]));
        const size_t after = pos + key_length;
        const bool right_ok =
            after < text.size() && std::isspace(static_cast<unsigned char>(text[after]));
        if (!left_ok || !right_ok) {
            pos = after;
            continue;
        }

        size_t p = after;
        while (p < text.size() && std::isspace(static_cast<unsigned char>(text[p]))) ++p;
        const size_t number_start = p;
        if (p < text.size() && (text[p] == '-' || text[p] == '+')) ++p;
        if (p >= text.size() || !std::isdigit(static_cast<unsigned char>(text[p]))) {
            pos = after;
            continue;
        }
        while (p < text.size() && std::isdigit(static_cast<unsigned char>(text[p]))) ++p;
        if (float_value && p < text.size() && text[p] == '.') {
            ++p;
            while (p < text.size() && std::isdigit(static_cast<unsigned char>(text[p]))) ++p;
        }
        const size_t number_end = p;

        const float value =
            static_cast<float>(
                std::atof(text.substr(number_start, number_end - number_start).c_str())) *
            factor;
        char buffer[32];
        if (float_value)
            std::snprintf(buffer, sizeof(buffer), "%.2f", value);
        else
            std::snprintf(buffer, sizeof(buffer), "%d",
                          static_cast<int>(std::lround(value)));
        text.replace(number_start, number_end - number_start, buffer);
        return true;
    }
    return false;
}

// SoFPlus's `_ss` pages used the same <tab> grid as `_ns` scaled by the wide ratio
// (130/170, 162/214, 252/332, 396/520 ~= 0.75). The `_ns` page is now rendered in the
// 0.75-shrunk ss_content centre, so its column positions need that same ratio or the
// server box lays out wider than its page (the server rows fall outside the clip).
bool scale_tab_positions(std::string& content, float factor) {
    if (!(factor > 0.0f) || std::fabs(factor - 1.0f) < 0.001f) return false;
    bool changed = false;
    size_t pos = 0;
    while ((pos = content.find("<tab ", pos)) != std::string::npos) {
        const size_t end = content.find('>', pos);
        if (end == std::string::npos) break;
        std::string line = content.substr(pos, end - pos + 1);
        bool line_changed = false;
        size_t p = 4;
        while (p < line.size()) {
            if (std::isspace(static_cast<unsigned char>(line[p]))) {
                ++p;
                continue;
            }
            if (!std::isdigit(static_cast<unsigned char>(line[p]))) break;
            const size_t start = p;
            while (p < line.size() && std::isdigit(static_cast<unsigned char>(line[p]))) ++p;
            const int value = std::atoi(line.substr(start, p - start).c_str());
            const int scaled = static_cast<int>(std::lround(static_cast<float>(value) * factor));
            char buffer[16];
            std::snprintf(buffer, sizeof(buffer), "%d", scaled);
            line.replace(start, p - start, buffer);
            p = start + std::strlen(buffer);
            line_changed = true;
        }
        if (line_changed) {
            content.replace(pos, end - pos + 1, line);
            pos += line.size();
            changed = true;
        } else {
            pos = end + 1;
        }
    }
    return changed;
}

// bghoul scale/yoff are authored in canvas space; apply only the vertical-vs-canvas
// ratio so they track the vertically shrunk frames.
bool scale_bghoul_for_wide_aspect(std::string& content, float factor) {
    if (!(factor > 0.0f) || std::fabs(factor - 1.0f) < 0.001f) return false;
    bool changed = false;
    size_t pos = 0;
    while ((pos = content.find("<bghoul ", pos)) != std::string::npos) {
        const size_t end = content.find('>', pos);
        if (end == std::string::npos) break;
        std::string line = content.substr(pos, end - pos + 1);
        bool line_changed = scale_keyword_number(line, "xoff", false, factor);
        line_changed = scale_keyword_number(line, "yoff", false, factor) || line_changed;
        line_changed = scale_keyword_number(line, "scale", true, factor) || line_changed;
        if (line_changed) {
            content.replace(pos, end - pos + 1, line);
            pos += line.size();
            changed = true;
        } else {
            pos = end + 1;
        }
    }
    return changed;
}

std::string s_ss_content_ns_page;

bool has_word(const std::string& text, const char* word) {
    const size_t length = std::strlen(word);
    size_t pos = 0;
    while ((pos = text.find(word, pos)) != std::string::npos) {
        const bool left = pos == 0 || std::isspace(static_cast<unsigned char>(text[pos - 1]));
        const size_t after = pos + length;
        const bool right = after >= text.size() ||
                           std::isspace(static_cast<unsigned char>(text[after])) ||
                           text[after] == '>';
        if (left && right) return true;
        pos = after;
    }
    return false;
}

bool is_ss_content_page(const char* path) {
    if (s_ss_content_ns_page.empty()) return false;
    const std::string normalized = normalize_menu_path(path);
    const size_t slash = normalized.rfind('/');
    const std::string leaf =
        slash == std::string::npos ? normalized : normalized.substr(slash + 1);
    return leaf == s_ss_content_ns_page + ".rmf";
}

// Frames inside a page are linked to the menu's default frame unless they carry
// an explicit cut. Add cut ss_content to the page's top-level frames so they
// become children of the ss_content frame from the outer wrapper. The frame
// width/height ratio is applied by the scaled_menu frame hook.
bool add_ss_content_cut(std::string& content) {
    bool changed = false;
    size_t pos = 0;
    while ((pos = content.find("<frame ", pos)) != std::string::npos) {
        const size_t end = content.find('>', pos);
        if (end == std::string::npos) break;
        std::string line = content.substr(pos, end - pos + 1);
        if (!has_word(line, "cut")) {
            line.insert(line.size() - 1, " cut ss_content");
            content.replace(pos, end - pos + 1, line);
            pos += line.size();
            changed = true;
        } else {
            pos = end + 1;
        }
    }
    return changed;
}

bool shortscreen_selected() {
    using CvarVariableValue_t = float(__cdecl*)(const char*);
    auto cvar_value = reinterpret_cast<CvarVariableValue_t>(
        rvaToAbsExe(reinterpret_cast<void*>(0x000216D0)));
    return cvar_value && cvar_value("_spf_cl_video_shortscreen") != 0.0f;
}

// Replace the outer wrapper's <exinclude _spf_cl_video_shortscreen ss ns> with
// the 16:9 chrome. The real _ns page is loaded into ss_content and cut to the
// full-height centre frame; the side frames are plain 16:9 pages.
bool inject_outer_16_9_wrapper(std::string& content) {
    static const char kTag[] = "<exinclude _spf_cl_video_shortscreen ";
    const size_t tag_len = sizeof(kTag) - 1;
    const size_t pos = content.find(kTag);
    if (pos == std::string::npos) return false;
    const size_t end = content.find('>', pos);
    if (end == std::string::npos) return false;
    if (!shortscreen_selected()) return false;

    size_t p = pos + tag_len;
    auto read_token = [&](std::string& out) -> bool {
        while (p < end && std::isspace(static_cast<unsigned char>(content[p]))) ++p;
        const size_t start = p;
        while (p < end && !std::isspace(static_cast<unsigned char>(content[p])) &&
               content[p] != '>')
            ++p;
        out.assign(content, start, p - start);
        return !out.empty();
    };
    std::string ss_name;
    std::string ns_name;
    if (!read_token(ss_name) || !read_token(ns_name)) return false;

    std::string stem = ss_name;
    const size_t slash = stem.rfind('/');
    if (slash != std::string::npos) stem = stem.substr(slash + 1);
    if (stem.size() >= 4 && stem.compare(stem.size() - 4, 4, ".rmf") == 0)
        stem.resize(stem.size() - 4);
    if (stem.size() >= 3 && stem.compare(stem.size() - 3, 3, "_ss") == 0)
        stem.resize(stem.size() - 3);
    if (!is_wide_chrome_stem(stem)) return false;

    std::string ns_page = ns_name;
    if (ns_page.size() >= 4 && ns_page.compare(ns_page.size() - 4, 4, ".rmf") == 0)
        ns_page.resize(ns_page.size() - 4);

    const int main_width = wide_main_width();
    const int side_width = (640 - main_width) / 2;
    char block[512];
    std::snprintf(block, sizeof(block),
                  "<frame 16_9_center 0 0>\n"
                  "<frame 16_9_left %d 0 page 16_9_l cut 16_9_center>\n"
                  "<frame ss_content %d 0 page %s cut 16_9_center>\n"
                  "<frame 16_9_right %d 0 page 16_9_r cut 16_9_center>",
                  side_width, main_width, ns_page.c_str(), side_width);
    content.replace(pos, end - pos + 1, block);
    s_ss_content_ns_page = ns_page;
    return true;
}

bool redirect_ss_path_to_ns(char* path) {
    if (!path) return false;
    char* base = path;
    for (char* p = path; *p; ++p)
        if (*p == '/' || *p == '\\') base = p + 1;
    char* end = base + std::strlen(base);
    if (end - base >= 4 && _strnicmp(end - 4, ".rmf", 4) == 0) end -= 4;
    if (end - base >= 3 && _strnicmp(end - 3, "_ss", 3) == 0) {
        end[-2] = 'n';
        end[-1] = 's';
        return true;
    }
    return false;
}

} // namespace

bool scaled_menu_redirect_ss_path(char* path) {
    if (!redirect_ss_path_to_ns(path)) return false;
    scaled_menu_note_ss_source();
    return true;
}

bool scaled_menu_transform_rmf_content(const char* path, std::string& content) {
    bool changed = false;
    // The real _ns page gets only the bghoul ratio adjustment; the outer wrapper
    // (the <exinclude> file) is what receives the 16:9 chrome.
    if (is_ns_menu_path(path)) {
        const float ratio = vertical_shrink_ratio();
        changed = scale_bghoul_for_wide_aspect(content, ratio);
        // Pages inside the 16:9 chrome are rendered in the vertically shrunk centre;
        // their authored <tab> grid has to follow the same ratio as the frames.
        if (scaled_menu_ss_content_page_active())
            changed = scale_tab_positions(content, ratio) || changed;
        if (is_ss_content_page(path)) {
            changed = add_ss_content_cut(content) || changed;
            scaled_menu_note_ss_content_page(true);
            s_ss_content_ns_page.clear();
        }
    }
    changed = inject_outer_16_9_wrapper(content) || changed;
    return changed;
}

#endif // FEATURE_SCALED_MENU
