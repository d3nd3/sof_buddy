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
#ifndef WM_QUIT
#define WM_QUIT 0x0012
#endif

static const int MAX_MSG_PER_FRAME = 5;

static void QuitPath();
static void SV_Shutdown(char*, int);

void PumpWindowMessagesCapped(bool from_winmain) {
  MSG msg;
  int processed = 0;
  while (processed < MAX_MSG_PER_FRAME &&
         PeekMessageA(&msg, nullptr, 0, WM_INPUT - 1, PM_REMOVE)) {
    if (msg.message == WM_QUIT) { if (from_winmain) SV_Shutdown(nullptr, 0); QuitPath(); }
    TranslateMessage(&msg);
    DispatchMessageA(&msg);
    ++processed;
  }
  while (processed < MAX_MSG_PER_FRAME &&
         PeekMessageA(&msg, nullptr, WM_INPUT + 1, 0xFFFFFFFF, PM_REMOVE)) {
    if (msg.message == WM_QUIT) { if (from_winmain) SV_Shutdown(nullptr, 0); QuitPath(); }
    TranslateMessage(&msg);
    DispatchMessageA(&msg);
    ++processed;
  }
}

static const uintptr_t RVA_SYS_MSG_TIME = 0x4034FC;
static const uintptr_t RVA_SYS_FRAME_TIME = 0x403528;
static const uintptr_t RVA_SV_Shutdown = 0x5FEC0;
static const uintptr_t RVA_QUIT_PATH = 0x65D71;
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
static void QuitPath() {
  (reinterpret_cast<void (*)()>(rvaToAbsExe(reinterpret_cast<void*>(RVA_QUIT_PATH))))();
}

void Sys_SendKeyEvents_Replacement() {
  PumpWindowMessagesCapped(false);

  #if FEATURE_MEDIA_TIMERS
  *SysFrameTimePtr() = my_TimeGetTime();
  #else
  *SysFrameTimePtr() = timeGetTime();
  #endif
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
  void* jmpAt = rvaToAbsExe(reinterpret_cast<void*>(RVA_WINMAIN_JMP_AT));
  WriteByte(reinterpret_cast<unsigned char*>(jmpAt), 0xEB);
  WriteByte(reinterpret_cast<unsigned char*>(jmpAt) + 1, static_cast<unsigned char>(RVA_WINMAIN_AFTER_LOOP - (RVA_WINMAIN_JMP_AT + 2)));
}

#endif
