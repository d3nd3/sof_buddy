#include "feature_config.h"

#if FEATURE_RAW_MOUSE

#include "shared.h"
#include "util.h"
#include <limits.h>

int raw_mouse_delta_x = 0;
int raw_mouse_delta_y = 0;
POINT window_center = {0, 0};
std::vector<BYTE> g_heapBuffer;
bool raw_mouse_center_valid = false;
bool raw_mouse_registered = false;
HWND raw_mouse_hwnd_target = nullptr;

bool raw_mouse_is_enabled()
{
    return in_mouse_raw && in_mouse_raw->value != 0.0f;
}

bool raw_mouse_api_supported()
{
#if SOFBUDDY_RAWINPUT_API_AVAILABLE
    return true;
#else
    return false;
#endif
}

void raw_mouse_reset_deltas()
{
    raw_mouse_delta_x = 0;
    raw_mouse_delta_y = 0;
}

void raw_mouse_consume_deltas()
{
    raw_mouse_reset_deltas();
}

void raw_mouse_update_center(int x, int y)
{
    window_center.x = x;
    window_center.y = y;
    raw_mouse_center_valid = true;
}

void raw_mouse_accumulate_delta(LONG dx, LONG dy)
{
    long long next_x = static_cast<long long>(raw_mouse_delta_x) + static_cast<long long>(dx);
    long long next_y = static_cast<long long>(raw_mouse_delta_y) + static_cast<long long>(dy);

    if (next_x > INT_MAX) next_x = INT_MAX;
    if (next_x < INT_MIN) next_x = INT_MIN;
    if (next_y > INT_MAX) next_y = INT_MAX;
    if (next_y < INT_MIN) next_y = INT_MIN;

    raw_mouse_delta_x = static_cast<int>(next_x);
    raw_mouse_delta_y = static_cast<int>(next_y);
}

bool raw_mouse_register_input(HWND hwnd, bool log_result)
{
#if !SOFBUDDY_RAWINPUT_API_AVAILABLE
    if (log_result) {
        PrintOut(PRINT_BAD, "raw_mouse: Raw Input API is unavailable for this build target\n");
    }
    raw_mouse_registered = false;
    raw_mouse_hwnd_target = nullptr;
    return false;
#else
    if (!hwnd) {
        if (log_result) {
            PrintOut(PRINT_BAD, "raw_mouse: Could not get window handle\n");
        }
        raw_mouse_registered = false;
        raw_mouse_hwnd_target = nullptr;
        return false;
    }

    RAWINPUTDEVICE rid;
    rid.usUsagePage = 0x01;
    rid.usUsage = 0x02;
    rid.dwFlags = 0;
    rid.hwndTarget = hwnd;

    if (!RegisterRawInputDevices(&rid, 1, sizeof(RAWINPUTDEVICE))) {
        if (log_result) {
            DWORD error = GetLastError();
            PrintOut(PRINT_BAD, "raw_mouse: Failed to register raw input (error %d)\n", error);
            PrintOut(PRINT_BAD, "raw_mouse: On X11/Wine, raw input may not be supported\n");
        }
        raw_mouse_registered = false;
        raw_mouse_hwnd_target = nullptr;
        return false;
    }

    if (log_result) {
        PrintOut(PRINT_GOOD, "raw_mouse: Successfully registered raw input device\n");
    }

    raw_mouse_registered = true;
    raw_mouse_hwnd_target = hwnd;
    return true;
#endif
}

void raw_mouse_unregister_input(bool log_result)
{
#if !SOFBUDDY_RAWINPUT_API_AVAILABLE
    (void)log_result;
    raw_mouse_registered = false;
    raw_mouse_hwnd_target = nullptr;
    return;
#else
    if (!raw_mouse_registered) {
        raw_mouse_hwnd_target = nullptr;
        return;
    }

    RAWINPUTDEVICE rid;
    rid.usUsagePage = 0x01;
    rid.usUsage = 0x02;
    rid.dwFlags = RIDEV_REMOVE;
    rid.hwndTarget = nullptr;

    if (!RegisterRawInputDevices(&rid, 1, sizeof(RAWINPUTDEVICE))) {
        if (log_result) {
            DWORD error = GetLastError();
            PrintOut(PRINT_BAD, "raw_mouse: Failed to unregister raw input (error %d)\n", error);
        }
    }

    raw_mouse_registered = false;
    raw_mouse_hwnd_target = nullptr;
#endif
}

#endif // FEATURE_RAW_MOUSE
