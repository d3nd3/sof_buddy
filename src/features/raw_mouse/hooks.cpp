#include "feature_config.h"

#if FEATURE_RAW_MOUSE

#include "feature_macro.h"
#include "shared_hook_manager.h"
#include "util.h"
#include <windows.h>

#define HID_USAGE_PAGE_GENERIC   0x01
#define HID_USAGE_GENERIC_MOUSE   0x02

extern cvar_t * in_mouse_raw;
extern void create_raw_mouse_cvars(void);
extern void raw_mouse_on_change(cvar_t *cvar);

static void raw_mouse_PostCvarInit();
static void raw_mouse_RefDllLoaded();


REGISTER_SHARED_HOOK_CALLBACK(RefDllLoaded, raw_mouse, raw_mouse_RefDllLoaded, 70, Post);
REGISTER_HOOK(IN_MouseMove, 0x2004A1F0, void, __cdecl, void* cmd);
REGISTER_HOOK(IN_MenuMouse, 0x2004A420, void, __cdecl, void* cvar1, void* cvar2);
REGISTER_MODULE_HOOK(GetCursorPos, "user32.dll", "GetCursorPos", BOOL, __stdcall, LPPOINT lpPoint);
REGISTER_MODULE_HOOK(SetCursorPos, "user32.dll", "SetCursorPos", BOOL, __stdcall, int X, int Y);
REGISTER_MODULE_HOOK(DispatchMessage, "user32.dll", "DispatchMessageA", LRESULT, __stdcall, const MSG* msg);

static int raw_mouse_delta_x = 0;
static int raw_mouse_delta_y = 0;
static POINT window_center = {0, 0};
static bool center_initialized = false;



/*
  We are happy to reinit this upon video library reload, most likely because SDL.
*/
static void raw_mouse_RefDllLoaded()
{
    create_raw_mouse_cvars();
}

void hkIN_MouseMove(void* cmd)
{
    oIN_MouseMove(cmd);
    
    if (in_mouse_raw && in_mouse_raw->value != 0) {
        raw_mouse_delta_x = 0;
        raw_mouse_delta_y = 0;
    }
}

void hkIN_MenuMouse(void* cvar1, void* cvar2)
{
    oIN_MenuMouse(cvar1, cvar2);
    
    if (in_mouse_raw && in_mouse_raw->value != 0) {
        raw_mouse_delta_x = 0;
        raw_mouse_delta_y = 0;
    }
}

BOOL __stdcall hkGetCursorPos(LPPOINT lpPoint)
{
    if (!in_mouse_raw || in_mouse_raw->value == 0) {
        return oGetCursorPos(lpPoint);
    }
    
    if (!center_initialized) {
        oGetCursorPos(&window_center);
        center_initialized = true;
    }
    
    if (lpPoint) {
        lpPoint->x = window_center.x + raw_mouse_delta_x;
        lpPoint->y = window_center.y + raw_mouse_delta_y;
    }
    
    return TRUE;
}

BOOL __stdcall hkSetCursorPos(int X, int Y)
{
    if (!in_mouse_raw || in_mouse_raw->value == 0) {
        return oSetCursorPos(X, Y);
    }
    
    window_center.x = X;
    window_center.y = Y;
    center_initialized = true;
    
    return TRUE;
}

static void ProcessRawInput(LPARAM lParam)
{
    UINT dwSize = 0;
    GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));
    
    if (dwSize == 0 || dwSize > 1024)
        return;

    BYTE buffer[1024];
    if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, buffer, &dwSize, sizeof(RAWINPUTHEADER)) != dwSize)
        return;

    RAWINPUT* raw = (RAWINPUT*)buffer;
    if (raw->header.dwType == RIM_TYPEMOUSE && raw->data.mouse.usFlags == MOUSE_MOVE_RELATIVE) {
        raw_mouse_delta_x += raw->data.mouse.lLastX;
        raw_mouse_delta_y += raw->data.mouse.lLastY;
    }
}

LRESULT __stdcall hkDispatchMessage(const MSG* msg)
{
    if (msg && in_mouse_raw && in_mouse_raw->value != 0) {
        if (msg->message == WM_INPUT) {
            ProcessRawInput(msg->lParam);
        }
    }
    return oDispatchMessage(msg);
}

/*
  We want Legacy Messages, for mouse1 mouse2 button presses.
  The engine does not process WM_MOUSEMOVE, it uses GetCursorPos instead.
*/
void raw_mouse_on_change(cvar_t *cvar)
{
    if (!cvar) return;
    
    PrintOut(PRINT_LOG, "raw_mouse: CVar _sofbuddy_rawmouse changed to %s\n", cvar->string);
    
    if (cvar->value != 0) {
        // Raw input requested - register raw input device
        HWND hwnd = GetActiveWindow();
        if (!hwnd) hwnd = GetForegroundWindow();
        
        if (hwnd) {
            PrintOut(PRINT_LOG, "raw_mouse: Window handle: 0x%p\n", hwnd);
            
            RAWINPUTDEVICE rid;
            rid.usUsagePage = HID_USAGE_PAGE_GENERIC;
            rid.usUsage = HID_USAGE_GENERIC_MOUSE;
            // RIDEV_NOLEGACY: Don't generate legacy mouse messages
            // RIDEV_INPUTSINK: Capture even when not focused (commented out for Wine/X11 compatibility)
            rid.dwFlags = 0;
            rid.hwndTarget = hwnd;
            
            BOOL success = RegisterRawInputDevices(&rid, 1, sizeof(RAWINPUTDEVICE));
            if (success) {
                PrintOut(PRINT_GOOD, "raw_mouse: Successfully registered raw input device\n");
            } else {
                DWORD error = GetLastError();
                PrintOut(PRINT_BAD, "raw_mouse: Failed to register raw input (error %d)\n", error);
                PrintOut(PRINT_BAD, "raw_mouse: On X11/Wine, raw input may not be supported\n");
            }
        } else {
            PrintOut(PRINT_BAD, "raw_mouse: Could not get window handle\n");
        }
        
        PrintOut(PRINT_GOOD, "raw_mouse: Raw input is now ENABLED\n");
    } else {
        PrintOut(PRINT_LOG, "raw_mouse: Raw input is now DISABLED (using legacy cursor mode)\n");
    }
}

#endif

