#include "feature_config.h"

#if FEATURE_INTERNAL_MENUS

#include "sof_compat.h"
#include "update_command.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace {

std::string normalize_menu_name(const char* menu_file) {
    if (!menu_file || !menu_file[0]) return "";

    std::string name(menu_file);
    std::replace(name.begin(), name.end(), '\\', '/');
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    while (name.rfind("./", 0) == 0)
        name.erase(0, 2);
    while (!name.empty() && name[0] == '/')
        name.erase(0, 1);
    if (name.rfind("menus/", 0) == 0)
        name.erase(0, 6);
    if (name.rfind("menu/", 0) == 0)
        name.erase(0, 5);

    if (name.size() > 4 && name.compare(name.size() - 4, 4, ".rmf") == 0)
        name.erase(name.size() - 4);

    return name;
}

std::string menu_leaf(const std::string& menu_name) {
    const size_t slash = menu_name.rfind('/');
    return (slash == std::string::npos) ? menu_name : menu_name.substr(slash + 1);
}

} // namespace

void internal_menus_M_PushMenu_post(char const* menu_file, char const* parentFrame, bool lock_input) {
    (void)lock_input;

    // Show startup update prompt over the intro/start screen, once, only when requested by updater.
    if (parentFrame && parentFrame[0]) return;
    if (menu_leaf(normalize_menu_name(menu_file)) != "start") return;
    if (!sofbuddy_update_consume_startup_prompt_request()) return;
    if (!orig_Cmd_ExecuteString) return;
    orig_Cmd_ExecuteString("menu sof_buddy/sb_update_prompt");
}

#endif
