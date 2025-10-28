#include "feature_config.h"

#if FEATURE_RAW_MOUSE

#include "feature_macro.h"
#include "shared_hook_manager.h"
#include "util.h"
#include <windows.h>
#include <vector>

#define HID_USAGE_PAGE_GENERIC   0x01
#define HID_USAGE_GENERIC_MOUSE   0x02

extern cvar_t * in_mouse_raw;
extern void create_raw_mouse_cvars(void);
extern void raw_mouse_on_change(cvar_t *cvar);

static void raw_mouse_PostCvarInit();
static void raw_mouse_RefDllLoaded();
static void raw_mouse_EarlyStartup();

REGISTER_SHARED_HOOK_CALLBACK(EarlyStartup, raw_mouse, raw_mouse_EarlyStartup, 70, Post);
REGISTER_SHARED_HOOK_CALLBACK(RefDllLoaded, raw_mouse, raw_mouse_RefDllLoaded, 70, Post);
REGISTER_HOOK(IN_MouseMove, (void*)0x0004A1F0, SofExe, void, __cdecl, void* cmd);
REGISTER_HOOK(IN_MenuMouse, (void*)0x0004A420, SofExe, void, __cdecl, void* cvar1, void* cvar2);

extern "C" {
// Module hooks are handled differently - they use GetProcAddress from loaded modules
REGISTER_MODULE_HOOK(GetCursorPos, "user32.dll", "GetCursorPos", BOOL, __stdcall, LPPOINT lpPoint);
REGISTER_MODULE_HOOK(DispatchMessage, "user32.dll", "DispatchMessageA", LRESULT, __stdcall, const MSG* msg);
}

using tSetCursorPos = BOOL(__stdcall*)(int X, int Y);
static tSetCursorPos oSetCursorPos = nullptr;
BOOL __stdcall hkSetCursorPos(int X, int Y);

static int raw_mouse_delta_x = 0;
static int raw_mouse_delta_y = 0;
static POINT window_center = {0, 0};
static std::vector<BYTE> g_heapBuffer;

static void raw_mouse_EarlyStartup()
{
    WriteE8Call(rvaToAbsExe((void*)0x0004A0B2), (void*)&hkSetCursorPos);
    WriteByte((unsigned char*)rvaToAbsExe((void*)0x0004A0B2) + 5, 0x90);
    WriteE8Call(rvaToAbsExe((void*)0x0004A410), (void*)&hkSetCursorPos);
    WriteByte((unsigned char*)rvaToAbsExe((void*)0x0004A410) + 5, 0x90);
    WriteE8Call(rvaToAbsExe((void*)0x0004A579), (void*)&hkSetCursorPos);
    WriteByte((unsigned char*)rvaToAbsExe((void*)0x0004A579) + 5, 0x90);

    if (!oSetCursorPos) {
        HMODULE hUser32 = GetModuleHandleA("user32.dll");
        if (hUser32) oSetCursorPos = (tSetCursorPos)GetProcAddress(hUser32, "SetCursorPos");
    }
}

static void raw_mouse_RefDllLoaded()
{
    create_raw_mouse_cvars();
}

/*
    mx = current_pos.x - window_center_x;
	my = current_pos.y - window_center_y;
    calculates the difference between the GetCursorPos() and the window_center
*/
void hkIN_MouseMove(void* cmd)
{
    oIN_MouseMove(cmd);
}

void hkIN_MenuMouse(void* cvar1, void* cvar2)
{
    oIN_MenuMouse(cvar1, cvar2);
}

BOOL __stdcall hkGetCursorPos(LPPOINT lpPoint)
{
    if (!in_mouse_raw || in_mouse_raw->value == 0) {
        return oGetCursorPos(lpPoint);
    }
    
    if (lpPoint) {
        lpPoint->x = window_center.x + raw_mouse_delta_x;
        lpPoint->y = window_center.y + raw_mouse_delta_y;
    }
    
    return TRUE;
}

/*
    Always called with windows_center_x and window_center_y as input params
    Can be called three times by the engine
      IN_ActivateMouse() and IN_MouseMove() and IN_MenuMouse()
    IN_ActivateMouse() does not call GetCursorPos(), the others do.
*/
BOOL __stdcall hkSetCursorPos(int X, int Y)
{
    if (!in_mouse_raw || in_mouse_raw->value == 0) {
        return oSetCursorPos(X, Y);
    }

    raw_mouse_delta_x = 0;
    raw_mouse_delta_y = 0;
    
    window_center.x = X;
    window_center.y = Y;
    
    return TRUE;
}

static void ProcessRawInput(LPARAM lParam)
{
    UINT dwSize = 0;
    GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));

    BYTE stackBuffer[256];
    if (dwSize <= sizeof(stackBuffer)) {
        GetRawInputData((HRAWINPUT)lParam, RID_INPUT, stackBuffer, &dwSize, sizeof(RAWINPUTHEADER));
        RAWINPUT* raw = (RAWINPUT*)stackBuffer;
        if (raw->header.dwType == RIM_TYPEMOUSE) {
            if ((raw->data.mouse.usFlags & MOUSE_MOVE_RELATIVE) == MOUSE_MOVE_RELATIVE) {
                if (raw->data.mouse.lLastX != 0 || raw->data.mouse.lLastY != 0) {
                    raw_mouse_delta_x += raw->data.mouse.lLastX;
                    raw_mouse_delta_y += raw->data.mouse.lLastY;
                }
            }
        }
    } else {
        g_heapBuffer.resize(dwSize);
        GetRawInputData((HRAWINPUT)lParam, RID_INPUT, g_heapBuffer.data(), &dwSize, sizeof(RAWINPUTHEADER));

        RAWINPUT* raw = (RAWINPUT*)g_heapBuffer.data();
        if (raw->header.dwType == RIM_TYPEMOUSE) {
            if ((raw->data.mouse.usFlags & MOUSE_MOVE_RELATIVE) == MOUSE_MOVE_RELATIVE) {
                if (raw->data.mouse.lLastX != 0 || raw->data.mouse.lLastY != 0) {
                    raw_mouse_delta_x += raw->data.mouse.lLastX;
                    raw_mouse_delta_y += raw->data.mouse.lLastY;
                }
            }
        }
    }
}

LRESULT __stdcall hkDispatchMessage(const MSG* msg)
{
    if (msg && in_mouse_raw && in_mouse_raw->value != 0) {
        if (msg->message == WM_INPUT) {
            ProcessRawInput(msg->lParam);
        }
    }
    //The MainWndProc will call DefWindowProcA
    return oDispatchMessage(msg);
}

void raw_mouse_on_change(cvar_t *cvar)
{
    if (!cvar) return;
    
    if (cvar->value != 0) {
        // Raw input requested - register raw input device
        HWND hwnd = GetActiveWindow();
        if (!hwnd) hwnd = GetForegroundWindow();
        
        if (hwnd) {
            RAWINPUTDEVICE rid;
            rid.usUsagePage = HID_USAGE_PAGE_GENERIC;
            rid.usUsage = HID_USAGE_GENERIC_MOUSE;
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

