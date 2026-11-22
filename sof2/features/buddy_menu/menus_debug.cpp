#include "menus_debug.h"
#include "detour.h"
#include "engine_internal.h"
#include "game_module.h"
#include "mem_patch.h"
#include "profile_mp_103.h"
#include "sof2_api.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static volatile LONG g_ba70_block_n = 0;
static volatile LONG g_ba70_depth = 0;
static volatile LONG g_token_advance_n = 0;
static uintptr_t g_rmf_stm_lo = 0;
static uintptr_t g_rmf_stm_hi = 0;

static void note_rmf_stm_range(const void* file) {
	const uintptr_t* w = (const uintptr_t*)file;
	g_rmf_stm_lo = g_rmf_stm_hi = 0;
	if (!w || !w[0] || !w[2] || w[2] <= w[0]) return;
	g_rmf_stm_lo = (uintptr_t)w[0];
	g_rmf_stm_hi = (uintptr_t)w[2];
}

static int ptr_in_rmf_stm(const void* p) {
	if (!p || !g_rmf_stm_hi) return 0;
	const uintptr_t u = (uintptr_t)p;
	return u >= g_rmf_stm_lo && u < g_rmf_stm_hi;
}

static void menus_native_heap_free(void* p) {
	HMODULE menus = GetModuleHandleA("Menusx86.dll");
	if (!menus || !p) return;
	using Fn = void(__cdecl*)(void*);
	((Fn)((uint8_t*)menus + 0x251BCu))(p);
}
#if defined(__i386__)
extern "C" void* g_rmf_token_adv;
#endif

void mnu_diag_file(const char* msg) {
	if (!msg) return;
	FILE* f = fopen("sof2bud_mnu.log", "a");
	if (!f) return;
	fprintf(f, "%s\n", msg);
	fclose(f);
}

