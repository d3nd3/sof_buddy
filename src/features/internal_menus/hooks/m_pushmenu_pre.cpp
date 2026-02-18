#include "feature_config.h"

#if FEATURE_INTERNAL_MENUS

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

void internal_menus_M_PushMenu_pre(char const*& menu_file, char const*& parentFrame, bool& lock_input) {
    std::string menu_name = normalize_menu_name(menu_file);
    const std::string leaf = menu_leaf(menu_name);

    if ((!parentFrame || !parentFrame[0]) &&
        leaf == "disclaimer" &&
        sofbuddy_update_consume_startup_prompt_request()) {
        menu_file = "sof_buddy/sb_update_prompt";
        menu_name = "sof_buddy/sb_update_prompt";
    }
}

#endif
