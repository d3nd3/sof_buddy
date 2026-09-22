#include "feature_config.h"

#if FEATURE_WINE_FOCUS

#include "sof_compat.h"
#include "util.h"
#include <windows.h>

// MainWndProc WM_ACTIVATE (SoF.exe 0x200666A7): ActiveApp is set only when
// LOWORD(wParam) is active AND HIWORD (fMinimized) is clear. Wine's first
// alt-tab return is active + minimized, so the window is shown
// (GLimp_AppActivate) and then Scr_UpdateScreen sleeps instead of drawing.
// A later clean activate is why the second alt-tab paints again.

static const void* kRvaActiveApp = (void*)0x40351C;
static const void* kRvaMinimized = (void*)0x403514;
static const void* kRvaClHwnd = (void*)0x403564;
static const void* kRvaInActivate = (void*)0x4A970;
static const void* kRvaSndActivate = (void*)0x1EB0;
static const void* kRvaModuleActivate = (void*)0x390D6C;
static const void* kRvaModuleHandle = (void*)0x390DA0;
static const void* kRvaGlimpAppActivate = (void*)0x403634;
static const void* kRvaReflibActive = (void*)0x403664;

static bool UnderWine() {
    static int cached = -1;
    if (cached < 0)
        cached = is_running_under_wine() ? 1 : 0;
    return cached == 1;
}

static WNDPROC g_prev = nullptr;
static HWND g_hooked = nullptr;
static int g_activate_depth = 0;

static LRESULT CALLBACK WineWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    // Strip the minimized bit on an activating message so the engine takes
    // the same path as a clean alt-tab (ActiveApp, input, GLimp restore).
    if (msg == WM_ACTIVATE && g_activate_depth < 2 &&
        LOWORD(wp) != WA_INACTIVE && HIWORD(wp)) {
        g_activate_depth++;
        LRESULT r = CallWindowProcA(g_prev, hwnd, msg, MAKELONG(LOWORD(wp), 0), lp);
        g_activate_depth--;
        return r;
    }
    return CallWindowProcA(g_prev, hwnd, msg, wp, lp);
}

static void Install(HWND game) {
    if (!game)
        return;
    if (game == g_hooked &&
        (WNDPROC)GetWindowLongPtrA(game, GWLP_WNDPROC) == WineWndProc)
        return;
    WNDPROC prev = (WNDPROC)SetWindowLongPtrA(game, GWLP_WNDPROC, (LONG_PTR)WineWndProc);
    if (!prev || prev == WineWndProc)
        return;
    g_prev = prev;
    g_hooked = game;
}

static void EngineActivate() {
    using Act = void(__cdecl*)(int);
    ((Act)rvaToAbsExe((void*)kRvaInActivate))(1);
    ((Act)rvaToAbsExe((void*)kRvaSndActivate))(1);
    HMODULE* mod = (HMODULE*)rvaToAbsExe((void*)kRvaModuleHandle);
    Act* modAct = (Act*)rvaToAbsExe((void*)kRvaModuleActivate);
    if (mod && *mod && modAct && *modAct)
        (*modAct)(1);
    auto* reflib = (unsigned char*)rvaToAbsExe((void*)kRvaReflibActive);
    Act* glimp = (Act*)rvaToAbsExe((void*)kRvaGlimpAppActivate);
    if (reflib && *reflib && glimp && *glimp)
        (*glimp)(1);
}

void wine_focus_scr_updatescreen_pre(bool& force) {
    (void)force;
    if (!UnderWine())
        return;
    int* active = (int*)rvaToAbsExe((void*)kRvaActiveApp);
    unsigned char* minimized = (unsigned char*)rvaToAbsExe((void*)kRvaMinimized);
    HWND* game = (HWND*)rvaToAbsExe((void*)kRvaClHwnd);
    if (!active || !minimized || !game || !*game)
        return;
    Install(*game);
    // Already stuck: engine recorded minimized, but the window is on screen.
    if (*active || !*minimized)
        return;
    if (IsIconic(*game) || !IsWindowVisible(*game))
        return;
    *active = 1;
    *minimized = 0;
    EngineActivate();
}

#endif
