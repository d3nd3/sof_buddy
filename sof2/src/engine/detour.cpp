#include "detour.h"
#include "mem_patch.h"
#include <windows.h>
#include <cstring>

bool sof2_patch_imm32(uint8_t* insn, uint8_t opcode, uint32_t value) {
	if (!insn || insn[0] != opcode) return false;
	return sof2_mem_write(insn + 1, &value, sizeof value);
}

bool sof2_hook_cdecl(void* target, void* detour, void** trampoline_out, size_t patch_len) {
	if (!target || !detour || !trampoline_out || patch_len < 5) return false;
	uint8_t* const at = (uint8_t*)target;
	uint8_t* tramp = (uint8_t*)VirtualAlloc(nullptr, patch_len + 5, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (!tramp) return false;
	memcpy(tramp, at, patch_len);
	const intptr_t back = (intptr_t)(at + patch_len) - (intptr_t)(tramp + patch_len + 5);
	tramp[patch_len] = 0xE9;
	memcpy(tramp + patch_len + 1, &back, 4);
	int32_t rel = (int32_t)((uint8_t*)detour - (at + 5));
	uint8_t jmp[5] = {0xE9, 0, 0, 0, 0};
	memcpy(jmp + 1, &rel, 4);
	if (!sof2_mem_write(at, jmp, 5)) {
		VirtualFree(tramp, 0, MEM_RELEASE);
		return false;
	}
	if (patch_len > 5) {
		const uint8_t nop = 0x90;
		for (size_t i = 5; i < patch_len; ++i) {
			if (!sof2_mem_write(at + i, &nop, 1)) {
				VirtualFree(tramp, 0, MEM_RELEASE);
				return false;
			}
		}
	}
	*trampoline_out = tramp;
	return true;
}
