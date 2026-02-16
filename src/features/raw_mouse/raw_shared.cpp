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
bool raw_mouse_cursor_clipped = false;
HWND raw_mouse_hwnd_target = nullptr;
static RECT raw_mouse_clip_rect = {0, 0, 0, 0};
static const int RAW_MOUSE_CLIP_INSET = 64;

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

static HWND RawMouseResolveTargetHwnd(HWND hwnd_hint)
{
    if (raw_mouse_hwnd_target && IsWindow(raw_mouse_hwnd_target)) {
        return raw_mouse_hwnd_target;
    }

    if (hwnd_hint && IsWindow(hwnd_hint)) {
        return hwnd_hint;
    }

    HWND hwnd = GetActiveWindow();
    if (!hwnd) {
        hwnd = GetForegroundWindow();
    }
    return hwnd;
}

static bool RawMouseHasForeground(HWND hwnd)
{
    if (!hwnd) {
        return false;
    }

    HWND foreground = GetForegroundWindow();
    if (!foreground) {
        return false;
    }

#if defined(GA_ROOT)
    HWND root_target = GetAncestor(hwnd, GA_ROOT);
    HWND root_foreground = GetAncestor(foreground, GA_ROOT);
    if (!root_target) root_target = hwnd;
    if (!root_foreground) root_foreground = foreground;
    return root_target == root_foreground;
#else
    return hwnd == foreground;
#endif
}

void raw_mouse_release_cursor_clip()
{
    if (raw_mouse_cursor_clipped) {
        ClipCursor(nullptr);
    }

    raw_mouse_cursor_clipped = false;
    SetRectEmpty(&raw_mouse_clip_rect);
}

void raw_mouse_refresh_cursor_clip(HWND hwnd_hint)
{
    if (!raw_mouse_api_supported() || !raw_mouse_is_enabled()) {
        raw_mouse_release_cursor_clip();
        return;
    }

    HWND hwnd = RawMouseResolveTargetHwnd(hwnd_hint);
    if (!hwnd || !RawMouseHasForeground(hwnd)) {
        raw_mouse_release_cursor_clip();
        return;
    }

    RECT client_rect;
    if (!GetClientRect(hwnd, &client_rect)) {
        raw_mouse_release_cursor_clip();
        return;
    }

    POINT top_left = {client_rect.left, client_rect.top};
    POINT bottom_right = {client_rect.right, client_rect.bottom};
    if (!ClientToScreen(hwnd, &top_left) || !ClientToScreen(hwnd, &bottom_right)) {
        raw_mouse_release_cursor_clip();
        return;
    }

    RECT new_clip_rect = {top_left.x, top_left.y, bottom_right.x, bottom_right.y};
    new_clip_rect.left += RAW_MOUSE_CLIP_INSET;
    new_clip_rect.right -= RAW_MOUSE_CLIP_INSET;
    new_clip_rect.top += RAW_MOUSE_CLIP_INSET;
    new_clip_rect.bottom -= RAW_MOUSE_CLIP_INSET;
    if (new_clip_rect.left >= new_clip_rect.right || new_clip_rect.top >= new_clip_rect.bottom) {
        raw_mouse_release_cursor_clip();
        return;
    }

    POINT cur;
    if (GetCursorPos(&cur) && oSetCursorPos) {
        int cx = cur.x, cy = cur.y;
        if (cx < new_clip_rect.left) cx = new_clip_rect.left;
        else if (cx >= new_clip_rect.right) cx = new_clip_rect.right - 1;
        if (cy < new_clip_rect.top) cy = new_clip_rect.top;
        else if (cy >= new_clip_rect.bottom) cy = new_clip_rect.bottom - 1;
        if (cx != cur.x || cy != cur.y)
            oSetCursorPos(cx, cy);
    }

    if (raw_mouse_cursor_clipped && EqualRect(&raw_mouse_clip_rect, &new_clip_rect)) {
        return;
    }

    if (!ClipCursor(&new_clip_rect)) {
        raw_mouse_release_cursor_clip();
        return;
    }

    raw_mouse_cursor_clipped = true;
    raw_mouse_clip_rect = new_clip_rect;
}

bool raw_mouse_register_input(HWND hwnd, bool log_result)
{
#if !SOFBUDDY_RAWINPUT_API_AVAILABLE
    if (log_result) {
        PrintOut(PRINT_BAD, "raw_mouse: Raw Input API is unavailable for this build target\n");
    }
    raw_mouse_registered = false;
    raw_mouse_hwnd_target = nullptr;
    raw_mouse_release_cursor_clip();
    return false;
#else
    if (!hwnd) {
        if (log_result) {
            PrintOut(PRINT_BAD, "raw_mouse: Could not get window handle\n");
        }
        raw_mouse_registered = false;
        raw_mouse_hwnd_target = nullptr;
        raw_mouse_release_cursor_clip();
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
        raw_mouse_release_cursor_clip();
        return false;
    }

    if (log_result) {
        PrintOut(PRINT_GOOD, "raw_mouse: Successfully registered raw input device\n");
    }

    raw_mouse_registered = true;
    raw_mouse_hwnd_target = hwnd;
    raw_mouse_refresh_cursor_clip(hwnd);
    return true;
#endif
}

void raw_mouse_unregister_input(bool log_result)
{
#if !SOFBUDDY_RAWINPUT_API_AVAILABLE
    (void)log_result;
    raw_mouse_registered = false;
    raw_mouse_hwnd_target = nullptr;
    raw_mouse_release_cursor_clip();
    return;
#else
    raw_mouse_release_cursor_clip();

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
