#include "../src/features/scaled_menu/scroll_restore.h"

#include <cassert>

int main() {
	using scaled_menu_detail::restored_scroll_offset;

	assert(restored_scroll_offset(0, 1000, 2000) == 0);
	assert(restored_scroll_offset(-4, 1000, 2000) == 0);
	assert(restored_scroll_offset(250, 1000, 2000) == 500);
	assert(restored_scroll_offset(250, 1000, 500) == 125);
	assert(restored_scroll_offset(100, 0, 2000) == 100);
	assert(restored_scroll_offset(100, 1000, 0) == 100);
	assert(restored_scroll_offset(999, 1000, 1000) == 999);
}
