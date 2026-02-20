#include "feature_config.h"

#if FEATURE_RAW_MOUSE

#include "shared.h"
#include "util.h"
#include <limits.h>
#include <vector>

int raw_mouse_delta_x = 0;
int raw_mouse_delta_y = 0;
POINT window_center = {0, 0};
bool raw_mouse_center_valid = false;
bool raw_mouse_registered = false;
bool raw_mouse_cursor_clipped = false;
HWND raw_mouse_hwnd_target = nullptr;
static RECT raw_mouse_clip_rect = {0, 0, 0, 0};
static const int RAW_MOUSE_CLIP_INSET = 64;
bool raw_processed_this_frame = false;
static std::vector<BYTE> raw_read_buf;
#ifndef ERROR_INSUFFICIENT_BUFFER
#define ERROR_INSUFFICIENT_BUFFER 122
#endif
#if !defined(_WIN64)
#define RAWMOUSE_DATA_OFFSET_WOW64 24
#endif

bool raw_mouse_is_enabled() {
  return in_mouse_raw && in_mouse_raw->value != 0.0f;
}

bool raw_mouse_api_supported() {
#if SOFBUDDY_RAWINPUT_API_AVAILABLE
  return true;
#else
  return false;
#endif
}

void raw_mouse_reset_deltas() {
  raw_mouse_delta_x = 0;
  raw_mouse_delta_y = 0;
}

void raw_mouse_consume_deltas() { raw_mouse_reset_deltas(); }

void raw_mouse_update_center(int x, int y) {
  window_center.x = x;
  window_center.y = y;
  raw_mouse_center_valid = true;
}

void raw_mouse_accumulate_delta(LONG dx, LONG dy) {
  long long next_x =
      static_cast<long long>(raw_mouse_delta_x) + static_cast<long long>(dx);
  long long next_y =
      static_cast<long long>(raw_mouse_delta_y) + static_cast<long long>(dy);

  if (next_x > INT_MAX)
    next_x = INT_MAX;
  if (next_x < INT_MIN)
    next_x = INT_MIN;
  if (next_y > INT_MAX)
    next_y = INT_MAX;
  if (next_y < INT_MIN)
    next_y = INT_MIN;

  raw_mouse_delta_x = static_cast<int>(next_x);
  raw_mouse_delta_y = static_cast<int>(next_y);
}

static bool RawMouseHasForeground(HWND hwnd);
void raw_mouse_ensure_registered(HWND hwnd_hint);

void raw_mouse_process_raw_mouse() {
#if !SOFBUDDY_RAWINPUT_API_AVAILABLE
  return;
#else
  if (!raw_mouse_is_enabled()) { return; }
  if (raw_processed_this_frame) return;
  raw_processed_this_frame = true;

  if (!raw_mouse_registered) raw_mouse_ensure_registered(nullptr);
  if (raw_mouse_hwnd_target && !RawMouseHasForeground(raw_mouse_hwnd_target)) return;

  UINT size = 0;
  if (GetRawInputBuffer(nullptr, &size, sizeof(RAWINPUTHEADER)) == (UINT)-1 || size == 0) { return; }
#if !defined(_WIN64)
  size *= 8;
#endif
  size = (size + sizeof(RAWINPUTHEADER) - 1) & ~(sizeof(RAWINPUTHEADER) - 1);
  if (raw_read_buf.size() < size)
    raw_read_buf.resize(size);

  UINT readSize = size;

  UINT count = GetRawInputBuffer(
      reinterpret_cast<PRAWINPUT>(raw_read_buf.data()),
      &readSize,
      sizeof(RAWINPUTHEADER)
  );

  for (int retries = 0; count == (UINT)-1 && retries < 4; ++retries) {
    DWORD err = GetLastError();
    if (err != ERROR_INSUFFICIENT_BUFFER) break;
    UINT required = readSize > 0 ? readSize : static_cast<UINT>(raw_read_buf.size() * 2);
#if !defined(_WIN64)
    if (readSize > 0) required = readSize * 8;
#endif
    required = (required + sizeof(RAWINPUTHEADER) - 1) & ~(sizeof(RAWINPUTHEADER) - 1);
    const UINT cap = 256 * 1024;
    if (required > cap) required = cap;
    if (required <= raw_read_buf.size()) required = static_cast<UINT>(raw_read_buf.size()) + sizeof(RAWINPUTHEADER);
    if (raw_read_buf.size() < required) raw_read_buf.resize(required);
    readSize = static_cast<UINT>(raw_read_buf.size());
    count = GetRawInputBuffer(
        reinterpret_cast<PRAWINPUT>(raw_read_buf.data()),
        &readSize,
        sizeof(RAWINPUTHEADER)
    );
  }

  if (count == (UINT)-1) {
    return;
  }

  PRAWINPUT current = reinterpret_cast<PRAWINPUT>(raw_read_buf.data());

  for (UINT i = 0; i < count; ++i) {
    if (current->header.dwType == RIM_TYPEMOUSE) {
#if defined(RAWMOUSE_DATA_OFFSET_WOW64)
      const RAWMOUSE* m = reinterpret_cast<const RAWMOUSE*>(reinterpret_cast<const char*>(current) + RAWMOUSE_DATA_OFFSET_WOW64);
#else
      const RAWMOUSE* m = &current->data.mouse;
#endif
      raw_mouse_accumulate_delta(m->lLastX, m->lLastY);
    }

    current = NEXTRAWINPUTBLOCK(current);
  }
#endif
}