namespace {

constexpr uint32_t kMenusImportRva = 0x375D8u;
constexpr size_t kHookLen = 6u;
constexpr uint32_t kRvaMenuOpenFrame = 0x0B680u;
constexpr uint32_t kRvaMenuCreateFrame = 0x08CA0u;
constexpr uint32_t kRvaMenuLoadAndPush = 0x02E50u;
constexpr uint32_t kRvaBe75CallRmf = 0x0BE75u;
constexpr uint32_t kRvaBe20TeardownSite = 0x0BEB9u;
constexpr uint32_t kRvaBa70SkipStrFree = 0x0BB8Eu;
constexpr uint32_t kRvaRmfLoadFile = 0x22D40u;
constexpr uint32_t kRvaSub4E40 = 0x04E40u;
constexpr uint32_t kRvaTintCallDefineTail = 0x0BB7Fu;
constexpr uint32_t kRvaConstCallDefineTail = 0x0BC95u;
constexpr uint32_t kRvaFontCallDefineTail = 0x0BCF7u;
constexpr uint32_t kRvaShaderCallDefineTail = 0x0BDF7u;
constexpr uint32_t kRvaBe20FreeRmfObj = 0x0BEC1u;
constexpr uint32_t kRvaBeacCallBa70 = 0x0BEACu;
constexpr uint32_t kRvaBe98TokenAdvance = 0x0BE98u;
constexpr uint32_t kRvaRmfTokenAdvance = 0x23180u;
constexpr uint32_t kRvaRmfDefineTail = 0x23680u;
constexpr uint32_t kRvaMenusDelete = 0x23960u;
constexpr uint32_t kRvaMenusHeapFree = 0x251BCu;
constexpr uint32_t kRvaMenusUiGlobal = 0x37558u; /* dword_40037558 — langfont path [ptr+0x60] */
constexpr uint32_t kRvaRmfSkipBufFree = 0x22FAFu;
volatile LONG g_dbg_hooks = 0;
volatile LONG g_mnu_depth = 0;
volatile LONG g_trace_imports = 0;
volatile LONG g_past_defines_shaders = 0;
volatile LONG g_lex_log_budget = 0;
volatile LONG g_define_tail_log_n = 0;

void mnu_log(const char* arrow, int depth, const char* fn, const char* detail) {
	const Sof2EngineApi* e = sof2_engine();
	if (!e || !e->Com_Printf) return;
	if (detail && detail[0])
		e->Com_Printf("sof2bud: mnu[%d] %s %s (%s)\n", depth, arrow, fn, detail);
	else
		e->Com_Printf("sof2bud: mnu[%d] %s %s\n", depth, arrow, fn);
}

void mnu_enter(const char* fn, const char* detail) {
	const int d = (int)InterlockedIncrement(&g_mnu_depth);
	mnu_log(">", d, fn, detail);
}

void mnu_leave(const char* fn, const char* detail) {
	const int d = (int)g_mnu_depth;
	mnu_log("<", d, fn, detail);
	InterlockedDecrement(&g_mnu_depth);
}

#define MNUDETAIL(buf, fmt, ...) snprintf(buf, sizeof buf, fmt, ##__VA_ARGS__)

using Menu_OpenFrame_t = char (__attribute__((thiscall)) *)(void* self, const char* pack, char* frame);
static Menu_OpenFrame_t Menu_OpenFrame_orig = nullptr;

char __attribute__((thiscall)) Menu_OpenFrame_hook(void* self, const char* pack, char* frame) {
	char detail[96];
	MNUDETAIL(detail, "pack=%s frame=%s child=%p", pack ? pack : "?", frame ? frame : "?", self ? *(void**)((char*)self + 4) : nullptr);
	mnu_enter("Menu_OpenFrame", detail);
	const char r = Menu_OpenFrame_orig(self, pack, frame);
	MNUDETAIL(detail, "ret=%d", (int)(unsigned char)r);
	mnu_leave("Menu_OpenFrame", detail);
	return r;
}

using Menu_CreateFrame_t = char (__attribute__((thiscall)) *)(void* self, const char* pack, char* frame, float a4);
static Menu_CreateFrame_t Menu_CreateFrame_orig = nullptr;

char __attribute__((thiscall)) Menu_CreateFrame_hook(void* self, const char* pack, char* frame, float a4) {
	char detail[96];
	MNUDETAIL(detail, "pack=%s frame=%s", pack ? pack : "?", frame ? frame : "?");
	mnu_enter("Menu_CreateFrame", detail);
	const char r = Menu_CreateFrame_orig(self, pack, frame, a4);
	MNUDETAIL(detail, "ret=%d", (int)(unsigned char)r);
	mnu_leave("Menu_CreateFrame", detail);
	return r;
}

using Menu_LoadAndPushFrame_t = char (__attribute__((thiscall)) *)(void* self, float a2);
static Menu_LoadAndPushFrame_t Menu_LoadAndPushFrame_orig = nullptr;

char __attribute__((thiscall)) Menu_LoadAndPushFrame_hook(void* self, float a2) {
	char detail[64];
	const char* path = self ? *(const char**)((char*)self + 48) : nullptr;
	MNUDETAIL(detail, "path=%s", path ? path : "?");
	mnu_enter("Menu_LoadAndPushFrame", detail);
	const char r = Menu_LoadAndPushFrame_orig(self, a2);
	MNUDETAIL(detail, "ret=%d", (int)(unsigned char)r);
	mnu_leave("Menu_LoadAndPushFrame", detail);
	return r;
}

using Sub4E40Fn = void*(__attribute__((thiscall)) *)(void*, void*, void*, void*);
static Sub4E40Fn Sub4E40_orig = nullptr;

using TokenAdvFn = char(__attribute__((thiscall)) *)(void* file, int n);

static char native_token_advance(void* file, int n, TokenAdvFn fn) {
#if defined(__i386__)
	char ret = 0;
	const uintptr_t fp = (uintptr_t)fn;
	__asm__ volatile("push %2\n\tmovl %1, %%ecx\n\tcall *%3\n\tmovb %%al, %0" : "=m"(ret) : "r"(file), "r"(n), "r"(fp) : "ecx",
	                                                                               "memory");
	return ret;
#else
	return fn(file, n);
#endif
}

char __attribute__((thiscall)) define_tail_log_shim(void* rmf) {
	const int n = (int)InterlockedIncrement(&g_define_tail_log_n);
	TokenAdvFn fn = (TokenAdvFn)g_rmf_token_adv;
	const char r = fn ? native_token_advance(rmf, 1, fn) : 0;
	const Sof2EngineApi* e0 = sof2_engine();
	const bool loud_tail = g_past_defines_shaders ? (n <= 80 || n % 10 == 0) : (n <= 30 || n % 20 == 0);
	if (e0 && e0->Com_Printf && loud_tail)
		e0->Com_Printf("sof2bud: mnu define-tail #%d ret=%d%s\n", n, (int)(unsigned char)r,
		                g_past_defines_shaders ? " [tint/const]" : "");
	if (n <= 30 || n % 20 == 0) {
		char buf[48];
		snprintf(buf, sizeof buf, "define-tail #%d ret=%d", n, (int)(unsigned char)r);
		mnu_diag_file(buf);
	}
	if (n == 1) mnu_diag_file("define-tail ok");
	return r;
}

void* __attribute__((thiscall)) Sub4E40_hook(void* self, void* a2, void* a3, void* a4) {
	void* const map = self;
	const Sof2EngineApi* e = sof2_engine();
	void* f4 = map ? *(void**)((char*)map + 0x10) : nullptr;
	if (e && e->Com_Printf)
		e->Com_Printf("sof2bud: mnu sub_4E40 enter self=%p [4]=%p a3=%p a4=%p\n", map, f4, a3, a4);
	void* r = Sub4E40_orig(map, a2, a3, a4);
	if (e && e->Com_Printf) e->Com_Printf("sof2bud: mnu sub_4E40 leave ret=%p\n", r);
	return r;
}

extern "C" void* __cdecl menu_new_38(void) {
	void* p = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 0x38u);
	const Sof2EngineApi* e = sof2_engine();
	if (e && e->Com_Printf) e->Com_Printf("sof2bud: mnu new(0x38)=%p\n", p);
	return p;
}

extern "C" void __cdecl menus_delete_shim(void* p) {
	if (!p || (uintptr_t)p < 0x10000u) return;
	/* Never free into the defines stm copy (token text / Z_Malloc buffer). */
	if (ptr_in_rmf_stm(p) || ptr_in_rmf_stm((const char*)p - 1)) return;
	menus_native_heap_free(p);
}

extern "C" void __cdecl menu_free_38(void* p) {
	const Sof2EngineApi* e = sof2_engine();
	const BOOL ok = p ? HeapFree(GetProcessHeap(), 0, p) : TRUE;
	if (e && e->Com_Printf) e->Com_Printf("sof2bud: mnu free(0x38)=%p ok=%d\n", p, (int)ok);
}

using RmfLoadFn = void*(__attribute__((thiscall)) *)(void*, const char*, char, char*);

static void* thiscall_rmf_load(void* o, const char* p, char im, char* st, RmfLoadFn fn) {
#if defined(__i386__)
	void* ret = nullptr;
	const uintptr_t fp = (uintptr_t)fn;
	const int im32 = (unsigned char)im;
	__asm__ volatile(
		"movl %1, %%ecx\n\t"
		"push %5\n\t"
		"push %4\n\t"
		"push %3\n\t"
		"call *%2\n\t"
		"movl %%eax, %0"
		: "=m"(ret)
		: "r"(o), "r"(fp), "r"(p), "r"(im32), "r"(st)
		: "ecx", "memory");
	return ret;
#else
	return fn(o, p, im, st);
#endif
}

extern "C" void* rmf_defines_load_body(void* obj, const char* pack, int is_menu, char* str) {
	HMODULE menus = GetModuleHandleA("Menusx86.dll");
	RmfLoadFn fn = menus ? (RmfLoadFn)((uint8_t*)menus + kRvaRmfLoadFile) : nullptr;
	void* r = fn ? thiscall_rmf_load(obj, pack, (char)is_menu, str, fn) : nullptr;
	const Sof2EngineApi* e = sof2_engine();
	if (e && e->Com_Printf) {
		e->Com_Printf("sof2bud: mnu RMF_LoadFile pack=%s obj=%p ret=%p\n", pack ? pack : "?", obj, r);
		if (r) {
			const void* const* h = (const void* const*)r;
			e->Com_Printf("sof2bud: mnu defines RMF stm=%p cur=%p end=%p\n", h[0], h[1], h[2]);
			e->Com_Printf("sof2bud: mnu >> defines load shim return %p\n", r);
			note_rmf_stm_range(r);
		}
	}
	return r;
}

extern "C" void be20_after_rmf_load(void* file) {
	char buf[128];
	const uintptr_t cur = file ? ((uintptr_t*)file)[1] : 0;
	const uintptr_t end = file ? ((uintptr_t*)file)[2] : 0;
	const int loop = file && cur && end && cur < end;
	snprintf(buf, sizeof buf, "BE20 after load file=%p cur=%p end=%p loop=%d", file, (void*)cur, (void*)end, loop);
	mnu_diag_file(buf);
	const Sof2EngineApi* e = sof2_engine();
	if (e && e->Com_Printf)
		e->Com_Printf("sof2bud: mnu >> BE20 after load eax/esi=%p cur=%p end=%p loop=%d tok=%p\n", file, (void*)cur,
		                (void*)end, loop, g_rmf_token_adv);
}

#if defined(__i386__)
/* BE75 call site: ecx=obj; stack [ret][pack][is_menu][str] — native RMF_LoadFile ends with ret 12. */
extern "C" __attribute__((naked)) void rmf_defines_load_shim_entry(void) {
	__asm__(
		"push %ebp\n\t"
		"mov %esp, %ebp\n\t"
		"push 16(%ebp)\n\t"
		"push 12(%ebp)\n\t"
		"push 8(%ebp)\n\t"
		"push %ecx\n\t"
		"call _rmf_defines_load_body\n\t"
		"addl $16, %esp\n\t"
		"push %eax\n\t"
		"call _be20_after_rmf_load\n\t"
		"pop %eax\n\t"
		"pop %ebp\n\t"
		"ret $12\n");
}
#endif

static void call_rmf_teardown_sidetables(void* file) {
	HMODULE menus = GetModuleHandleA("Menusx86.dll");
	if (!menus || !file) return;
	const uintptr_t fp = (uintptr_t)((uint8_t*)menus + 0x22F80u);
#if defined(__i386__)
	__asm__ volatile("movl %0, %%ecx\n\tcall *%1" : : "r"(file), "r"(fp) : "ecx", "memory");
#else
	using Fn = void*(__thiscall*)(void*);
	((Fn)fp)(file);
#endif
}

extern "C" void __cdecl rmf_teardown_cdecl(void* file) {
	char buf[96];
	snprintf(buf, sizeof buf, "teardown file=%p", file);
	mnu_diag_file(buf);
	const Sof2EngineApi* e = sof2_engine();
	const uintptr_t* w = (const uintptr_t*)file;
	if (e && e->Com_Printf)
		e->Com_Printf(
			"sof2bud: mnu defines parse done file=%p cur=%p end=%p\n", file, w ? (void*)w[1] : nullptr,
			w ? (void*)w[2] : nullptr);
	if (file) {
		mnu_diag_file("teardown 22F80");
		call_rmf_teardown_sidetables(file);
		mnu_diag_file("teardown Menus_HeapFree");
		menus_native_heap_free(file);
		mnu_diag_file("teardown Menus_HeapFree done");
	}
	g_rmf_stm_lo = g_rmf_stm_hi = 0;
	if (e && e->Com_Printf) e->Com_Printf("sof2bud: mnu defines RMF teardown ok file=%p\n", file);
	mnu_diag_file("teardown ok");
}

bool hook_one(uint8_t* base, uint32_t rva, void* detour, void** orig, const char* name, size_t patch_len = kHookLen) {
	void* tramp = nullptr;
	if (!sof2_hook_cdecl(base + rva, detour, &tramp, patch_len)) {
		const Sof2EngineApi* e = sof2_engine();
		if (e && e->Com_Printf) e->Com_Printf("sof2bud: mnu hook failed %s\n", name);
		return false;
	}
	*orig = tramp;
	return true;
}

void Cmd_menu_dbg_f(void) {
	const Sof2EngineApi* e = sof2_engine();
	if (!e || !e->Cmd_Argc) return;
	if (e->Cmd_Argc() < 2 || !e->Cmd_Argv(1)) {
		if (e->Com_Printf) e->Com_Printf("Usage: sof2buddy_menu_dbg imports\n");
		return;
	}
	if (strcmp(e->Cmd_Argv(1), "imports") == 0) sof2_menus_debug_dump_imports();
}

}  // namespace

void sof2_menus_repin_native_calls(HMODULE menus) {
	if (!menus) return;
	uint8_t* base = (uint8_t*)menus;
	uint8_t* heapfree = base + 0xBB8Eu;
	if (heapfree[0] != 0xE8) {
		const uint8_t op = 0xE8;
		const int32_t rel = (int32_t)((base + 0x251BCu) - (heapfree + 5));
		sof2_mem_write(heapfree, &op, 1);
		sof2_mem_write(heapfree + 1, &rel, sizeof rel);
	}
	uint8_t* call_tok = base + 0xBE98u;
	if (call_tok[0] == 0xE8) {
		const int32_t rel = (int32_t)((base + 0x23180u) - (call_tok + 5));
		sof2_mem_write(call_tok + 1, &rel, sizeof rel);
	}
	uint8_t* tint_tail = base + 0xBB7Fu;
	if (tint_tail[0] == 0xE8) {
		const int32_t rel = (int32_t)((base + 0x23680u) - (tint_tail + 5));
		sof2_mem_write(tint_tail + 1, &rel, sizeof rel);
	}
	uint8_t* const_tail = base + 0xBC95u;
	if (const_tail[0] == 0xE8) {
		const int32_t rel = (int32_t)((base + 0x23680u) - (const_tail + 5));
		sof2_mem_write(const_tail + 1, &rel, sizeof rel);
	}
	uint8_t* font_tail = base + 0xBCF7u;
	if (font_tail[0] == 0xE8) {
		const int32_t rel = (int32_t)((base + 0x23680u) - (font_tail + 5));
		sof2_mem_write(font_tail + 1, &rel, sizeof rel);
	}
	uint8_t* shader_tail = base + 0xBDF7u;
	if (shader_tail[0] == 0xE8) {
		const int32_t rel = (int32_t)((base + 0x23680u) - (shader_tail + 5));
		sof2_mem_write(shader_tail + 1, &rel, sizeof rel);
	}
	uint8_t* call_ba70 = base + 0xBEACu;
	if (call_ba70[0] == 0xE8) {
		const int32_t rel = (int32_t)((base + 0xBA70u) - (call_ba70 + 5));
		sof2_mem_write(call_ba70 + 1, &rel, sizeof rel);
	}
}

extern "C" void token_advance_log_enter(void* file, int n) {
	const int i = (int)InterlockedIncrement(&g_token_advance_n);
	if (i <= 40 || i % 15 == 0) {
		char buf[80];
		snprintf(buf, sizeof buf, "TokenAdvance #%d n=%d file=%p", i, n, file);
		mnu_diag_file(buf);
	}
	const Sof2EngineApi* e = sof2_engine();
	if ((i <= 40 || i % 15 == 0) && e && e->Com_Printf) {
		const uintptr_t* w0 = (const uintptr_t*)file;
		e->Com_Printf("sof2bud: mnu TokenAdvance #%d enter n=%d cur=%p\n", i, n, w0 ? (void*)w0[1] : nullptr);
	}
}

#if defined(__i386__)
extern "C" {
void* g_rmf_token_adv = nullptr;
}
/* BE94 push n; mov ecx,esi; call — must match RMF_TokenAdvance thiscall (ret 4). */
char __attribute__((thiscall)) be98_token_gate(void* file, int n) {
	token_advance_log_enter(file, n);
	const int seq = (int)g_token_advance_n;
	TokenAdvFn fn = (TokenAdvFn)g_rmf_token_adv;
	const char r = fn ? native_token_advance(file, n, fn) : 0;
	if (seq <= 40 || seq % 15 == 0) {
		const Sof2EngineApi* e = sof2_engine();
		const uintptr_t* w = (const uintptr_t*)file;
		if (e && e->Com_Printf)
			e->Com_Printf("sof2bud: mnu TokenAdvance #%d ret=%d cur=%p end=%p\n", seq, (int)(unsigned char)r,
			                w ? (void*)w[1] : nullptr, w ? (void*)w[2] : nullptr);
	}
	return r;
}
#endif

int sof2_menus_ba70_active(void) {
	return (int)InterlockedCompareExchange(&g_ba70_depth, 0, 0) > 0;
}

void* g_ba70_native = nullptr;

extern "C" void ba70_depth_inc(void) {
	InterlockedIncrement(&g_ba70_depth);
	const int n = (int)InterlockedIncrement(&g_ba70_block_n);
	char buf[48];
	snprintf(buf, sizeof buf, "BA70 #%d enter", n);
	mnu_diag_file(buf);
	const Sof2EngineApi* e = sof2_engine();
	if (e && e->Com_Printf && (n <= 80 || (n % 8) == 0))
		e->Com_Printf("sof2bud: mnu BA70 block #%d enter\n", n);
}

extern "C" void ba70_depth_dec(void) {
	InterlockedDecrement(&g_ba70_depth);
	const int n = (int)g_ba70_block_n;
	char buf[48];
	snprintf(buf, sizeof buf, "BA70 #%d leave", n);
	mnu_diag_file(buf);
	const Sof2EngineApi* e = sof2_engine();
	if (e && e->Com_Printf && (n <= 64 || (n % 4) == 0))
		e->Com_Printf("sof2bud: mnu BA70 block #%d leave\n", n);
}

void sof2_menus_lex_progress(int lex_n, int c) {
	(void)lex_n;
	(void)c;
}

void sof2_menus_shader_progress(int shader_n) {
	if (!g_ba70_depth || (shader_n % 25 != 0 && shader_n != 1)) return;
	const Sof2EngineApi* e = sof2_engine();
	if (e && e->Com_Printf)
		e->Com_Printf("sof2bud: mnu >> BA70#%d shader reg #%d\n", (int)g_ba70_block_n, shader_n);
}

#if defined(__i386__)
extern "C" __attribute__((naked)) void ba70_invoke_shim(void) {
	/* bea9 push esi; beaa mov ecx,edi — no C on stack during BA70 (TokenAdvance uses ~8KiB alloca). */
	__asm__(
		"push %%ecx\n\t"
		"call _ba70_depth_inc\n\t"
		"pop %%ecx\n\t"
		"push 4(%%esp)\n\t"
		"call *_g_ba70_native\n\t"
		"addl $4, %%esp\n\t"
		"call _ba70_depth_dec\n\t"
		"ret\n" ::
			: "memory");
}
#else
extern "C" void ba70_invoke_shim(void) {}
#endif

void sof2_menus_trace_begin(void) { InterlockedExchange(&g_trace_imports, 1); }

void sof2_menus_trace_end(void) { InterlockedExchange(&g_trace_imports, 0); }

void sof2_menus_trace_import(int slot, const char* detail) {
	if (!g_trace_imports) return;
	const Sof2EngineApi* e = sof2_engine();
	if (!e || !e->Com_Printf) return;
	e->Com_Printf("sof2bud: mnu import[%d] %s\n", slot, detail ? detail : "?");
}

void sof2_menus_trace_step(const char* step) {
	if (!g_trace_imports || !step || !step[0]) return;
	const Sof2EngineApi* e = sof2_engine();
	if (e && e->Com_Printf) e->Com_Printf("sof2bud: mnu >> %s\n", step);
}

void sof2_menus_trace_fmt(const char* fmt, ...) {
	if (!g_trace_imports || !fmt) return;
	const Sof2EngineApi* e = sof2_engine();
	if (!e || !e->Com_Printf) return;
	char buf[320];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);
	e->Com_Printf("sof2bud: mnu >> %s\n", buf);
}

void sof2_menus_debug_dump_imports(void) {
	const Sof2EngineApi* e = sof2_engine();
	HMODULE menus = GetModuleHandleA("Menusx86.dll");
	if (!e || !e->Com_Printf || !menus) return;
	void** slots = (void**)((uint8_t*)menus + kMenusImportRva);
	static const int kWatch[] = {10, 28, 31, 32, 36, 37, 39, 40, 41, 42, 50, 61, 62, 73, 74, 79, 80, 82, 83, 85, 86, 88, 94, 96};
	for (size_t i = 0; i < sizeof kWatch / sizeof kWatch[0]; ++i) {
		const int slot = kWatch[i];
		if (slot == 39 || slot == 40)
			e->Com_Printf("sof2bud: Menusx86 import[%3d] = %d (screen)\n", slot, (int)(intptr_t)slots[slot]);
		else
			e->Com_Printf("sof2bud: Menusx86 import[%3d] = %p\n", slot, slots[slot]);
	}
}

#if defined(__i386__)
static LONG WINAPI mnu_crash_log(EXCEPTION_POINTERS* ep) {
	if (!ep || !ep->ExceptionRecord) return EXCEPTION_CONTINUE_SEARCH;
	const DWORD c = ep->ExceptionRecord->ExceptionCode;
	if (c != EXCEPTION_ACCESS_VIOLATION && c != EXCEPTION_ILLEGAL_INSTRUCTION && c != EXCEPTION_INT_DIVIDE_BY_ZERO)
		return EXCEPTION_CONTINUE_SEARCH;
	char buf[96];
	snprintf(buf, sizeof buf, "crash code=%lu at %p", (unsigned long)c, ep->ExceptionRecord->ExceptionAddress);
	mnu_diag_file(buf);
	return EXCEPTION_CONTINUE_SEARCH;
}
#endif

void sof2_menus_apply_mp_patches(HMODULE menus) {
	if (!menus) return;
	static volatile LONG done = 0;
	if (InterlockedCompareExchange(&done, 1, 0) != 0) return;
#if defined(__i386__)
	static volatile LONG veh = 0;
	if (InterlockedCompareExchange(&veh, 1, 0) == 0) AddVectoredExceptionHandler(1, mnu_crash_log);
#endif
	uint8_t* base = (uint8_t*)menus;
#if defined(__i386__)
	g_rmf_token_adv = base + kRvaRmfTokenAdvance;
	g_ba70_native = base + 0xBA70u;
#endif
	const Sof2EngineApi* e = sof2_engine();
	static uint8_t g_menus_ui_global_stub[128];
	void** ui_g = (void**)(base + kRvaMenusUiGlobal);
	if (ui_g && (!*ui_g || (uintptr_t)*ui_g < 0x10000u)) {
		memset(g_menus_ui_global_stub, 0, sizeof g_menus_ui_global_stub);
		void* p = g_menus_ui_global_stub;
		if (sof2_mem_write(ui_g, &p, sizeof p) && e && e->Com_Printf)
			e->Com_Printf("sof2bud: Menusx86 UI global stub @%p\n", p);
	}
	/* BE7E xor esi is only 2 bytes; a 5-byte patch here overwrote BE80 with jmp teardown (EB 37). */
	static const uint8_t kBe7eXor[] = {0x33, 0xF6};
	static const uint8_t kBe80Gate[] = {0x8B, 0x46, 0x04, 0x85, 0xC0};
	if (sof2_mem_write(base + 0xBE7Eu, kBe7eXor, sizeof kBe7eXor) && sof2_mem_write(base + 0xBE80u, kBe80Gate, sizeof kBe80Gate) && e &&
	    e->Com_Printf)
		e->Com_Printf("sof2bud: Menusx86 BE20 loop gate restored\n");
	/* Do not redirect MenuSystem_Init -> BE20 through sof2bud (GCC/MSVC SEH mismatch crashes after defines). */
	if (e && e->Com_Printf) e->Com_Printf("sof2bud: Menusx86 BE20 call left native (SEH)\n");
	/* Leave BE52 operator new native — RMF 0x38 object must use Menus CRT heap (pairs with teardown Menus_HeapFree). */
	if (e && e->Com_Printf) e->Com_Printf("sof2bud: Menusx86 BE52 new(0x38) left native (Menus heap)\n");
	uint8_t* call_rmf = base + kRvaBe75CallRmf;
	if (call_rmf[0] == 0xE8) {
		const int32_t rel = (int32_t)((uint8_t*)rmf_defines_load_shim_entry - (call_rmf + 5));
		if (sof2_mem_write(call_rmf + 1, &rel, sizeof rel) && e && e->Com_Printf)
			e->Com_Printf("sof2bud: Menusx86 RMF_LoadFile call shim installed\n");
	}
	/* RMF teardown: skip Z_Free on stm copy if const/tint still hold pointers into it (UAF). */
	uint8_t* zfree_call = base + kRvaRmfSkipBufFree;
	static const uint8_t kSkip[] = {0x83, 0xC4, 0x04, 0x90, 0x90, 0x90}; /* add esp,4; drop push arg, skip Z_Free */
	if (zfree_call[0] == 0xFF && zfree_call[1] == 0x15) {
		if (sof2_mem_write(zfree_call, kSkip, sizeof kSkip) && e && e->Com_Printf)
			e->Com_Printf("sof2bud: Menusx86 RMF stm-buf skip-free patch applied (iat)\n");
	} else if (zfree_call[0] == 0xE8) {
		if (sof2_mem_write(zfree_call, kSkip, sizeof kSkip) && e && e->Com_Printf)
			e->Com_Printf("sof2bud: Menusx86 RMF stm-buf skip-free patch applied (e8)\n");
	} else if (e && e->Com_Printf)
		e->Com_Printf("sof2bud: Menusx86 RMF skip-free skip (op=%02X %02X)\n", (unsigned)zfree_call[0],
		              (unsigned)zfree_call[1]);
	/* BE20 epilogue: skip 22F80+23960; cdecl rmf_teardown_cdecl(esi); fall through to BE20 exit */
	uint8_t* td = base + kRvaBe20TeardownSite;
	if (td[0] == 0x8B && td[1] == 0xCE) {
		uint8_t patch[13];
		patch[0] = 0x56;
		patch[1] = 0xE8;
		const int32_t rel = (int32_t)((uint8_t*)rmf_teardown_cdecl - (td + 6));
		memcpy(patch + 2, &rel, 4);
		patch[6] = 0x83;
		patch[7] = 0xC4;
		patch[8] = 0x04;
		memset(patch + 9, 0x90, 4);
		if (sof2_mem_write(td, patch, sizeof patch) && e && e->Com_Printf)
			e->Com_Printf("sof2bud: Menusx86 BE20 teardown cdecl patch applied\n");
	} else if (e && e->Com_Printf)
		e->Com_Printf("sof2bud: Menusx86 BE20 teardown patch skip (op=%02X %02X)\n", (unsigned)td[0], (unsigned)td[1]);
	/* Keep BA70 Menus_HeapFree @BB8E native — NOP broke tint/const CString refcount teardown. */
	if (e && e->Com_Printf) e->Com_Printf("sof2bud: Menusx86 BA70 string-free left native\n");
	/* Tint/const define-tail: native RMF_DefineTail_Parse (GCC shim after ~53 shader blocks correlated with AV). */
	uint8_t* tint_tail = base + kRvaTintCallDefineTail;
	if (tint_tail[0] == 0xE8) {
		const int32_t rel = (int32_t)((base + kRvaRmfDefineTail) - (tint_tail + 5));
		if (sof2_mem_write(tint_tail + 1, &rel, sizeof rel) && e && e->Com_Printf)
			e->Com_Printf("sof2bud: Menusx86 tint define-tail native\n");
	}
	uint8_t* const_tail = base + kRvaConstCallDefineTail;
	if (const_tail[0] == 0xE8) {
		const int32_t rel = (int32_t)((base + kRvaRmfDefineTail) - (const_tail + 5));
		if (sof2_mem_write(const_tail + 1, &rel, sizeof rel) && e && e->Com_Printf)
			e->Com_Printf("sof2bud: Menusx86 const define-tail native\n");
	}
	if (e && e->Com_Printf) e->Com_Printf("sof2bud: Menusx86 font/shader define-tail native (repin each open)\n");
	uint8_t* call_tok = base + kRvaBe98TokenAdvance;
	if (call_tok[0] == 0xE8) {
		const int32_t rel = (int32_t)((base + kRvaRmfTokenAdvance) - (call_tok + 5));
		sof2_mem_write(call_tok + 1, &rel, sizeof rel);
	}
	if (e && e->Com_Printf) e->Com_Printf("sof2bud: Menusx86 TokenAdvance -> native (SEH)\n");
	if (e && e->Com_Printf) e->Com_Printf("sof2bud: Menusx86 BA70 call left native (ecx=edi)\n");
	/* Leave sub_40023960 native — TokenAdvance string refcount must use Menus CRT free. */
	if (e && e->Com_Printf) e->Com_Printf("sof2bud: Menusx86 delete left native (refcount)\n");
}

