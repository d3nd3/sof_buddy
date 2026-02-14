#include "feature_config.h"

#if FEATURE_SCALED_HUD || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE

#include "../shared.h"
#include <string.h>

static int append_char(char* dst, int dst_size, int& out_i, char c)
{
    if (out_i >= dst_size - 1) {
        return 0;
    }
    dst[out_i++] = c;
    return 1;
}

static int count_centerprint_lines(const char* text)
{
    if (!text || !*text) {
        return 1;
    }

    int lines = 1;
    for (const char* p = text; *p; ++p) {
        if (*p == '\n') {
            ++lines;
        }
    }
    return lines;
}

static bool any_line_exceeds_limit(const char* text, int max_chars_per_line)
{
    if (!text || max_chars_per_line < 1) {
        return false;
    }

    int line_len = 0;
    for (const char* p = text; *p; ++p) {
        if (*p == '\n') {
            line_len = 0;
            continue;
        }
        ++line_len;
        if (line_len > max_chars_per_line) {
            return true;
        }
    }
    return false;
}

static void wrap_centerprint_vanilla_like(const char* src, int max_chars_per_line, char* dst, int dst_size)
{
    int out_i = 0;
    int line_len = 0;

    const char* p = src;
    while (*p && out_i < dst_size - 1) {
        if (*p == '\n') {
            if (!append_char(dst, dst_size, out_i, '\n')) {
                break;
            }
            ++p;
            line_len = 0;
            continue;
        }

        // Capture whitespace between words (spaces/tabs only; newlines handled above).
        const char* ws_start = p;
        int ws_len = 0;
        while (*p == ' ' || *p == '\t') {
            ++p;
            ++ws_len;
        }

        if (!*p) {
            break;
        }
        if (*p == '\n') {
            continue;
        }

        // Capture the next word.
        const char* word_start = p;
        int word_len = 0;
        while (*p && *p != ' ' && *p != '\t' && *p != '\n') {
            ++p;
            ++word_len;
        }
        if (word_len <= 0) {
            continue;
        }

        // If previous content already overflowed the policy, wrap now.
        // This keeps the offending word on the previous line and wraps before the next one.
        if (line_len > max_chars_per_line) {
            if (!append_char(dst, dst_size, out_i, '\n')) {
                break;
            }
            line_len = 0;
            ws_len = 0;
        }

        // Preserve inter-word spacing when staying on the same line.
        if (line_len > 0) {
            for (int i = 0; i < ws_len; ++i) {
                if (!append_char(dst, dst_size, out_i, ws_start[i])) {
                    break;
                }
                ++line_len;
            }
        }

        // Write full word (never split mid-word).
        for (int i = 0; i < word_len && out_i < dst_size - 1; ++i) {
            if (!append_char(dst, dst_size, out_i, word_start[i])) {
                break;
            }
            ++line_len;
        }
    }

    dst[out_i] = '\0';
}

static void track_centerprint_text(const char* text)
{
    if (!text) {
        g_lastCenterPrintText[0] = '\0';
        g_lastCenterPrintLineCount = 1;
        ++g_lastCenterPrintSeq;
        return;
    }

    strncpy(g_lastCenterPrintText, text, sizeof(g_lastCenterPrintText) - 1);
    g_lastCenterPrintText[sizeof(g_lastCenterPrintText) - 1] = '\0';
    g_lastCenterPrintLineCount = count_centerprint_lines(g_lastCenterPrintText);
    ++g_lastCenterPrintSeq;
}

void hkSCR_CenterPrint(char * text, detour_SCR_CenterPrint::tSCR_CenterPrint original)
{
    SOFBUDDY_ASSERT(original != nullptr);

    if (!text) {
        track_centerprint_text(nullptr);
        original(text);
        return;
    }

    float scale = 1.0f;
    if (_sofbuddy_font_scale) {
        scale = _sofbuddy_font_scale->value;
    }
    if (scale <= 1.0f) {
        track_centerprint_text(text);
        original(text);
        return;
    }

    int vid_w = current_vid_w;
    if (vid_w <= 0) {
        vid_w = 640;
    }

    // Keep a fixed horizontal screen-space budget (about 50% of screen width):
    // chars * glyphWidth(8) * scale <= 0.5 * vid_w
    // => chars <= vid_w / (16 * scale)
    int max_chars_per_line = static_cast<int>(static_cast<float>(vid_w) / (16.0f * scale));
    if (max_chars_per_line < 1) {
        max_chars_per_line = 1;
    }

    // If text already fits current resolution/scale bounds, don't rewrite.
    if (!any_line_exceeds_limit(text, max_chars_per_line)) {
        track_centerprint_text(text);
        original(text);
        return;
    }

    // Use persistent storage in case SCR_CenterPrint keeps a pointer to input text.
    // This also avoids stack lifetime issues with delayed centerprint drawing.
    static char wrapped[1024];
    wrap_centerprint_vanilla_like(text, max_chars_per_line, wrapped, static_cast<int>(sizeof(wrapped)));
    track_centerprint_text(wrapped);
    original(wrapped);
}

#endif // FEATURE_SCALED_HUD || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE
