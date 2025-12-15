#include "feature_config.h"

#if FEATURE_RAW_MOUSE

#include <stdint.h>
#include "sof_compat.h"
#include "util.h"
#include <windows.h>
#include "shared.h"

cvar_t * in_mouse_raw = NULL;

void raw_mouse_on_change(cvar_t *cvar)
{
    if (!cvar) return;
    
    if (cvar->value != 0) {
        HWND hwnd = GetActiveWindow();
        if (!hwnd) hwnd = GetForegroundWindow();
        
        if (hwnd) {
            RAWINPUTDEVICE rid;
            rid.usUsagePage = 0x01;
            rid.usUsage = 0x02;
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

void create_raw_mouse_cvars(void) {
    in_mouse_raw = orig_Cvar_Get("_sofbuddy_rawmouse", "0", CVAR_ARCHIVE, &raw_mouse_on_change);
    
    PrintOut(PRINT_LOG, "raw_mouse: Registered _sofbuddy_rawmouse cvar with change callback\n");
}

#endif

