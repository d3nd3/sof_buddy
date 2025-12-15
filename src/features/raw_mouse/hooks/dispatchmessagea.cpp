#include "feature_config.h"

#if FEATURE_RAW_MOUSE

#include "sof_compat.h"
#include "util.h"
#include "generated_detours.h"
#include "../shared.h"

using detour_DispatchMessageA::tDispatchMessageA;

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

LRESULT dispatchmessagea_override_callback(const MSG* msg, detour_DispatchMessageA::tDispatchMessageA original) {
    if (msg && in_mouse_raw && in_mouse_raw->value != 0) {
        if (msg->message == WM_INPUT) {
            ProcessRawInput(msg->lParam);
        }
    }
    
    // Call original function
    return original ? original(msg) : 0;
}

#endif // FEATURE_RAW_MOUSE

