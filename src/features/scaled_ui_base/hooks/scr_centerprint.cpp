#include "feature_config.h"

#if FEATURE_SCALED_HUD || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE

#include "../shared.h"
#include "generated_detours.h"
#include "sof_compat.h"
#include <string.h>

// centerprint_linecount @ SoF.exe+0x230310 (set by SCR_CenterPrint word-wrap pass)
static const void* kCenterPrintLineCountRva = (const void*)0x00230310;

static int append_char(char* dst, int dst_size, int& out_i, char c)
{
    if (out_i >= dst_size - 1) return 0;
    dst[out_i++] = c;
    return 1;
}

static int count_centerprint_lines(const char* text)
{
    if (!text || !*text) return 1;
    int lines = 1;
    for (const char* p = text; *p; ++p)
        if (*p == '\n') ++lines;
    return lines;
}

static bool any_line_exceeds_limit(const char* text, int max_chars_per_line)
{
    if (!text || max_chars_per_line < 1) return false;

    int line_len = 0;
    for (const char* p = text; *p; ++p) {
        if (*p == '\n') {
            line_len = 0;
            continue;
        }
        ++line_len;
        if (line_len > max_chars_per_line) return true;
    }
    return false;
}

// Recreate vanilla SCR_CenterPrint wrapping: word boundaries, explicit \n between lines.
static void wrap_centerprint_vanilla_like(const char* src, int max_chars_per_line, char* dst, int dst_size)
{
    int out_i = 0;
    int line_len = 0;

    const char* p = src;
    while (*p && out_i < dst_size - 1) {
        if (*p == '\n') {
            if (!append_char(dst, dst_size, out_i, '\n')) break;
            ++p;
            line_len = 0;
            continue;
        }

        const char* ws_start = p;
        int ws_len = 0;
        while (*p == ' ' || *p == '\t') {
            ++p;
            ++ws_len;
        }

        if (!*p) break;
        if (*p == '\n') continue;

        const char* word_start = p;
        int word_len = 0;
        while (*p && *p != ' ' && *p != '\t' && *p != '\n') {
            ++p;
            ++word_len;
        }
        if (word_len <= 0) continue;

        if (line_len > max_chars_per_line) {
            if (!append_char(dst, dst_size, out_i, '\n')) break;
            line_len = 0;
            ws_len = 0;
        }

        if (line_len > 0) {
            for (int i = 0; i < ws_len; ++i) {
                if (!append_char(dst, dst_size, out_i, ws_start[i])) break;
                ++line_len;
            }
        }

        for (int i = 0; i < word_len && out_i < dst_size - 1; ++i) {
            if (!append_char(dst, dst_size, out_i, word_start[i])) break;
            ++line_len;
        }
    }

    dst[out_i] = '\0';
}

void hkSCR_CenterPrint(char * text, detour_SCR_CenterPrint::tSCR_CenterPrint original)
{
    SOFBUDDY_ASSERT(original != nullptr);

    g_centerPrintScaleSeq = 0;
    g_centerPrintLineStep = 0.0f;

    if (!text) {
        g_lastCenterPrintText[0] = '\0';
        g_lastCenterPrintLineCount = 1;
        ++g_lastCenterPrintSeq;
        original(text);
        return;
    }

    // Use resolved fontScale (includes auto / -1 mode), not raw cvar value.
    float scale = fontScale;
    if (scale <= 0.0f) scale = 1.0f;

    const char* passText = text;
    static char wrapped[1024];
    bool preWrapped = false;

    if (scale > 1.0f) {
        int vid_w = current_vid_w;
        if (vid_w <= 0) vid_w = 640;

        // ~50% screen width budget: chars * 8px * scale <= 0.5 * vid_w
        int max_chars_per_line = static_cast<int>(static_cast<float>(vid_w) / (16.0f * scale));
        if (max_chars_per_line < 1) max_chars_per_line = 1;

        if (any_line_exceeds_limit(text, max_chars_per_line)) {
            wrap_centerprint_vanilla_like(text, max_chars_per_line, wrapped, static_cast<int>(sizeof(wrapped)));
            passText = wrapped;
            preWrapped = true;
        }
    }

    strncpy(g_lastCenterPrintText, passText, sizeof(g_lastCenterPrintText) - 1);
    g_lastCenterPrintText[sizeof(g_lastCenterPrintText) - 1] = '\0';

    original(const_cast<char*>(passText));

    if (preWrapped)
        g_lastCenterPrintLineCount = count_centerprint_lines(passText);
    else {
        int lines = 1;
        const int* pLines = (const int*)rvaToAbsExe((void*)kCenterPrintLineCountRva);
        if (pLines && *pLines > 0) lines = *pLines;
        g_lastCenterPrintLineCount = lines;
    }
    ++g_lastCenterPrintSeq;
}

#endif // FEATURE_SCALED_HUD || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE
