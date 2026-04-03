#include "feature_config.h"

#if FEATURE_RAW_MOUSE

#include "shared.h"
#include "util.h"
#include <windows.h>
#include <mmsystem.h>
#if FEATURE_MEDIA_TIMERS
#include "features/media_timers/shared.h"
#endif

#ifndef WM_INPUT
#define WM_INPUT 0x00FF
#endif

#if defined(_MSC_VER)
#define RAW_MOUSE_NOINLINE __declspec(noinline)
#else
#define RAW_MOUSE_NOINLINE __attribute__((noinline))
#endif

/* Vanilla Q2-style pumps drain the whole queue each Sys_SendKeyEvents; we cap
 * so WM_MOUSEMOVE storms cannot eat the frame (see e.g. PeekMessage cost with
 * high-frequency mouse messages). There is no official number—5 is very safe,
 * 10–15 is a common middle ground for modern poll rates + resize/activate bursts. */
static const int MAX_MSG_PER_FRAME = 10;

static void QuitPath();
static void SV_Shutdown(char*, int);
static unsigned* SysMsgTimePtr();


static void DispatchOneMsg(MSG* m, bool from_winmain) {
  if (m->message == WM_QUIT) {
    if (from_winmain) {
      SV_Shutdown(nullptr, 0);
    }
    QuitPath();
  }
  *SysMsgTimePtr() = static_cast<unsigned>(m->time);
  TranslateMessage(m);
  DispatchMessageA(m);
}

void PumpWindowMessagesCapped(bool from_winmain) {
  /* Vanilla Q2-style: drain the whole queue (matches classic WinMain inner loop).
   * No WM_INPUT banding — not needed when raw mouse is off. */
  if (!raw_mouse_is_enabled()) {
    MSG msg;
    while (PeekMessageA(&msg, nullptr, 0, 0, PM_NOREMOVE)) {
      if (!GetMessageA(&msg, nullptr, 0, 0)) {
        if (from_winmain) {
          SV_Shutdown(nullptr, 0);
        }
        QuitPath();
      }
      DispatchOneMsg(&msg, from_winmain);
    }
    return;
  }

  MSG msg;
  MSG low_msg;
  MSG high_msg;
  int processed = 0;
  /* WM_INPUT is never removed (avoids high-frequency peel cost). When it sits at
   * the queue head, PeekMessage with ID bands skips it and removes the next matching
   * message. When the head is not WM_INPUT, GetMessage preserves strict FIFO.
   * If both bands have a candidate (WM_INPUT at head, or interleaved), msg.time
   * picks the earlier; tie-break prefers high band (mouse / client-area messages). */
  while (processed < MAX_MSG_PER_FRAME &&
         PeekMessageA(&msg, nullptr, 0, 0, PM_NOREMOVE)) {
    if (msg.message != WM_INPUT) {
      if (!GetMessageA(&msg, nullptr, 0, 0)) {
        if (from_winmain) {
          SV_Shutdown(nullptr, 0);
        }
        QuitPath();
      }
      DispatchOneMsg(&msg, from_winmain);
      ++processed;
      continue;
    }

    const BOOL has_low =
        PeekMessageA(&low_msg, nullptr, 0, WM_INPUT - 1, PM_NOREMOVE);
    const BOOL has_high = PeekMessageA(&high_msg, nullptr, WM_INPUT + 1, 0xFFFFFFFF,
                                        PM_NOREMOVE);
    if (!has_low && !has_high) {
      break;
    }
    if (has_low && !has_high) {
      if (!PeekMessageA(&msg, nullptr, 0, WM_INPUT - 1, PM_REMOVE)) {
        break;
      }
    } else if (!has_low && has_high) {
      if (!PeekMessageA(&msg, nullptr, WM_INPUT + 1, 0xFFFFFFFF, PM_REMOVE)) {
        break;
      }
    } else {
      const DWORD t_low = low_msg.time;
      const DWORD t_high = high_msg.time;
      if (t_low < t_high) {
        if (!PeekMessageA(&msg, nullptr, 0, WM_INPUT - 1, PM_REMOVE)) {
          break;
        }
      } else if (t_high < t_low) {
        if (!PeekMessageA(&msg, nullptr, WM_INPUT + 1, 0xFFFFFFFF, PM_REMOVE)) {
          break;
        }
      } else {
        if (!PeekMessageA(&msg, nullptr, WM_INPUT + 1, 0xFFFFFFFF, PM_REMOVE)) {
          break;
        }
      }
    }
    DispatchOneMsg(&msg, from_winmain);
    ++processed;
  }
}

