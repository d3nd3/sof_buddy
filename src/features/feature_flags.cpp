#include "feature_flags.h"
#include <stddef.h>

#include "util.h"

/*
    Central feature flag documentation

    How to toggle:
      - Base features are defined in hdr/features.h (existing file).
      - Granular flags below default to ON here. To turn any OFF, add
            #define <FLAG>_OFF
        in hdr/features.h (or any header included before feature_flags.h).

    Flags and what they gate (with key detours/edits):

    FEATURE_MEDIA_TIMERS
      - QPC timers + custom WinMain pacing and SoFPlus timer integration.
      - Detours/edits (see media_timers.cpp):
          * DetourCreate(0x20055930 -> my_Sys_Milliseconds)
          * WriteE8Call(0x20066412 -> winmain_loop);
            WriteE9Jmp(0x20066417 -> 0x2006643C)
          * WriteE8Call(0x20065D5E -> my_Sys_Milliseconds); WriteByte(0x20065D63, 0x90)

    FEATURE_CONSOLE_PASTE_FIX
      - Safely handles large clipboard pastes in console.
      - Edits (see sof_buddy.cpp and scaled_font.cpp notes):
          * WriteE8Call(0x2004BB63 -> my_Sys_GetClipboardData)
          * WriteE9Jmp(0x2004BB6C -> 0x2004BBFE)
          * (alt path guarded by FEATURE_CONSOLE_PASTE_FIX) jumps around Con_DrawInput region

      - Ensures cl_maxfps limiter and cinematic freeze interplay doesn't black-screen after videos.
      - Detours/edits (see sof_buddy.cpp and ref_fixes.cpp notes):
          * DetourCreate(0x50075190 -> my_CinematicFreeze)
          * WriteByte(0x2000D973, 0x90); WriteByte(0x2000D974, 0x90)

    FEATURE_FONT_SCALING
      - Scales console and selected HUD elements; glVertex2f scaling and layout adjustments.
      - Detours/edits (see scaled_font.cpp):
          * Early NOPs in Con_DrawConsole dirty rect calls
          * DetourCreate(Con_Init -> my_Con_Init)
          * DetourCreate(Con_DrawNotify/Con_DrawConsole)
          * DetourCreate(glVertex2f, DrawStretchPic, DrawPicOptions,
                        DrawCroppedPicOptions, R_DrawFont, Draw_String_Color)
          * WriteE8Call at CroppedPic four vertex call sites

    FEATURE_HD_TEX
      - Adjusts m32 default sizes for HD textures.
      - Detours (ref_fixes.cpp): DetourCreate(GL_BuildPolygonFromSurface)

    FEATURE_TEAMICON_FIX
      - Corrects team icon placement/FOV; spectator/CTF cases.
      - Detours (ref_fixes.cpp): DetourCreate(drawTeamIcons) + FOV pointer writes

    FEATURE_ALT_LIGHTING
      - Optional lighting blend mode with cvar control; can enable WhiteMagicRaven profile.
      - Detours (ref_fixes.cpp): DetourCreate(R_BlendLightmaps) and redirected glBlendFunc

    FEATURE_SOFPLUS_INTEGRATION
      - Detects spcl.dll (CRC), loads and wires winsock exports, integrates timers
      - See wsock_entry.cpp: sofplus_copy(), SoFplusLoadFn, checksum guards

    Notes
      - ref_gl early/late hooks (VID_LoadRefresh, VID_CheckChanges, RefInMemory) are internal plumbing automatically enabled when any
        renderer-affecting features are compiled (e.g. FEATURE_HD_TEX, FEATURE_TEAMICON_FIX, FEATURE_ALT_LIGHTING, FEATURE_FONT_SCALING).
*/

// Simple helper to emit ON/OFF for a macro
#define FEATURE_STATUS(name) (PrintOut(PRINT_LOG, "  %-28s : %s\n", name, "ON"))

void features_print_summary(void)
{
    PrintOut(PRINT_GOOD, "SoF Buddy feature toggles (build-time)\n");

#ifdef FEATURE_MEDIA_TIMERS
    FEATURE_STATUS("FEATURE_MEDIA_TIMERS");
#else
    PrintOut(PRINT_LOG, "  %-28s : %s\n", "FEATURE_MEDIA_TIMERS", "OFF");
#endif

#ifdef FEATURE_HD_TEX
    FEATURE_STATUS("FEATURE_HD_TEX");
#else
    PrintOut(PRINT_LOG, "  %-28s : %s\n", "FEATURE_HD_TEX", "OFF");
#endif

#ifdef FEATURE_TEAMICON_FIX
    FEATURE_STATUS("FEATURE_TEAMICON_FIX");
#else
    PrintOut(PRINT_LOG, "  %-28s : %s\n", "FEATURE_TEAMICON_FIX", "OFF");
#endif

#ifdef FEATURE_ALT_LIGHTING
    FEATURE_STATUS("FEATURE_ALT_LIGHTING");
#else
    PrintOut(PRINT_LOG, "  %-28s : %s\n", "FEATURE_ALT_LIGHTING", "OFF");
#endif

#ifdef FEATURE_FONT_SCALING
    FEATURE_STATUS("FEATURE_FONT_SCALING");
#else
    PrintOut(PRINT_LOG, "  %-28s : %s\n", "FEATURE_FONT_SCALING", "OFF");
#endif

    // Console paste fix is always on
    FEATURE_STATUS("FEATURE_CONSOLE_PASTE_FIX");

    // Cinematic freeze fix is always on
    FEATURE_STATUS("FEATURE_CINEMATICFREEZE_FIX");

    // Internals: FS/Cbuf/Ref hooks are not user features; hidden from summary
}


