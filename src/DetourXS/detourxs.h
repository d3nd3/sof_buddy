#ifndef DETOURXS_H
#define DETOURXS_H

#include <windows.h>

#define DETOUR_LEN_AUTO 0 // Finds the detour length automatically

enum
{
	DETOUR_TYPE_JMP, // min detour len: 5
	DETOUR_TYPE_PUSH_RET, // min detour len: 6
	DETOUR_TYPE_NOP_JMP, // min detour len: 6
	DETOUR_TYPE_NOP_NOP_JMP, // min detour len: 7
	DETOUR_TYPE_STC_JC, // min detour len: 7
	DETOUR_TYPE_CLC_JNC, // min detour len: 7
};

LPVOID DetourCreate(LPVOID lpFuncOrig, LPVOID lpFuncDetour, int patchType, int detourLen=DETOUR_LEN_AUTO);
LPVOID DetourCreate(LPCSTR lpModuleName, LPCSTR lpProcName, LPVOID lpFuncDetour, int patchType, int detourLen=DETOUR_LEN_AUTO);
BOOL DetourRemove(void** lppDetourCreatePtr);

/** Re-encode the first-instruction E8 rel32 call for execution at `trampoline` (same 5 bytes, new displacement). */
void DetourFixupTrampolineRel32Call(void* trampoline, void* targetAbsolute);

#endif // DETOURXS_H
