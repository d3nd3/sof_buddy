// Host test for the scaled-menu tooltip word wrapper (pure logic, no engine deps).
#include "../src/features/scaled_menu/tip_wrap.h"

#include <cassert>
#include <cstdio>
#include <cstring>

namespace {

int g_glyph_width = 7;

int mono_width(const char* text, void* user) {
	(void)user;
	return static_cast<int>(std::strlen(text)) * g_glyph_width;
}

struct Result {
	char storage[1024];
	int starts[24];
	int count;
};

Result wrap(const char* text, int max_width, int glyph_width = 7) {
	g_glyph_width = glyph_width;
	Result r{};
	r.count = scaled_menu_detail::wrap_tip_text(
	    text, max_width, mono_width, nullptr, r.storage,
	    static_cast<int>(sizeof(r.storage)), r.starts, 24);
	return r;
}

const char* line(const Result& r, int i) { return r.storage + r.starts[i]; }

void expect_lines(const Result& r, const char* const* want, int count) {
	assert(r.count == count);
	for (int i = 0; i < count; ++i) assert(std::strcmp(line(r, i), want[i]) == 0);
}

} // namespace

int main() {
	// Fits within the width: the wrapper still reports one line.
	{
		Result r = wrap("short tip", 200);
		const char* want[] = {"short tip"};
		expect_lines(r, want, 1);
	}

	// Greedy wrap at word boundaries: 10 characters fit in 70 px.
	{
		Result r = wrap("aaaa bbbb cccc", 70);
		const char* want[] = {"aaaa bbbb", "cccc"};
		expect_lines(r, want, 2);
	}

	// Leading/multiple spaces collapse to single word gaps.
	{
		Result r = wrap("  aa   bb  ", 70);
		const char* want[] = {"aa bb"};
		expect_lines(r, want, 1);
	}

	// A word wider than the line is hard-split by character.
	{
		Result r = wrap("abcdefghijklmno", 70);
		const char* want[] = {"abcdefghij", "klmno"};
		expect_lines(r, want, 2);
	}

	// Hard split after a wrapped prefix leaves the remainder for the next word.
	{
		Result r = wrap("xx abcdefghijklmno yy", 70);
		const char* want[] = {"xx", "abcdefghij", "klmno yy"};
		expect_lines(r, want, 3);
	}

	// Exact fit is not split.
	{
		Result r = wrap("abcdefghij", 70);
		const char* want[] = {"abcdefghij"};
		expect_lines(r, want, 1);
	}

	// Relatively scaled text (countdown of glyph width) wraps the same way.
	{
		Result r = wrap("aaaa bbbb cccc", 140, 14);
		const char* want[] = {"aaaa bbbb", "cccc"};
		expect_lines(r, want, 2);
	}

	// Empty and oversized input fall back to the stock single-line draw.
	assert(wrap("", 100).count == 0);
	assert(wrap("   \t  ", 100).count == 0);
	{
		char big[1200];
		std::memset(big, 'a', sizeof(big));
		big[sizeof(big) - 1] = '\0';
		assert(wrap(big, 100).count == 0);
	}

	std::puts("tip wrap tests passed");
	return 0;
}
