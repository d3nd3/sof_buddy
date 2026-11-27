#include "feature_config.h"

#if FEATURE_SCALED_MENU

#ifdef UI_MENU

#include <cctype>
#include <cstring>
#include <string>
#include "../shared.h"
#include "util.h"

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

bool contains_ci(const std::string& text, const char* needle) {
	const size_t needle_len = std::strlen(needle);
	if (needle_len == 0 || text.size() < needle_len) return false;
	for (size_t i = 0; i + needle_len <= text.size(); ++i) {
		bool match = true;
		for (size_t j = 0; j < needle_len; ++j) {
			char a = text[i + j];
			char b = needle[j];
			if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
			if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
			if (a != b) {
				match = false;
				break;
			}
		}
		if (match) return true;
	}
	return false;
}

} // namespace

void scaled_menu_M_PushMenu_pre(char const*& menu_file, char const*& parentFrame, bool& lock_input) {
	(void)parentFrame;
	(void)lock_input;
	if (!menu_file || !menu_file[0] || std::strcmp(menu_file, "default") == 0) return;
	char name[64];
	normalized_menu_leaf(menu_file, name, sizeof(name));

	const bool native_requested = scaled_menu_consume_menu_native_request();
	const bool internal_menu = contains_ci(menu_file, "sof_buddy/");
	// The 4:3->wide frame height correction is on by default for every menu.
	// That includes loose user menus and pages the game pushes later, so an
	// unknown or automatically spawned menu is never left with the stock
	// horizontal canvas. Authors who authored a native canvas can opt out with
	// menu_native; internal sof_buddy pages always keep their authored canvas.
	const bool skip_vertical_shrink = internal_menu || native_requested;
	const bool loading_footer =
	    contains_ci(std::string(menu_file), "loading/loading");
	scaled_menu_set_pushed_menu(name, skip_vertical_shrink, loading_footer);
}

#endif // UI_MENU

#endif // FEATURE_SCALED_MENU
