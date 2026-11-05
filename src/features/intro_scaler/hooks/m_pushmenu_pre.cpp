#include "feature_config.h"

#if FEATURE_INTRO_SCALER

#ifdef UI_MENU

#include "../shared.h"
#include <cctype>
#include <cstring>

namespace {

bool path_is_intro_menu(const char* path) {
	if (!path || !path[0]) return false;
	const char* end = path + std::strlen(path);
	while (end > path && (end[-1] == '/' || end[-1] == '\\')) --end;
	const char* base = path;
	for (const char* s = path; s < end; ++s) {
	 if (*s == '/' || *s == '\\') base = s + 1;
	}
	const size_t len = static_cast<size_t>(end - base);
	if (len == 5 && std::strncmp(base, "intro", 5) == 0) return true;
	if (len > 6 && std::strncmp(base + len - 6, "/intro", 6) == 0) return true;
	return false;
}

bool path_is_disclaimer_menu(const char* path) {
	if (!path || !path[0]) return false;
	const char* end = path + std::strlen(path);
	while (end > path && (end[-1] == '/' || end[-1] == '\\')) --end;
	const char* base = path;
	for (const char* s = path; s < end; ++s) {
	 if (*s == '/' || *s == '\\') base = s + 1;
	}
	const size_t len = static_cast<size_t>(end - base);
	if (len == 10 && std::strncmp(base, "disclaimer", 10) == 0) return true;
	if (len == 12 && std::strncmp(base, "m_disclaimer", 12) == 0) return true;
	if (len == 15 && std::strncmp(base, "m_disclaimer_lv", 15) == 0) return true;
	if (len > 11 && std::strncmp(base + len - 11, "/disclaimer", 11) == 0) return true;
	return false;
}

} // namespace

void intro_scaler_M_PushMenu_pre(char const*& menu_file, char const*& parentFrame, bool& lock_input) {
	(void)parentFrame;
	(void)lock_input;
	intro_scaler_on_startup_menu_pushed(path_is_disclaimer_menu(menu_file), path_is_intro_menu(menu_file));
}

#endif // UI_MENU

#endif // FEATURE_INTRO_SCALER
