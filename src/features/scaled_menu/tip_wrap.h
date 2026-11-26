#pragma once

#include <cstddef>
#include <cstring>

namespace scaled_menu_detail {

using TipMeasureFn = int (*)(const char* text, void* user);

// Greedy word wrap for menu tooltips. Lines are stored as NUL-separated strings in
// `out`; `line_starts[i]` receives each line's byte offset. A single word wider than
// `max_width` is hard-split by character so it can never run off the screen.
// Returns the line count, or 0 when the text cannot be represented in the buffers.
inline int wrap_tip_text(const char* text, int max_width, TipMeasureFn measure,
                         void* user, char* out, int out_size, int* line_starts,
                         int max_lines) {
	if (!text || !measure || !out || !line_starts || out_size <= 0 || max_lines <= 0)
		return 0;
	const std::size_t len = std::strlen(text);
	if (len == 0 || len >= 1000) return 0;

	// Tokenize a private copy; the output keeps single spaces between words.
	char copy[1024];
	std::memcpy(copy, text, len + 1);
	char* words[128];
	int word_count = 0;
	char* p = copy;
	while (*p) {
		while (*p == ' ' || *p == '\t') ++p;
		if (!*p) break;
		if (word_count >= static_cast<int>(sizeof(words) / sizeof(words[0]))) return 0;
		words[word_count++] = p;
		while (*p && *p != ' ' && *p != '\t') ++p;
		if (*p) *p++ = '\0';
	}
	if (word_count == 0) return 0;

	int line_count = 0;
	int used = 0;
	char line[512];
	int line_len = 0;

	const auto flush_line = [&]() -> bool {
		if (line_len == 0) return true;
		if (line_count >= max_lines) return false;
		if (used + line_len + 1 > out_size) return false;
		line_starts[line_count++] = used;
		std::memcpy(out + used, line, static_cast<std::size_t>(line_len));
		out[used + line_len] = '\0';
		used += line_len + 1;
		line_len = 0;
		return true;
	};

	for (int i = 0; i < word_count; ++i) {
		const char* word = words[i];
		const int word_len = static_cast<int>(std::strlen(word));
		const int base = line_len;
		if (base + (base ? 1 : 0) + word_len >= static_cast<int>(sizeof(line)))
			return 0;
		if (base) line[line_len++] = ' ';
		std::memcpy(line + line_len, word, static_cast<std::size_t>(word_len));
		line_len += word_len;
		line[line_len] = '\0';
		if (measure(line, user) <= max_width) continue;

		// The word pushed the line over; drop it, flush what fit, then retry.
		line_len = base;
		line[line_len] = '\0';
		if (line_len > 0 && !flush_line()) return 0;

		std::memcpy(line, word, static_cast<std::size_t>(word_len));
		line_len = word_len;
		line[line_len] = '\0';
		if (measure(line, user) <= max_width) continue;

		// The word alone is wider than the allowed width: split it mid-word.
		line_len = 0;
		line[line_len] = '\0';
		for (int k = 0; k < word_len; ++k) {
			line[line_len++] = word[k];
			line[line_len] = '\0';
			if (line_len > 1 && measure(line, user) > max_width) {
				line_len--;
				line[line_len] = '\0';
				if (!flush_line()) return 0;
				line[0] = word[k];
				line_len = 1;
				line[line_len] = '\0';
			}
		}
	}
	if (!flush_line()) return 0;
	return line_count;
}

} // namespace scaled_menu_detail
