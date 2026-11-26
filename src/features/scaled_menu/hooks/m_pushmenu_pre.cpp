#include "feature_config.h"

#if FEATURE_SCALED_MENU

#ifdef UI_MENU

#include <cctype>
#include <cstdio>
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

bool file_exists(const std::string& path) {
	FILE* file = std::fopen(path.c_str(), "rb");
	if (!file) return false;
	std::fclose(file);
	return true;
}

std::string normalized_menu_rel(const char* path) {
	std::string rel = path ? path : "";
	for (char& c : rel) {
		if (c == '\\') c = '/';
	}
	while (rel.size() >= 2 && rel.compare(0, 2, "./") == 0) rel.erase(0, 2);
	while (!rel.empty() && rel[0] == '/') rel.erase(0, 1);
	if (rel.size() < 4 || rel.compare(rel.size() - 4, 4, ".rmf") != 0) rel += ".rmf";
	return rel;
}

// User menus live on disk under User/menus or base/menus (or as loose files in
// the game root). Stock menus are served from the numbered/SoFPlus paks. The
// default `menu` command skips the 4:3->wide frame correction for these loose
// menus; `menu_wide` opts them back in.
bool menu_file_is_user_file(const char* path) {
	if (!path || !path[0]) return false;
	cvar_t* basedir = findCvar(const_cast<char*>("basedir"));
	std::string root =
	    (basedir && basedir->string && basedir->string[0]) ? basedir->string : ".";
	while (root.size() > 1 && (root.back() == '/' || root.back() == '\\')) root.pop_back();

	const std::string rel = normalized_menu_rel(path);
	std::string rel_menus = rel;
	if (rel_menus.compare(0, 6, "menus/") == 0) rel_menus.erase(0, 6);

	static const char* const kRoots[] = {
	    "User/menus/", "base/menus/", "User/", "base/", "",
	};
	for (const char* prefix : kRoots) {
		const std::string base = root + "/" + prefix;
		if (file_exists(base + rel_menus) || file_exists(base + rel)) return true;
	}
	return false;
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

	const bool force_wide = scaled_menu_consume_menu_wide_request();
	const bool user_menu = menu_file_is_user_file(menu_file);
	const bool internal_menu = contains_ci(menu_file, "sof_buddy/");
	// Default `menu`: loose user menus keep their authored 640x480 frame geometry.
	// Stock menus keep the 4:3->wide correction. `menu_wide` forces the correction,
	// but internal sof_buddy pages always keep the stock frame canvas.
	const bool skip_vertical_shrink = internal_menu || (!force_wide && user_menu);
	scaled_menu_set_pushed_menu(name, skip_vertical_shrink);
}

#endif // UI_MENU

#endif // FEATURE_SCALED_MENU