static HWND RawMouseResolveTargetHwnd(HWND hwnd_hint) {
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

static bool RawMouseHasForeground(HWND hwnd) {
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
  if (!root_target)
    root_target = hwnd;
  if (!root_foreground)
    root_foreground = foreground;
  return root_target == root_foreground;
#else
  return hwnd == foreground;
#endif
}

static HWND RawMouseGetRoot(HWND hwnd) {
#if defined(GA_ROOT)
  HWND root = GetAncestor(hwnd, GA_ROOT);
  return root ? root : hwnd;
#else
  return hwnd;
#endif
}

void raw_mouse_ensure_registered(HWND hwnd_hint) {
  if (!raw_mouse_is_enabled() || !raw_mouse_api_supported()) return;
  if (raw_mouse_hwnd_target && !IsWindow(raw_mouse_hwnd_target)) {
    raw_mouse_registered = false;
    raw_mouse_hwnd_target = nullptr;
  }
  HWND hwnd = (hwnd_hint && IsWindow(hwnd_hint)) ? hwnd_hint : nullptr;
  if (!hwnd && raw_mouse_hwnd_target) hwnd = raw_mouse_hwnd_target;
  if (!hwnd) hwnd = GetActiveWindow();
  if (!hwnd) hwnd = GetForegroundWindow();
  if (!hwnd) return;
  HWND root = RawMouseGetRoot(hwnd);
  if (raw_mouse_registered && raw_mouse_hwnd_target && IsWindow(raw_mouse_hwnd_target) && RawMouseGetRoot(raw_mouse_hwnd_target) == root) return;
  raw_mouse_register_input(root, false);
}

void raw_mouse_release_cursor_clip() {
  if (raw_mouse_cursor_clipped) {
    ClipCursor(nullptr);
  }

  raw_mouse_cursor_clipped = false;
  SetRectEmpty(&raw_mouse_clip_rect);
}

void raw_mouse_refresh_cursor_clip(HWND hwnd_hint) {
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
  if (!ClientToScreen(hwnd, &top_left) ||
      !ClientToScreen(hwnd, &bottom_right)) {
    raw_mouse_release_cursor_clip();
    return;
  }

  RECT new_clip_rect = {top_left.x, top_left.y, bottom_right.x, bottom_right.y};
  new_clip_rect.left += RAW_MOUSE_CLIP_INSET;
  new_clip_rect.right -= RAW_MOUSE_CLIP_INSET;
  new_clip_rect.top += RAW_MOUSE_CLIP_INSET;
  new_clip_rect.bottom -= RAW_MOUSE_CLIP_INSET;
  if (new_clip_rect.left >= new_clip_rect.right ||
      new_clip_rect.top >= new_clip_rect.bottom) {
    raw_mouse_release_cursor_clip();
    return;
  }

  if (raw_mouse_cursor_clipped &&
      EqualRect(&raw_mouse_clip_rect, &new_clip_rect)) {
    return;
  }

  if (!ClipCursor(&new_clip_rect)) {
    raw_mouse_release_cursor_clip();
    return;
  }

  raw_mouse_cursor_clipped = true;
  raw_mouse_clip_rect = new_clip_rect;
}

bool raw_mouse_register_input(HWND hwnd, bool log_result) {
#if !SOFBUDDY_RAWINPUT_API_AVAILABLE
  if (log_result) {
    PrintOut(PRINT_BAD,
             "raw_mouse: Raw Input API is unavailable for this build target\n");
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
      PrintOut(PRINT_BAD,
               "raw_mouse: Failed to register raw input (error %d)\n", error);
      PrintOut(PRINT_BAD,
               "raw_mouse: On X11/Wine, raw input may not be supported\n");
    }
    raw_mouse_registered = false;
    raw_mouse_hwnd_target = nullptr;
    raw_mouse_release_cursor_clip();
    return false;
  }

  if (log_result) {
    PrintOut(PRINT_DEV,
             "raw_mouse: Successfully registered raw input device\n");
  }

  raw_mouse_registered = true;
  raw_mouse_hwnd_target = hwnd;
  raw_mouse_refresh_cursor_clip(hwnd);
  return true;
#endif
}

void raw_mouse_unregister_input(bool log_result) {
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
      PrintOut(PRINT_BAD,
               "raw_mouse: Failed to unregister raw input (error %d)\n", error);
    }
  }

  raw_mouse_registered = false;
  raw_mouse_hwnd_target = nullptr;
#endif
}

#endif // FEATURE_RAW_MOUSE