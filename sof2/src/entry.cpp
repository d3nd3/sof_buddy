#include "feature.h"
#include <windows.h>

extern void sof2_wsock_link(void);

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID) {
	if (reason != DLL_PROCESS_ATTACH) return TRUE;
	DisableThreadLibraryCalls(inst);
	sof2_wsock_link();
	sof2_lifecycle_start();
	return TRUE;
}
