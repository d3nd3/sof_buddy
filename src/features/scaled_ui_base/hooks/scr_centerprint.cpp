#include "feature_config.h"

#if FEATURE_SCALED_HUD || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE

#include "../shared.h"

void hkSCR_CenterPrint(char * text, detour_SCR_CenterPrint::tSCR_CenterPrint original)
{
    SOFBUDDY_ASSERT(original != nullptr);
    if (!original) {
        return;
    }

    if (!text) {
        original(text);
        return;
    }

    float scale = 1.0f;
    if (_sofbuddy_font_scale) {
        scale = _sofbuddy_font_scale->value;
    }
    if (scale <= 1.0f) {
        original(text);
        return;
    }

    const int base_width_chars = 40;
    int max_chars_per_line = static_cast<int>(base_width_chars / scale);
    if (max_chars_per_line < 1) {
        max_chars_per_line = 1;
    }

    char wrapped[1024];
    int out_i = 0;
    int line_start = 0;
    int col = 0;
    int last_space = -1;

    for (int in_i = 0; text[in_i] != '\0' && out_i < static_cast<int>(sizeof(wrapped)) - 1; ++in_i) {
        const char c = text[in_i];
        wrapped[out_i++] = c;

        if (c == '\n') {
            line_start = out_i;
            col = 0;
            last_space = -1;
            continue;
        }

        if (c == ' ') {
            last_space = out_i - 1;
        }

        ++col;
        if (col <= max_chars_per_line) {
            continue;
        }

        // Prefer wrapping at the last space in the current line to avoid splitting words.
        if (last_space >= line_start) {
            wrapped[last_space] = '\n';
            line_start = last_space + 1;
            col = out_i - line_start;
            last_space = -1;
            for (int j = out_i - 1; j >= line_start; --j) {
                if (wrapped[j] == ' ') {
                    last_space = j;
                    break;
                }
            }
            continue;
        }

        // If no space is available, hard-wrap at the current position.
        if (out_i < static_cast<int>(sizeof(wrapped)) - 1) {
            wrapped[out_i++] = '\n';
            line_start = out_i;
            col = 0;
            last_space = -1;
        }
    }

    wrapped[out_i] = '\0';
    original(wrapped);
}

#endif // FEATURE_SCALED_HUD || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE
