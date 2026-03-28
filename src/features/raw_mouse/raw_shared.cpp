#include "feature_config.h"

#if FEATURE_RAW_MOUSE

#include "shared.h"
#include "util.h"
#include <limits.h>
#include <stddef.h>
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
static std::vector<BYTE> raw_read_buf;
static const uintptr_t RVA_ACTIVE_APP = 0x40351C;
static const uintptr_t RVA_CL_HWND = 0x403564;
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
void raw_mouse_ensure_registered(HWND hwnd_hint, bool log_register_attempts);

static int* RawMouseActiveAppPtr() {
  return reinterpret_cast<int*>(
      rvaToAbsExe(reinterpret_cast<void*>(RVA_ACTIVE_APP)));
}

static HWND* RawMouseClHwndPtr() {
  return reinterpret_cast<HWND*>(
      rvaToAbsExe(reinterpret_cast<void*>(RVA_CL_HWND)));
}

static bool RawMouseGameAppIsActive() {
  const int* active = RawMouseActiveAppPtr();
  return !active || *active != 0;
}

static HWND RawMouseGameWindowHwnd() {
  const HWND* hwnd = RawMouseClHwndPtr();
  return hwnd ? *hwnd : nullptr;
}

#if SOFBUDDY_RAWINPUT_API_AVAILABLE
static void RawMouseDrainInputBufferToDeltas() {
  for (int batch = 0; batch < 32; ++batch) {
    UINT size = 0;
    if (GetRawInputBuffer(nullptr, &size, sizeof(RAWINPUTHEADER)) == (UINT)-1 || size == 0)
      return;
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
  }
}
#endif

void raw_mouse_drain_pending_raw_for_cursor() {
#if !SOFBUDDY_RAWINPUT_API_AVAILABLE
  return;
#else
  if (!raw_mouse_is_enabled()) return;
  if (!raw_mouse_registered) raw_mouse_ensure_registered(nullptr, false);
  if (!RawMouseGameAppIsActive()) return;
  if (raw_mouse_hwnd_target && !RawMouseHasForeground(raw_mouse_hwnd_target)) return;
  RawMouseDrainInputBufferToDeltas();
#endif
}

void raw_mouse_process_raw_mouse() {
  raw_mouse_drain_pending_raw_for_cursor();
}

static bool RawMouseHwndIsOurProcess(HWND hwnd) {
  if (!hwnd || !IsWindow(hwnd)) return false;
  DWORD pid = 0;
  GetWindowThreadProcessId(hwnd, &pid);
  return pid == GetCurrentProcessId();
}

static HWND RawMouseGetRoot(HWND hwnd) {
#if defined(GA_ROOT)
  HWND root = GetAncestor(hwnd, GA_ROOT);
  return root ? root : hwnd;
#else
  return hwnd;
#endif
}

static bool RawMouseWindowClassEquals(HWND hwnd, const char* class_name) {
  if (!hwnd || !class_name) return false;
  char actual[128] = {};
  const int len = GetClassNameA(hwnd, actual, sizeof(actual));
  return len > 0 && lstrcmpiA(actual, class_name) == 0;
}

static HWND RawMouseNormalizeCandidateHwnd(HWND hwnd) {
  if (!hwnd || !IsWindow(hwnd)) return nullptr;
  HWND root = RawMouseGetRoot(hwnd);
  if (!root || !IsWindow(root)) return nullptr;
  if (!RawMouseHwndIsOurProcess(root)) return nullptr;

  const LONG_PTR style = GetWindowLongPtrA(root, GWL_STYLE);
  if ((style & WS_CHILD) != 0) return nullptr;

  return root;
}

static int RawMouseWindowCandidateScore(HWND hwnd) {
  hwnd = RawMouseNormalizeCandidateHwnd(hwnd);
  if (!hwnd) return INT_MIN;

  int score = 0;
  const HWND game_hwnd = RawMouseNormalizeCandidateHwnd(RawMouseGameWindowHwnd());
  if (IsWindowVisible(hwnd)) score += 32;
  if (GetWindow(hwnd, GW_OWNER) == nullptr) score += 16;
  if ((GetWindowLongPtrA(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) == 0) score += 8;
  if (RawMouseWindowClassEquals(hwnd, "Quake 2")) score += 64;
  if (game_hwnd && hwnd == game_hwnd) score += 256;

  const HWND active = RawMouseNormalizeCandidateHwnd(GetActiveWindow());
  if (hwnd == active) score += 24;

  const HWND foreground = RawMouseNormalizeCandidateHwnd(GetForegroundWindow());
  if (hwnd == foreground) score += 24;

  return score;
}

static void RawMouseConsiderCandidate(HWND candidate, HWND* best,
                                      int* best_score) {
  const int score = RawMouseWindowCandidateScore(candidate);
  if (score == INT_MIN) return;
  candidate = RawMouseNormalizeCandidateHwnd(candidate);
  if (!candidate) return;
  if (!*best || score > *best_score) {
    *best = candidate;
    *best_score = score;
  }
}

struct RawMouseThreadEnumState {
  HWND best;
  int best_score;
};

static BOOL CALLBACK RawMouseEnumThreadTopLevelProc(HWND hwnd, LPARAM lp) {
  auto* state = reinterpret_cast<RawMouseThreadEnumState*>(lp);
  if (!state) return FALSE;
  RawMouseConsiderCandidate(hwnd, &state->best, &state->best_score);
  return TRUE;
}

static HWND RawMouseBestTopLevelOnCurrentThread() {
  RawMouseThreadEnumState state = {};
  state.best_score = INT_MIN;
  EnumThreadWindows(GetCurrentThreadId(), RawMouseEnumThreadTopLevelProc,
                    reinterpret_cast<LPARAM>(&state));
  return state.best;
}

static void RawMouseLogWindowDetails(const char* label, HWND hwnd, int mode) {
  if (!label) return;
  if (!hwnd) {
    PrintOut(mode, "raw_mouse: %s: (null)\n", label);
    return;
  }

  const bool is_window = IsWindow(hwnd) != FALSE;
  DWORD pid = 0;
  DWORD tid = 0;
  if (is_window) {
    tid = GetWindowThreadProcessId(hwnd, &pid);
  }

  HWND root = is_window ? RawMouseGetRoot(hwnd) : nullptr;
  HWND owner = is_window ? GetWindow(hwnd, GW_OWNER) : nullptr;
  const LONG_PTR style = is_window ? GetWindowLongPtrA(hwnd, GWL_STYLE) : 0;
  const LONG_PTR exstyle = is_window ? GetWindowLongPtrA(hwnd, GWL_EXSTYLE) : 0;
  char class_name[128] = {};
  if (is_window) {
    GetClassNameA(hwnd, class_name, sizeof(class_name));
  }

  PrintOut(mode,
           "raw_mouse: %s: hwnd=%p valid=%d pid=%lu tid=%lu class=%s "
           "visible=%d owner=%p root=%p style=0x%08lx exstyle=0x%08lx\n",
           label, reinterpret_cast<void*>(hwnd), is_window ? 1 : 0,
           static_cast<unsigned long>(pid), static_cast<unsigned long>(tid),
           class_name[0] ? class_name : "<unknown>",
           is_window ? (IsWindowVisible(hwnd) ? 1 : 0) : 0,
           reinterpret_cast<void*>(owner), reinterpret_cast<void*>(root),
           static_cast<unsigned long>(style),
           static_cast<unsigned long>(exstyle));
}

#if SOFBUDDY_RAWINPUT_API_AVAILABLE
static void RawMouseLogRegisteredMouseDevice(int mode) {
  UINT count = 0;
  if (GetRegisteredRawInputDevices(nullptr, &count,
                                   sizeof(RAWINPUTDEVICE)) == (UINT)-1) {
    PrintOut(mode,
             "raw_mouse: GetRegisteredRawInputDevices failed (error %lu)\n",
             static_cast<unsigned long>(GetLastError()));
    return;
  }

  if (count == 0) {
    PrintOut(mode,
             "raw_mouse: No raw input device classes are registered in this "
             "process\n");
    return;
  }

  std::vector<RAWINPUTDEVICE> devices(count);
  UINT read =
      GetRegisteredRawInputDevices(devices.data(), &count,
                                   sizeof(RAWINPUTDEVICE));
  if (read == (UINT)-1) {
    PrintOut(mode,
             "raw_mouse: Failed to read registered raw input devices "
             "(error %lu)\n",
             static_cast<unsigned long>(GetLastError()));
    return;
  }

  bool found_mouse = false;
  for (UINT i = 0; i < read; ++i) {
    const RAWINPUTDEVICE& rid = devices[i];
    if (rid.usUsagePage != 0x01 || rid.usUsage != 0x02) continue;
    found_mouse = true;
    PrintOut(mode,
             "raw_mouse: Registered mouse raw input flags=0x%08lx "
             "hwndTarget=%p\n",
             static_cast<unsigned long>(rid.dwFlags),
             reinterpret_cast<void*>(rid.hwndTarget));
    RawMouseLogWindowDetails("registered mouse target", rid.hwndTarget, mode);
  }

  if (!found_mouse) {
    PrintOut(mode,
             "raw_mouse: No mouse raw input registration is currently present "
             "in this process\n");
  }
}
#endif

static void RawMouseLogResolutionState(HWND hwnd_hint, int mode) {
  PrintOut(mode, "raw_mouse: ActiveApp=%d\n", RawMouseGameAppIsActive() ? 1 : 0);
  RawMouseLogWindowDetails("cl_hwnd", RawMouseGameWindowHwnd(), mode);
  RawMouseLogWindowDetails("cached target", raw_mouse_hwnd_target, mode);
  RawMouseLogWindowDetails("hwnd hint", hwnd_hint, mode);

  GUITHREADINFO gti = {};
  gti.cbSize = sizeof(GUITHREADINFO);
  if (GetGUIThreadInfo(GetCurrentThreadId(), &gti)) {
    RawMouseLogWindowDetails("GUI thread active", gti.hwndActive, mode);
    RawMouseLogWindowDetails("GUI thread focus", gti.hwndFocus, mode);
  } else {
    PrintOut(mode, "raw_mouse: GetGUIThreadInfo failed (error %lu)\n",
             static_cast<unsigned long>(GetLastError()));
  }

  RawMouseLogWindowDetails("GetActiveWindow", GetActiveWindow(), mode);
  RawMouseLogWindowDetails("GetForegroundWindow", GetForegroundWindow(), mode);
  RawMouseLogWindowDetails("best thread top-level",
                           RawMouseBestTopLevelOnCurrentThread(), mode);
}

static HWND RawMouseResolveLocalWindow(HWND hwnd_hint) {
  HWND best = nullptr;
  int best_score = INT_MIN;

  RawMouseConsiderCandidate(RawMouseGameWindowHwnd(), &best, &best_score);
  RawMouseConsiderCandidate(raw_mouse_hwnd_target, &best, &best_score);
  RawMouseConsiderCandidate(hwnd_hint, &best, &best_score);

  GUITHREADINFO gti = {};
  gti.cbSize = sizeof(GUITHREADINFO);
  if (GetGUIThreadInfo(GetCurrentThreadId(), &gti)) {
    RawMouseConsiderCandidate(gti.hwndActive, &best, &best_score);
    RawMouseConsiderCandidate(gti.hwndFocus, &best, &best_score);
  }

  RawMouseConsiderCandidate(GetActiveWindow(), &best, &best_score);
  RawMouseConsiderCandidate(GetForegroundWindow(), &best, &best_score);
  RawMouseConsiderCandidate(RawMouseBestTopLevelOnCurrentThread(), &best,
                            &best_score);

  return best;
}

static HWND RawMouseResolveTargetHwnd(HWND hwnd_hint) {
  return RawMouseResolveLocalWindow(hwnd_hint);
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

static void RawMouseDropRegistration() {
  raw_mouse_registered = false;
  raw_mouse_hwnd_target = nullptr;
  raw_mouse_release_cursor_clip();
}

#if SOFBUDDY_RAWINPUT_API_AVAILABLE
/* HWND used as rid.hwndTarget must belong to this process: RegisterRawInputDevices
 * rejects foreign HWNDs (error 87 is typical), by design so another process cannot
 * attach raw input to arbitrary third-party windows. */
static HWND RawMouseFallbackRegistrationHwnd() {
  HWND w = RawMouseResolveLocalWindow(nullptr);
  if (w) {
    return w;
  }
  HWND fg = GetForegroundWindow();
  if (fg && RawMouseHwndIsOurProcess(fg)) {
    return RawMouseNormalizeCandidateHwnd(fg);
  }
  return nullptr;
}

/* Prefer focus-following (hwndTarget=NULL). If that fails with ERROR_INVALID_PARAMETER,
 * retry with an explicit top-level HWND from RawMouseFallbackRegistrationHwnd (always
 * same-process via RawMouseNormalizeCandidateHwnd). Registration may be called from any
 * thread; the device list is per-process. */
static bool RawMouseCommitRawInputRegistration(bool log_result) {
  RAWINPUTDEVICE rid = {};
  rid.usUsagePage = 0x01;
  rid.usUsage = 0x02;
  rid.dwFlags = 0;
  rid.hwndTarget = nullptr;

  if (!RegisterRawInputDevices(&rid, 1, sizeof(RAWINPUTDEVICE))) {
    const DWORD err_null = GetLastError();
    bool tried_fallback = false;
    if (err_null == ERROR_INVALID_PARAMETER) {
      HWND fallback = RawMouseFallbackRegistrationHwnd();
      if (fallback) {
        tried_fallback = true;
        rid.hwndTarget = fallback;
        if (RegisterRawInputDevices(&rid, 1, sizeof(RAWINPUTDEVICE))) {
          raw_mouse_registered = true;
          if (log_result) {
            PrintOut(PRINT_DEV,
                     "raw_mouse: Raw input registration succeeded using "
                     "explicit top-level hwnd (focus-following NULL was "
                     "rejected with error %lu)\n",
                     static_cast<unsigned long>(err_null));
            RawMouseLogWindowDetails("registration hwndTarget", fallback,
                                     PRINT_DEV);
            RawMouseLogRegisteredMouseDevice(PRINT_DEV);
          }
          return true;
        }
      }
    }
    const DWORD err_report = tried_fallback ? GetLastError() : err_null;
    if (log_result) {
      PrintOut(PRINT_BAD,
               "raw_mouse: Failed to register raw input (error %lu)\n",
               static_cast<unsigned long>(err_report));
      if (err_report == ERROR_INVALID_PARAMETER) {
        PrintOut(PRINT_BAD,
                 "raw_mouse: Parameter validation failed for focus-following "
                 "(NULL)%s\n",
                 tried_fallback ? " and explicit-hwnd fallback" : "");
        RawMouseLogResolutionState(nullptr, PRINT_BAD);
        RawMouseLogRegisteredMouseDevice(PRINT_BAD);
      } else if (is_running_under_wine()) {
        PrintOut(PRINT_BAD,
                 "raw_mouse: Under Wine/X11, raw input may be limited; try a "
                 "newer Proton or native Windows if problems persist\n");
      }
    }
    RawMouseDropRegistration();
    return false;
  }

  raw_mouse_registered = true;
  if (log_result) {
    PrintOut(PRINT_DEV,
             "raw_mouse: Raw input registration succeeded "
             "(focus-following mode, hwndTarget=NULL)\n");
    RawMouseLogRegisteredMouseDevice(PRINT_DEV);
  }
  return true;
}
#endif

void raw_mouse_ensure_registered(HWND hwnd_hint, bool log_register_attempts) {
  if (!raw_mouse_is_enabled() || !raw_mouse_api_supported()) return;

  if (raw_mouse_hwnd_target && !IsWindow(raw_mouse_hwnd_target)) {
    raw_mouse_hwnd_target = nullptr;
    raw_mouse_release_cursor_clip();
  }

  HWND hwnd = RawMouseResolveLocalWindow(
      (hwnd_hint && IsWindow(hwnd_hint)) ? hwnd_hint : nullptr);
  if (hwnd && (!raw_mouse_hwnd_target || !IsWindow(raw_mouse_hwnd_target) ||
               RawMouseGetRoot(raw_mouse_hwnd_target) != hwnd)) {
    raw_mouse_hwnd_target = hwnd;
    raw_mouse_refresh_cursor_clip(hwnd);
  }

  if (raw_mouse_registered) {
    if (!raw_mouse_hwnd_target && log_register_attempts) {
      PrintOut(PRINT_DEV,
               "raw_mouse: Raw input is already registered; waiting for a "
               "usable game window for cursor clip/focus tracking\n");
      RawMouseLogResolutionState(hwnd_hint, PRINT_DEV);
    }
    return;
  }
#if SOFBUDDY_RAWINPUT_API_AVAILABLE
  if (!RawMouseCommitRawInputRegistration(log_register_attempts)) return;
#endif

  if (raw_mouse_hwnd_target) {
    raw_mouse_refresh_cursor_clip(raw_mouse_hwnd_target);
  } else if (log_register_attempts) {
    PrintOut(PRINT_DEV,
             "raw_mouse: Raw input registered before the game window was "
             "resolved; clip will arm on later window events\n");
    RawMouseLogResolutionState(hwnd_hint, PRINT_DEV);
  }
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

void raw_mouse_unregister_input(bool log_result) {
#if !SOFBUDDY_RAWINPUT_API_AVAILABLE
  (void)log_result;
  RawMouseDropRegistration();
  return;
#else
  raw_mouse_release_cursor_clip();

  if (!raw_mouse_registered) {
    raw_mouse_hwnd_target = nullptr;
    return;
  }

  RAWINPUTDEVICE rid = {};
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