static const uintptr_t RVA_SYS_MSG_TIME = 0x4034FC;
static const uintptr_t RVA_SYS_FRAME_TIME = 0x403528;
static const uintptr_t RVA_SV_Shutdown = 0x5FEC0;
static const uintptr_t RVA_QUIT_PATH = 0x655C0;
static const uintptr_t RVA_CL_Shutdown = 0xDDE0;
static const uintptr_t RVA_sub_after_CL_Shutdown = 0x1FB10;
static const uintptr_t RVA_FreeMemory = 0xF9F75;
/* RVA_QUIT_PATH does: timeEndPeriod(1), CL_Shutdown(RVA_CL_Shutdown), sub(RVA_sub_after_CL_Shutdown), dedicated cvar check, winstart buffer check, FreeMemory(RVA_FreeMemory), _exit(0) */

static unsigned* SysMsgTimePtr() {
  return reinterpret_cast<unsigned*>(rvaToAbsExe(reinterpret_cast<void*>(RVA_SYS_MSG_TIME)));
}
static unsigned* SysFrameTimePtr() {
  return reinterpret_cast<unsigned*>(rvaToAbsExe(reinterpret_cast<void*>(RVA_SYS_FRAME_TIME)));
}
static void SV_Shutdown(char* a, int b) {
  (reinterpret_cast<void (*)(char*, int)>(rvaToAbsExe(reinterpret_cast<void*>(RVA_SV_Shutdown))))(a, b);
}
static RAW_MOUSE_NOINLINE void QuitPath() {
  (reinterpret_cast<void (*)()>(rvaToAbsExe(reinterpret_cast<void*>(RVA_QUIT_PATH))))();
}

void raw_mouse_write_engine_frame_time() {
#if FEATURE_MEDIA_TIMERS
  *SysFrameTimePtr() = my_TimeGetTime();
#else
  *SysFrameTimePtr() = timeGetTime();
#endif
}

void Sys_SendKeyEvents_Replacement() {
  PumpWindowMessagesCapped(false);
  raw_mouse_write_engine_frame_time();
}

void WinMainPeekLoop() {
  PumpWindowMessagesCapped(true);
}

static const uintptr_t RVA_WINMAIN_PEEK_CALL = 0x663B3;
static const uintptr_t RVA_WINMAIN_JMP_AT = 0x663C4;
static const uintptr_t RVA_WINMAIN_AFTER_LOOP = 0x66412;

void raw_mouse_install_winmain_peek_detour() {

  void* at = rvaToAbsExe(reinterpret_cast<void*>(RVA_WINMAIN_PEEK_CALL));
  WriteE8Call(at, reinterpret_cast<void*>(&WinMainPeekLoop));

  WriteNops(reinterpret_cast<unsigned char*>(at) + 5, 10);

  //Skip over direct to Sys_Milliseconds part
  void* jmpAt = rvaToAbsExe(reinterpret_cast<void*>(RVA_WINMAIN_JMP_AT));
  WriteByte(reinterpret_cast<unsigned char*>(jmpAt), 0xEB);
  WriteByte(reinterpret_cast<unsigned char*>(jmpAt) + 1, static_cast<unsigned char>(RVA_WINMAIN_AFTER_LOOP - (RVA_WINMAIN_JMP_AT + 2)));
}

#endif
