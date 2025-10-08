
#include "windows.h"
#include "detourxs.h"
#include <stdio.h>
#include "ADE32.h"
#include "util.h"

#pragma warning(disable: 4311)
#pragma warning(disable: 4312)
#pragma warning(disable: 4244)

#define DETOUR_MAX_SRCH_OPLEN 64

#define JMP32_SZ 5
#define BIT32_SZ 4

// jmp32 sig
#define SIG_SZ 3
#define SIG_OP_0 0xCC
#define SIG_OP_1 0x90
#define SIG_OP_2 0xC3

int GetDetourLen(int patchType);
int GetDetourLenAuto(PBYTE &pbFuncOrig, int minDetLen);

// Thin wrapper for APIs
LPVOID DetourCreate(LPCSTR lpModuleName, LPCSTR lpProcName, LPVOID lpFuncDetour, int patchType, int detourLen)
{
	LPVOID lpFuncOrig = NULL;

		
	if((lpFuncOrig = (void*)GetProcAddress(GetModuleHandle(lpModuleName), lpProcName)) == NULL)
		return NULL;

	return DetourCreate(lpFuncOrig, lpFuncDetour, patchType, detourLen);
}


LPVOID DetourCreate(LPVOID lpFuncOrig, LPVOID lpFuncDetour, int patchType, int detourLen)
{

	LPVOID lpMallocPtr = NULL;
		DWORD dwProt = 0;
	PBYTE pbMallocPtr = NULL;
	PBYTE pbFuncOrig = (PBYTE)lpFuncOrig;
	PBYTE pbFuncDetour = (PBYTE)lpFuncDetour;
	PBYTE pbPatchBuf = NULL;
	int minDetLen = 0;
	int detLen = 0;

	// Get detour length
	if((minDetLen = GetDetourLen(patchType)) == 0)
		return NULL;

	if(detourLen != DETOUR_LEN_AUTO)
		detLen = detourLen;

	else
	{
		detLen = GetDetourLenAuto(pbFuncOrig, minDetLen);
		if(detLen < minDetLen)
			return NULL;
		// Debug: log computed auto detour length
		{
			char dbg[256];
			sprintf(dbg, "DetourCreate: AUTO detour length computed = %d (min=%d) for orig=0x%08X\n", detLen, minDetLen, (unsigned int)(uintptr_t)pbFuncOrig);
			PrintOut(PRINT_LOG, "%s", dbg);
		}
	}

    // Alloc executable memory for the trampoline (must be executable for
    // the CPU to jump into it). Using VirtualAlloc with PAGE_EXECUTE_READWRITE
    // ensures the returned buffer is executable even on systems with DEP.
    SIZE_T allocSize = detLen + JMP32_SZ + SIG_SZ;
    lpMallocPtr = VirtualAlloc(NULL, allocSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (lpMallocPtr == NULL)
        return NULL;

    pbMallocPtr = (PBYTE)lpMallocPtr;

    // Enable writing to original
    if (!VirtualProtect(lpFuncOrig, detLen, PAGE_READWRITE, &dwProt)) {
        char errbuf[256];
        sprintf(errbuf, "DetourCreate: VirtualProtect failed for %p (detLen=%d)\n", lpFuncOrig, detLen);
        PrintOut(PRINT_LOG, "%s", errbuf);
        VirtualFree(lpMallocPtr, 0, MEM_RELEASE);
        return NULL;
    }

	// Write overwritten bytes to the malloc
	memcpy(lpMallocPtr, lpFuncOrig, detLen);
	pbMallocPtr += detLen;
    pbMallocPtr[0] = 0xE9;
    *(DWORD*)(pbMallocPtr+1) = (DWORD)((pbFuncOrig+detLen)-pbMallocPtr)-JMP32_SZ;
    // Validate trampoline jmp back resolves to the expected resume address
    {
        DWORD rel = *(DWORD*)(pbMallocPtr+1);
        DWORD jmp_from = (DWORD)(pbMallocPtr);
        DWORD jmp_dest = jmp_from + JMP32_SZ + rel;
        DWORD expected = (DWORD)(pbFuncOrig + detLen);
        if (jmp_dest != expected) {
            char errbuf[256];
            sprintf(errbuf, "DetourCreate: trampoline jmp_dest=0x%08X != expected resume=0x%08X for orig=0x%08X\n", jmp_dest, expected, (unsigned int)(uintptr_t)pbFuncOrig);
            PrintOut(PRINT_LOG, "%s", errbuf);
            // continue; we still return trampoline but log mismatch
        }
    }
	pbMallocPtr += JMP32_SZ;
	pbMallocPtr[0] = SIG_OP_0;
	pbMallocPtr[1] = SIG_OP_1;
	pbMallocPtr[2] = SIG_OP_2;

    // Create a buffer to prepare the detour bytes
    pbPatchBuf = new BYTE[detLen];
    memset(pbPatchBuf, 0x90, detLen);

	switch(patchType)
	{
        case DETOUR_TYPE_JMP:
            pbPatchBuf[0] = 0xE9;
            *(DWORD*)&pbPatchBuf[1] = (DWORD)(pbFuncDetour - pbFuncOrig) - 5;
            // Validate target address encoded in patch buffer
            {
                DWORD rel = *(DWORD*)&pbPatchBuf[1];
                DWORD dest = (DWORD)((DWORD)pbFuncOrig + 5 + rel);
                if (dest != (DWORD)(uintptr_t)pbFuncDetour) {
                    char errbuf[256];
                    sprintf(errbuf, "DetourCreate: patch jmp dest=0x%08X != detour_fn=0x%08X (orig=0x%08X)\n", dest, (unsigned int)(uintptr_t)pbFuncDetour, (unsigned int)(uintptr_t)pbFuncOrig);
                    PrintOut(PRINT_LOG, "%s", errbuf);
                }
            }
            break;

		case DETOUR_TYPE_PUSH_RET:
			pbPatchBuf[0] = 0x68;
			*(DWORD*)&pbPatchBuf[1] = (DWORD)pbFuncDetour;
			pbPatchBuf[5] = 0xC3;
			break;

		case DETOUR_TYPE_NOP_JMP:
			pbPatchBuf[0] = 0x90;
			pbPatchBuf[1] = 0xE9;
			*(DWORD*)&pbPatchBuf[2] = (DWORD)(pbFuncDetour - pbFuncOrig) - 6;
			break;

		case DETOUR_TYPE_NOP_NOP_JMP:
			pbPatchBuf[0] = 0x90;
			pbPatchBuf[1] = 0x90;
			pbPatchBuf[2] = 0xE9;
			*(DWORD*)&pbPatchBuf[3] = (DWORD)(pbFuncDetour - pbFuncOrig) - 7;
			break;

		case DETOUR_TYPE_STC_JC:
			pbPatchBuf[0] = 0xF9;
			pbPatchBuf[1] = 0x0F;
			pbPatchBuf[2] = 0x82;
			*(DWORD*)&pbPatchBuf[3] = (DWORD)(pbFuncDetour - pbFuncOrig) - 7;
			break;

		case DETOUR_TYPE_CLC_JNC:
			pbPatchBuf[0] = 0xF8;
			pbPatchBuf[1] = 0x0F;
			pbPatchBuf[2] = 0x83;
			*(DWORD*)&pbPatchBuf[3] = (DWORD)(pbFuncDetour - pbFuncOrig) - 7;
			break;
		
		default:
			return NULL;
	}

    // Write the detour into the original function
    for(int i=0; i<detLen; i++)
        pbFuncOrig[i] = pbPatchBuf[i];

    // Ensure the CPU instruction cache is coherent after patching code
    FlushInstructionCache(GetCurrentProcess(), lpFuncOrig, detLen);

    // Clean up patch buffer
    delete [] pbPatchBuf;

    // Reset original mem flags
    VirtualProtect(lpFuncOrig, detLen, dwProt, &dwProt);

    return lpMallocPtr;
}

BOOL DetourRemove(void** lppDetourCreatePtr)
{

	LPVOID lpDetourCreatePtr = *lppDetourCreatePtr;
	PBYTE pbMallocPtr = NULL;
	DWORD dwFuncOrig = 0;
	DWORD dwProt = 0;
	int i=0;

	if((pbMallocPtr = (PBYTE)lpDetourCreatePtr) == NULL)
		return FALSE;

	// Find the orig jmp32 opcode sig
	for(i=0; i<=DETOUR_MAX_SRCH_OPLEN; i++)
	{
		if(pbMallocPtr[i] == SIG_OP_0 
			&& pbMallocPtr[i+1] == SIG_OP_1
			&& pbMallocPtr[i+2] == SIG_OP_2)
			break;

		if(i == DETOUR_MAX_SRCH_OPLEN)
			return FALSE;
	}

	// Calculate the original address
	pbMallocPtr += (i-JMP32_SZ+1); // Inc to jmp
	dwFuncOrig = *(DWORD*)pbMallocPtr; // Get 32bit jmp
	pbMallocPtr += BIT32_SZ; // Inc to end of jmp
	dwFuncOrig += (DWORD)pbMallocPtr; // Add this addr to 32bit jmp
	dwFuncOrig -= (i-JMP32_SZ); // Dec by detour len to get to start of orig

    // Write the overwritten bytes back to the original
    VirtualProtect((LPVOID)dwFuncOrig, (i-JMP32_SZ), PAGE_READWRITE, &dwProt);
    memcpy((LPVOID)dwFuncOrig, lpDetourCreatePtr, (i-JMP32_SZ));

    // Ensure CPU instruction cache coherency after restoring original code
    FlushInstructionCache(GetCurrentProcess(), (LPVOID)dwFuncOrig, (i-JMP32_SZ));

    VirtualProtect((LPVOID)dwFuncOrig, (i-JMP32_SZ), dwProt, &dwProt);

	*lppDetourCreatePtr = NULL;
	return TRUE;
}

int GetDetourLen(int patchType)
{
	switch(patchType)
	{
		case DETOUR_TYPE_JMP:
			return 5;

		case DETOUR_TYPE_PUSH_RET:
		case DETOUR_TYPE_NOP_JMP:
			return 6;
		
		case DETOUR_TYPE_NOP_NOP_JMP:
		case DETOUR_TYPE_STC_JC:
		case DETOUR_TYPE_CLC_JNC:
			return 7;
		
		default:
			return 0;
	}
}

int GetDetourLenAuto(PBYTE &pbFuncOrig, int minDetLen)
{
	int len = 0;
	PBYTE pbCurOp = pbFuncOrig;

	while(len < minDetLen)
	{
		int i = oplen(pbCurOp);
		
		if(i == 0 || i == -1)
			return 0;

		if(len > DETOUR_MAX_SRCH_OPLEN)
			return 0;

		len += i;
		pbCurOp += i;
	}

	return len;
}
