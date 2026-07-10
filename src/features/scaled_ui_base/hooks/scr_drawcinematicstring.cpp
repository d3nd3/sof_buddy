#include "feature_config.h"

#if FEATURE_SCALED_HUD || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE

#include "util.h"
#include "generated_detours.h"
#include "sof_compat.h"
#include "../shared.h"

// SCR_DrawCinematicString typematic text buffer (byte_2022FF10 @ SoF.exe+0x22FF10)
static const void* kCineTypematicBufRva = (const void*)0x0022FF10;
static const float kCineLineHeight = 8.0f;

static int countCineLines(const char* s) {
    if (!s || !*s) return 1;
    int lines = 1;
    for (; *s; ++s)
        if (*s == '\n') ++lines;
    return lines;
}

void hkSCR_DrawCinematicString(int speed, int x, int y, detour_SCR_DrawCinematicString::tSCR_DrawCinematicString original) {
    SOFBUDDY_ASSERT(original != nullptr);

    resetGlVertexQuadState();
    g_activeRenderType = uiRenderType::Cinematic;
    g_cineTextBaseY = -1.0f;
    g_cineTextBottomY = -1.0f;
    g_cineTextTargetBottomY = -1.0f;
    g_cineTextLineCount = countCineLines((const char*)rvaToAbsExe((void*)kCineTypematicBufRva));
    g_cinematicDrawDepth++;
    original(speed, x, y);
    g_cinematicDrawDepth--;
    g_activeRenderType = uiRenderType::None;
    resetGlVertexQuadState();
}

#endif // FEATURE_SCALED_HUD || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE
