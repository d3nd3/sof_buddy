#pragma once

#include <stddef.h>
#include <stdint.h>

bool sof2_patch_imm32(uint8_t* insn, uint8_t opcode, uint32_t value);
bool sof2_hook_cdecl(void* target, void* detour, void** trampoline_out, size_t patch_len);
