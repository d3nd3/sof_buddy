#include "feature_config.h"

#if FEATURE_SCALED_MENU

#ifdef UI_MENU

#include <cctype>
#include <cstring>
#include "../shared.h"

namespace {

size_t normalized_menu_leaf(const char* path, char* out, size_t out_size) {
	if (!path || !path[0] || !out || out_size == 0) return 0;
	const char* end = path + std::strlen(path);
	while (end > path && (end[-1] == '/' || end[-1] == '\\')) --end;
	const char* base = path;
	for (const char* s = path; s < end; ++s) {
		if (*s == '/' || *s == '\\') base = s + 1;
	}
	size_t len = static_cast<size_t>(end - base);
	if (len > 4 && base[len - 4] == '.') {
		const char c0 = static_cast<char>(std::tolower(static_cast<unsigned char>(base[len - 3])));
		const char c1 = static_cast<char>(std::tolower(static_cast<unsigned char>(base[len - 2])));
		const char c2 = static_cast<char>(std::tolower(static_cast<unsigned char>(base[len - 1])));
		if (c0 == 'r' && c1 == 'm' && c2 == 'f') len -= 4;
	}
	if (len >= out_size) len = out_size - 1;
	for (size_t i = 0; i < len; ++i)
		out[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(base[i])));
	out[len] = '\0';
	return len;
}

} // namespace

void scaled_menu_M_PushMenu_pre(char const*& menu_file, char const*& parentFrame, bool& lock_input) {
	(void)parentFrame;
	(void)lock_input;
	if (!menu_file || !menu_file[0] || std::strcmp(menu_file, "default") == 0) return;
	char name[64];
	normalized_menu_leaf(menu_file, name, sizeof(name));
	scaled_menu_set_pushed_menu(name);
}

#endif // UI_MENU

#endif // FEATURE_SCALED_MENU
