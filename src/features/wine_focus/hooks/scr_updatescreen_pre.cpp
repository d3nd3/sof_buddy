#include "feature_config.h"

#if FEATURE_WINE_FOCUS

#include "sof_compat.h"
#include "util.h"
#include <windows.h>

// MainWndProc WM_ACTIVATE (SoF.exe 0x200666A7) sets ActiveApp only when the
// window is activating and HIWORD(wParam) is clear. Wine 10's first alt-tab
// return is active + minimized. GLimp_AppActivate then ShowWindow(SW_RESTORE),
// which nests another WM_ACTIVATE that takes the inactive path and clears
// ActiveApp. Scr_UpdateScreen Sleep()s, so the client stays the gray class brush.

static const void* kRvaActiveApp = (void*)0x40351C;
static const void* kRvaMinimized = (void*)0x403514;
static const void* kRvaClHwnd = (void*)0x403564;
static const void* kRvaInActivate = (void*)0x4A970;
static const void* kRvaSndActivate = (void*)0x1EB0;
static const void* kRvaModuleActivate = (void*)0x390D6C;
static const void* kRvaModuleHandle = (void*)0x390DA0;
static const void* kRvaGlimpAppActivate = (void*)0x403634;
static const void* kRvaReflibActive = (void*)0x403664;

static int* Active() { return (int*)rvaToAbsExe((void*)kRvaActiveApp); }
static unsigned char* Minimized() { return (unsigned char*)rvaToAbsExe((void*)kRvaMinimized); }

static bool UnderWine() {
    static int cached = -1;
    if (cached < 0)
        cached = is_running_under_wine() ? 1 : 0;
    return cached == 1;
}

// A real minimize sits at (-32000, -32000). A gray restored window does not.
static bool OnScreen(HWND hwnd) {
    RECT r;
    if (!hwnd || !GetWindowRect(hwnd, &r))
        return false;
    return r.left > -32000 && r.top > -32000 &&
           (r.right - r.left) > 32 && (r.bottom - r.top) > 32;
}

static bool Foreground(HWND hwnd) {
    HWND fg = GetForegroundWindow();
    return fg && hwnd && GetAncestor(fg, GA_ROOT) == GetAncestor(hwnd, GA_ROOT);
}

static void MarkActive() {
    if (int* active = Active())
        *active = 1;
    if (unsigned char* minimized = Minimized())
        *minimized = 0;
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

// GLimp_AppActivate prologue in stock ref_gl: hDC is the dword before hWnd,
// hGLRC the dword after. Wine often unbinds the context on minimize, so the
// public wglGetCurrent* calls come back null and SoF never makes it current again.
static bool RefGlContext(HWND hwnd, HDC* outDc, HGLRC* outRc) {
    static uintptr_t slot = 0;
    if (!slot || *(HWND*)slot != hwnd) {
        slot = 0;
        HMODULE ref = GetModuleHandleA("ref_gl.dll");
        if (ref) {
            auto* p = (unsigned char*)ref;
            const unsigned char sig[] = {0x8B, 0x44, 0x24, 0x04, 0x85, 0xC0, 0x74};
            for (int i = 0; i < 0x2C000 - 16; i++) {
                if (memcmp(p + i, sig, sizeof(sig)) || p[i + 8] != 0xA1)
                    continue;
                uintptr_t cand = *(uintptr_t*)(p + i + 9);
                if (cand > 0x10000 && *(HWND*)cand == hwnd) {
                    slot = cand;
                    break;
                }
            }
        }
    }
    if (!slot || *(HWND*)slot != hwnd)
        return false;
    HDC dc = *(HDC*)(slot - 4);
    HGLRC rc = *(HGLRC*)(slot + 4);
    if (!dc || !rc)
        return false;
    *outDc = dc;
    *outRc = rc;
    return true;
}

// Wine drops the GL drawable across minimize. Rebinding recreates it.
static void RebindGl(HWND hwnd) {
    HMODULE gl = GetModuleHandleA("opengl32.dll");
    if (!gl)
        return;
    using MakeFn = BOOL (WINAPI*)(HDC, HGLRC);
    using RcFn = HGLRC (WINAPI*)();
    using DcFn = HDC (WINAPI*)();
    using VpFn = void (APIENTRY*)(int, int, int, int);
    auto make = (MakeFn)GetProcAddress(gl, "wglMakeCurrent");
    auto getRc = (RcFn)GetProcAddress(gl, "wglGetCurrentContext");
    auto getDc = (DcFn)GetProcAddress(gl, "wglGetCurrentDC");
    auto viewport = (VpFn)GetProcAddress(gl, "glViewport");
    if (!make)
        return;
    HDC dc = getDc ? getDc() : nullptr;
    HGLRC rc = getRc ? getRc() : nullptr;
    if (!dc || !rc)
        RefGlContext(hwnd, &dc, &rc);
    if (!dc || !rc || !make(dc, rc))
        return;
    RECT cr;
    if (viewport && GetClientRect(hwnd, &cr) && cr.right > 1 && cr.bottom > 1)
        viewport(0, 0, cr.right, cr.bottom);
}

static WNDPROC g_prev = nullptr;
static HWND g_hooked = nullptr;
static int g_activate_depth = 0;

static LRESULT CALLBACK WineWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg != WM_ACTIVATE)
        return CallWindowProcA(g_prev, hwnd, msg, wp, lp);
    // Nested activate from ShowWindow(SW_RESTORE) would clear ActiveApp.
    if (g_activate_depth)
        return 0;
    if (LOWORD(wp) == WA_INACTIVE)
        return CallWindowProcA(g_prev, hwnd, msg, wp, lp);

    g_activate_depth++;
    LRESULT r = CallWindowProcA(g_prev, hwnd, msg, MAKELONG(LOWORD(wp), 0), lp);
    g_activate_depth--;
    MarkActive();
    RebindGl(hwnd);
    return r;
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

void wine_focus_scr_updatescreen_pre(bool& force) {
    (void)force;
    if (!UnderWine())
        return;
    int* active = Active();
    unsigned char* minimized = Minimized();
    HWND* game = (HWND*)rvaToAbsExe((void*)kRvaClHwnd);
    if (!active || !minimized || !game || !*game)
        return;
    Install(*game);
    if (*active && !*minimized)
        return;
    // Inactive in the engine, but the window is the one on screen.
    if (!Foreground(*game) && !OnScreen(*game))
        return;
    g_activate_depth++;
    EngineActivate();
    g_activate_depth--;
    MarkActive();
    RebindGl(*game);
}

#endif
