#include "feature_config.h"

#if FEATURE_SCALED_MENU

#ifdef UI_MENU

#include "menu_variant.h"

#include <cstring>

static bool ends_with_ci(const char* text, size_t length, const char* suffix) {
	const size_t suffix_length = std::strlen(suffix);
	if (length < suffix_length) return false;
	for (size_t i = 0; i < suffix_length; ++i) {
		char a = text[length - suffix_length + i];
		char b = suffix[i];
		if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
		if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
		if (a != b) return false;
	}
	return true;
}

static bool s_wide_variant = false;
static bool s_ss_content_page_active = false;

const char* scaled_menu_variant_redirect(const char* name, char* buffer, size_t buffer_size) {
	if (!name || !*name) return name;
	const size_t length = std::strlen(name);
	if (length + 1 > buffer_size) return name;
	std::memcpy(buffer, name, length + 1);

	size_t stem = length;
	if (stem >= 4 && ends_with_ci(buffer, stem, ".rmf")) stem -= 4;
	if (stem >= 3 && ends_with_ci(buffer, stem, "_ss")) {
		s_wide_variant = true;
		buffer[stem - 2] = 'n';
		buffer[stem - 1] = 's';
		return buffer;
	}
	if (stem >= 3 && ends_with_ci(buffer, stem, "_ns")) s_wide_variant = false;
	return name;
}

void scaled_menu_variant_reset(void) {
	s_wide_variant = false;
	s_ss_content_page_active = false;
}

bool scaled_menu_wide_variant_active(void) {
	return s_wide_variant;
}

void scaled_menu_note_ss_source(void) {
	s_wide_variant = true;
}

bool scaled_menu_ss_content_page_active(void) {
	return s_ss_content_page_active;
}

void scaled_menu_note_ss_content_page(bool active) {
	s_ss_content_page_active = active;
}

#endif // UI_MENU

#endif // FEATURE_SCALED_MENU
