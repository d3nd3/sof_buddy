#pragma once

#include "feature_config.h"

#if FEATURE_RAW_MOUSE

#include "sof_compat.h"
#include <windows.h>
#include <vector>

using tSetCursorPos = BOOL(__stdcall*)(int X, int Y);

extern tSetCursorPos oSetCursorPos;
extern cvar_t *in_mouse_raw;
extern int raw_mouse_delta_x;
extern int raw_mouse_delta_y;
extern POINT window_center;
extern std::vector<BYTE> g_heapBuffer;

void raw_mouse_on_change(cvar_t *cvar);
void create_raw_mouse_cvars(void);
BOOL __stdcall hkSetCursorPos(int X, int Y);

#endif // FEATURE_RAW_MOUSE


