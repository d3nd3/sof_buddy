#include "mem_patch.h"
#include <windows.h>
#include <string.h>

bool sof2_mem_write(void* dst, const void* src, size_t n) {
	DWORD prot = 0;
	if (!VirtualProtect(dst, n, PAGE_EXECUTE_READWRITE, &prot)) return false;
	memcpy(dst, src, n);
	FlushInstructionCache(GetCurrentProcess(), dst, n);
	VirtualProtect(dst, n, prot, &prot);
	return true;
}
