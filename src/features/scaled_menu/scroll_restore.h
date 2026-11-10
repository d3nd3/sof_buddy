#pragma once

#include <cmath>

namespace scaled_menu_detail {

// The engine stores the current page scroll in stm_c+0x80 and the scrollable
// content height in stm_c+0x7C. A menu reparse creates a new stm_c, so carry the
// old position across as a fraction of the content height rather than raw pixels
// (the content itself is rebuilt at the new menu scale).
inline int restored_scroll_offset(int saved_offset, int saved_total, int new_total) {
	if (saved_offset <= 0) return 0;
	if (saved_total <= 0 || new_total <= 0) return saved_offset;
	const double scaled = static_cast<double>(saved_offset) *
	                      static_cast<double>(new_total) /
	                      static_cast<double>(saved_total);
	return static_cast<int>(std::llround(scaled));
}

} // namespace scaled_menu_detail
