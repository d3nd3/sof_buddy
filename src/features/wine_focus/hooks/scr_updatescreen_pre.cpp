#include "feature_config.h"

#if FEATURE_WINE_FOCUS

#include "sof_compat.h"
#include "util.h"
#include <windows.h>

// SoF.exe MainWndProc WM_ACTIVATE: ActiveApp is set only when the window is
// active and the minimized bit is clear. Wine 10 often delivers the first
// alt-tab return as active + minimized, so the window is restored and then
// Scr_UpdateScreen refuses to draw (gray client area). A second alt-tab sends
// a clean activate. If the game window is actually in front, treat that as focused.

static const void* kRvaActiveApp = (void*)0x40351C;
static const void* kRvaMinimized = (void*)0x403514;
static const void* kRvaClHwnd = (void*)0x403564;
static const void* kRvaInActivate = (void*)0x4A970;
static const void* kRvaSndActivate = (void*)0x1EB0;
static const void* kRvaModuleActivate = (void*)0x390D6C;
static const void* kRvaModuleHandle = (void*)0x390DA0;

static bool UnderWine() {
    static int cached = -1;
    if (cached < 0)
        cached = is_running_under_wine() ? 1 : 0;
    return cached == 1;
}

static bool GameWindowInFront(HWND game) {
    HWND fg = GetForegroundWindow();
    if (!fg || !game)
        return false;
    if (fg == game)
        return true;
    return GetAncestor(fg, GA_ROOT) == GetAncestor(game, GA_ROOT);
}

void wine_focus_scr_updatescreen_pre(bool& force) {
    (void)force;
    if (!UnderWine())
        return;
    int* active = (int*)rvaToAbsExe((void*)kRvaActiveApp);
    unsigned char* minimized = (unsigned char*)rvaToAbsExe((void*)kRvaMinimized);
    HWND* game = (HWND*)rvaToAbsExe((void*)kRvaClHwnd);
    if (!active || !minimized || !game || !*game || *active)
        return;
    if (!GameWindowInFront(*game))
        return;

    *active = 1;
    *minimized = 0;
    using Act = void(__cdecl*)(int);
    ((Act)rvaToAbsExe((void*)kRvaInActivate))(1);
    ((Act)rvaToAbsExe((void*)kRvaSndActivate))(1);
    HMODULE* mod = (HMODULE*)rvaToAbsExe((void*)kRvaModuleHandle);
    Act* modAct = (Act*)rvaToAbsExe((void*)kRvaModuleActivate);
    if (mod && *mod && modAct && *modAct)
        (*modAct)(1);
}

#endif
