#ifndef sofbuddyh
#define sofbuddyh
#include <windows.h>
extern HMODULE o_sofplus;
void afterSoFplusInit(void);

DWORD WINAPI sofbuddy_thread(LPVOID lpParam);
#endif