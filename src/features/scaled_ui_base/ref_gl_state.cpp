#include "feature_config.h"

#if FEATURE_SCALED_CON || FEATURE_SCALED_HUD

#include <stdint.h>
#include <string.h>
#include "sof_compat.h"
#include "util.h"
#include "ref_gl_state.h"

#define GL_BLEND 0x0BE2

typedef void (__attribute__((stdcall)) *tDisableCapFn)(unsigned cap);
static tDisableCapFn s_disableCap = nullptr;
static void (__cdecl *s_checkComplex)(void) = nullptr;

// Draw_Pic: mov eax, GL_ptr
static constexpr uintptr_t kGlPtrInsnRva = 0x1F63;

static void* ref_gl_ctx() {
	unsigned char const* insn = (unsigned char const*)rvaToAbsRef((void*)kGlPtrInsnRva);
	uint32_t mem = 0;
	if (insn[0] == 0xA1) {
		memcpy(&mem, insn + 1, 4);
	} else if (insn[0] == 0x8B && (insn[1] == 0x05 || insn[1] == 0x0D)) {
		memcpy(&mem, insn + 2, 4);
	} else {
		SOFBUDDY_ASSERT(false);
		return nullptr;
	}
	return (void*)(uintptr_t)*(uint32_t const*)mem;
}

static void call_disable_cap(void* gl, unsigned cap) {
#if defined(__i386__)
	asm volatile(
		"movl %1, %%ecx\n\t"
		"pushl %2\n\t"
		"call *%0\n\t"
		"addl $4, %%esp"
		:
		: "r"(s_disableCap), "r"(gl), "r"(cap)
		: "ecx", "memory");
#else
	(void)gl;
	(void)cap;
#endif
}

void ref_gl_state_init() {
	s_disableCap = (tDisableCapFn)rvaToAbsRef((void*)0xD740);
	s_checkComplex = (void(__cdecl*)(void))rvaToAbsRef((void*)0x1B650);
	SOFBUDDY_ASSERT(s_disableCap != nullptr);
	SOFBUDDY_ASSERT(s_checkComplex != nullptr);
	SOFBUDDY_ASSERT(ref_gl_ctx() != nullptr);
}

void ref_gl_cache_disable_blend() {
	if (!s_disableCap) return;
	void* gl = ref_gl_ctx();
	if (!gl) return;
	call_disable_cap(gl, GL_BLEND);
}

void ref_gl_cache_sync() {
	if (s_checkComplex) s_checkComplex();
}

#endif
