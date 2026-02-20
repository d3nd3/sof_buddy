#pragma once

#include "feature_config.h"

#if FEATURE_RAW_MOUSE

#include "sof_compat.h"
#include <windows.h>

#ifndef WM_INPUT
#define WM_INPUT 0x00FF
#endif

#ifndef MOUSE_MOVE_ABSOLUTE
#define MOUSE_MOVE_ABSOLUTE 0x0001
#endif

#ifndef RIDEV_REMOVE
#define RIDEV_REMOVE 0x00000001
#endif

#if defined(_WIN32_WINNT) && (_WIN32_WINNT >= 0x0501) && defined(RID_INPUT) && defined(RIM_TYPEMOUSE)
#define SOFBUDDY_RAWINPUT_API_AVAILABLE 1
#else
#define SOFBUDDY_RAWINPUT_API_AVAILABLE 0
#endif

using tSetCursorPos = BOOL(__stdcall*)(int X, int Y);

extern tSetCursorPos oSetCursorPos;
extern cvar_t *in_mouse_raw;
extern int raw_mouse_delta_x;
extern int raw_mouse_delta_y;
extern POINT window_center;
extern bool raw_mouse_center_valid;
extern bool raw_mouse_registered;
extern bool raw_mouse_cursor_clipped;
extern HWND raw_mouse_hwnd_target;
extern bool raw_processed_this_frame;

inline void raw_mouse_on_peek_returned_no_message() { raw_processed_this_frame = false; }
void raw_mouse_on_change(cvar_t *cvar);
void create_raw_mouse_cvars(void);
bool raw_mouse_is_enabled();
bool raw_mouse_api_supported();
void raw_mouse_reset_deltas();
void raw_mouse_consume_deltas();
void raw_mouse_update_center(int x, int y);
void raw_mouse_accumulate_delta(LONG dx, LONG dy);
void raw_mouse_ensure_registered(HWND hwnd_hint);
void raw_mouse_refresh_cursor_clip(HWND hwnd_hint);
void raw_mouse_release_cursor_clip();
bool raw_mouse_register_input(HWND hwnd, bool log_result);
void raw_mouse_unregister_input(bool log_result);
void raw_mouse_process_raw_mouse();
void PumpWindowMessagesCapped(bool from_winmain);
void Sys_SendKeyEvents_Replacement();
void raw_mouse_install_winmain_peek_detour();
BOOL __stdcall hkSetCursorPos(int X, int Y);

#endif // FEATURE_RAW_MOUSE