void sof2_menus_install_debug_hooks(HMODULE menus) {
	if (!menus || InterlockedCompareExchange(&g_dbg_hooks, 1, 0) != 0) return;
	const Sof2EngineApi* eng = sof2_engine();
	/* Frame loaders use SEH — 6-byte detours crash right after defines teardown (post-#54). */
	if (eng && eng->Com_Printf) eng->Com_Printf("sof2bud: menu frame hooks off (native OpenFrame/CreateFrame)\n");
	if (eng && eng->Cmd_AddCommand) eng->Cmd_AddCommand("sof2buddy_menu_dbg", Cmd_menu_dbg_f);
}

void sof2_menus_reset_define_parse_state(void) {
	InterlockedExchange(&g_token_advance_n, 0);
	InterlockedExchange(&g_ba70_block_n, 0);
	InterlockedExchange(&g_define_tail_log_n, 0);
	InterlockedExchange(&g_past_defines_shaders, 0);
	InterlockedExchange(&g_lex_log_budget, 0);
	InterlockedExchange(&g_ba70_depth, 0);
	FILE* f = fopen("sof2bud_mnu.log", "w");
	if (f) fclose(f);
	mnu_diag_file("define parse start");
	HMODULE menus = GetModuleHandleA("Menusx86.dll");
	if (menus) sof2_menus_repin_native_calls(menus);
}

void sof2_menus_mark_past_defines_shaders(void) {
	if (InterlockedCompareExchange(&g_past_defines_shaders, 1, 0) != 0) return;
	InterlockedExchange(&g_lex_log_budget, 48);
	const Sof2EngineApi* e = sof2_engine();
	if (e && e->Com_Printf)
		e->Com_Printf("sof2bud: mnu >> tint/const phase (tok=%d ba70=%d tails=%d)\n", (int)g_token_advance_n,
		                (int)g_ba70_block_n, (int)g_define_tail_log_n);
	mnu_diag_file("tint/const phase");
}

int sof2_menus_log_lex_after_shaders(int c) {
	if (!g_past_defines_shaders || InterlockedDecrement(&g_lex_log_budget) < 0) return c;
	const Sof2EngineApi* e = sof2_engine();
	if (e && e->Com_Printf) e->Com_Printf("sof2bud: mnu lex post-shader char=%d\n", c);
	return c;
}
