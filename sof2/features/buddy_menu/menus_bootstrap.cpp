#include "menus_bootstrap.h"
#include "menus_debug.h"
#include "engine_internal.h"
#include "game_module.h"
#include "profile_mp_103.h"
#include "sof2_api.h"
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

namespace {

constexpr uint32_t kMenusImportRva = 0x375D8u;
constexpr uint32_t kMenusExportRva = 0x37590u;
constexpr int kMenuApiVersion = 2;
constexpr size_t kMenuImportSlots = 0x198u / sizeof(void*);
constexpr size_t kRefExportBytes = 0xA4u;

using GetUiApiFn = int* (__cdecl*)(int version, const void* imports);

static volatile LONG g_menus_init_done = 0;
static volatile LONG g_menus_ok = 0;
static volatile LONG g_menu_cmds_registered = 0;
static void** g_menu_exports = nullptr;
static uint8_t* g_game_base = nullptr;
static uint8_t g_ui_lang_stub[128];
static int g_menu_screen_w = 640;
static int g_menu_screen_h = 480;
/* Menusx86 treats import[42] as &cls.state; value 8 triggers import[73] snd gate (SP snd ref). */
static int g_menu_cls_gate = 1;

void refresh_menu_screen_dims(uint8_t* game) {
	if (!game) return;
	using P = Sof2Mp103Profile;
	const int w = *(const int*)(game + P::kRvaGlConfigVidWidth);
	const int h = *(const int*)(game + P::kRvaGlConfigVidHeight);
	if (w > 0) g_menu_screen_w = w;
	if (h > 0) g_menu_screen_h = h;
}

void* game_fn(uint8_t* game, uint32_t rva) {
	return game && rva ? (void*)(game + rva) : nullptr;
}

void __cdecl thunk_menu_com_printf(const char* fmt, ...) {
	const Sof2EngineApi* e = sof2_engine();
	if (!e || !e->Com_Printf || !fmt) return;
	char buf[4096];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);
	if (strstr(buf, "menu_system initialised")) e->Com_Printf("sof2bud: mnu defines load complete\n");
	if (strstr(buf, "Creating menu")) e->Com_Printf("sof2bud: mnu Menu_Create starting (engine log)\n");
	e->Com_Printf("%s", buf);
}

int __cdecl thunk_sys_milliseconds(int clk) {
	if (!g_game_base) return 0;
	using Fn = int(__cdecl*)(int);
	const int ms = ((Fn)(g_game_base + Sof2Mp103Profile::kRvaSysMilliseconds))(clk);
	const Sof2EngineApi* e = sof2_engine();
	if (e && e->Com_Printf && clk == 0)
		e->Com_Printf("sof2bud: mnu Sys_Milliseconds(%d)=%d\n", clk, ms);
	return ms;
}

bool ref_exports_ready(uint8_t* game) {
	if (!game) return false;
	const uintptr_t first = *(const uintptr_t*)(game + Sof2Mp103Profile::kRvaRefGlExports);
	return first > 0x10000u && first < 0x7FFF0000u;
}

bool menus_imports_installed(uint8_t* menus) {
	return menus && *(void**)(menus + kMenusImportRva) != nullptr;
}

char* __cdecl thunk_cvar_get3(char* name, char* def, int flags) {
	using Fn = char*(__cdecl*)(char*, char*, int, int, int);
	if (!g_game_base) return nullptr;
	sof2_menus_trace_import(28, name ? name : "?");
	const bool last = name && !strcmp(name, "m_cursor_accel");
	if (last) sof2_menus_trace_step("before Cvar_Get m_cursor_accel");
	char* r = ((Fn)(g_game_base + Sof2Mp103Profile::kRvaCvarGet))(name, def, flags, 0, 0);
	if (last) sof2_menus_trace_step("after Cvar_Get m_cursor_accel");
	return r;
}

char __cdecl thunk_menu_cvar_gate(char on) {
	sof2_menus_trace_import(35, "menu_cvar_gate");
	return on;
}

/* Engine Com_sprintf uses a ~32KB stack buffer — avoid from Menusx86 (tiny frames / SEH). */
int __cdecl thunk_menu_sprintf(char* dst, int size, const char* fmt, ...) {
	if (!dst || size <= 0 || !fmt) return 0;
	va_list ap;
	va_start(ap, fmt);
	int n = -1;
	if (!strcmp(fmt, "menus/%s.rmf")) {
		const char* a = va_arg(ap, const char*);
		n = snprintf(dst, (size_t)size, fmt, a ? a : "");
	} else if (!strcmp(fmt, "huds/%s.hud")) {
		const char* a = va_arg(ap, const char*);
		n = snprintf(dst, (size_t)size, fmt, a ? a : "");
	} else if (!strcmp(fmt, "%s/%s")) {
		const char* a = va_arg(ap, const char*);
		const char* b = va_arg(ap, const char*);
		n = snprintf(dst, (size_t)size, fmt, a ? a : "", b ? b : "");
	}
	va_end(ap);
	const Sof2EngineApi* e = sof2_engine();
	if (n >= 0) {
		if (e && e->Com_Printf) e->Com_Printf("sof2bud: menu sprintf lite -> %s\n", dst);
		return n;
	}
	char tmp[512];
	va_start(ap, fmt);
	n = vsnprintf(tmp, sizeof tmp, fmt, ap);
	va_end(ap);
	if (n < 0) return 0;
	strncpy(dst, tmp, (size_t)size - 1);
	dst[size - 1] = '\0';
	if (e && e->Com_Printf) e->Com_Printf("sof2bud: menu sprintf vsn -> %s\n", dst);
	return (int)strlen(dst);
}

/* import[50]: va-list format -> static buffer (Menus passes fmt first, uses eax as char*). */
char* __cdecl thunk_menu_va(const char* fmt, ...) {
	static char buf[1024];
	if (!fmt) {
		buf[0] = '\0';
		return buf;
	}
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);
	const Sof2EngineApi* e = sof2_engine();
	static volatile LONG va_n = 0;
	const int vn = (int)InterlockedIncrement(&va_n);
	if (e && e->Com_Printf && (vn <= 24 || vn % 40 == 0)) e->Com_Printf("sof2bud: menu va -> %s\n", buf);
	return buf;
}

void __cdecl thunk_void_noop(void) {}

int __cdecl thunk_int0_char(char) { return 0; }

bool __cdecl thunk_menu_not_busy(void) { return false; }

int __cdecl thunk_ref_skip(void*, int) { return 0; }

char __cdecl thunk_menu_snd_gate(void) {
	sof2_menus_trace_import(73, "snd_gate");
	return 0;
}

void refresh_menu_cls_gate(uint8_t* game) {
	if (!game) return;
	const int s = *(const int*)(game + Sof2Mp103Profile::kRvaClsState);
	/* MP: avoid cls==8 path that calls import[73] until snd ref is wired like SP. */
	g_menu_cls_gate = (s == 8) ? 1 : s;
	sof2_menus_trace_step("cls_gate refreshed");
}

bool ptr_looks_readable(const void* p) {
	const uintptr_t u = (uintptr_t)p;
	return u >= 0x10000u && u < 0x7FFF0000u;
}

bool looks_like_cvar_name(const char* s) {
	if (!ptr_looks_readable(s)) return false;
	unsigned char c = (unsigned char)s[0];
	if (c < 'a' || c > 'z') return false;
	for (int i = 0; i < 64 && s[i]; ++i) {
		c = (unsigned char)s[i];
		if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
	}
	return true;
}

bool arg0_pb_looks_like_immediate_float(uint32_t pb) {
	if (pb < 0x10000u) return true;
	const uint8_t hi = (uint8_t)(pb >> 24);
	/* Menusx86 passes small floats as the fake "void*" (e.g. 0x3F800000 = 1.0f). */
	return hi == 0x3F || hi == 0x3E || hi == 0x40;
}

bool arg0_looks_like_float_bits(void* arg0, float val) {
	uint32_t vb = 0;
	memcpy(&vb, &val, sizeof vb);
	const uint32_t pb = (uint32_t)(uintptr_t)arg0;
	return arg0_pb_looks_like_immediate_float(pb) || pb == vb;
}

char* cvar_name_from_menus_arg(void* arg0, float val, int* path_out) {
	if (path_out) *path_out = 0;
	if (!arg0 || arg0_looks_like_float_bits(arg0, val)) return nullptr;
	if (!ptr_looks_readable(arg0)) return nullptr;
	char* direct = (char*)arg0;
	if (looks_like_cvar_name(direct)) {
		if (path_out) *path_out = 1;
		return direct;
	}
	if (ptr_looks_readable(arg0)) {
		char* from_struct = *(char**)arg0;
		if (looks_like_cvar_name(from_struct)) {
			if (path_out) *path_out = 2;
			return from_struct;
		}
	}
	if (path_out) *path_out = -1;
	return nullptr;
}

void __cdecl thunk_cvar_set_float3(void* arg0, float val, int flags) {
	sof2_menus_trace_step("Cvar_SetFloat thunk entry");
	uint32_t pb = (uint32_t)(uintptr_t)arg0, vb = 0;
	memcpy(&vb, &val, sizeof vb);
	sof2_menus_trace_fmt("setfloat arg0=%p pb=0x%08X val=%g vb=0x%08X flags=%d", arg0, pb, (double)val, vb, flags);
	if (!g_game_base) {
		sof2_menus_trace_step("setfloat abort no game_base");
		return;
	}
	int name_path = 0;
	char* name = cvar_name_from_menus_arg(arg0, val, &name_path);
	sof2_menus_trace_fmt("setfloat name_path=%d fake=%d", name_path, arg0_looks_like_float_bits(arg0, val) ? 1 : 0);
	/* Menusx86: menu_allowSave uses (float bits as fake "ptr", float val) — not cvar_t* */
	if (!name && arg0_looks_like_float_bits(arg0, val)) {
		name = (char*)"menu_allowSave";
		sof2_menus_trace_step("menu_allowSave float-arg fixup");
	}
	sof2_menus_trace_import(31, name ? name : "?");
	if (name && !strcmp(name, "menu_allowSave")) {
		sof2_menus_trace_step("menu_allowSave via Cvar_Set");
		const char* s = (val != 0.f) ? "1" : "0";
		sof2_menus_trace_fmt("menu_allowSave Cvar_Set -> %s", s);
		using Fn = char*(__cdecl*)(char*, char*);
		((Fn)(g_game_base + Sof2Mp103Profile::kRvaCvarSet))(name, (char*)s);
		sof2_menus_trace_step("menu_allowSave done");
		return;
	}
	if (!name) sof2_menus_trace_step("setfloat no name — calling engine with arg0 as cvar*");
	sof2_menus_trace_step("before Cvar_SetFloat");
	using Fn = char*(__cdecl*)(char*, float);
	if (name)
		((Fn)(g_game_base + Sof2Mp103Profile::kRvaCvarSetFloat))(name, val);
	else
		((Fn)(g_game_base + Sof2Mp103Profile::kRvaCvarSetFloat))((char*)arg0, val);
	sof2_menus_trace_step("after Cvar_SetFloat");
}

char* __cdecl thunk_cvar_set3(char* name, char* val, int) {
	if (!g_game_base) return nullptr;
	sof2_menus_trace_import(29, name ? name : "?");
	sof2_menus_trace_step("before Cvar_Set");
	using Fn = char*(__cdecl*)(char*, char*);
	char* r = ((Fn)(g_game_base + Sof2Mp103Profile::kRvaCvarSet))(name, val);
	sof2_menus_trace_step("after Cvar_Set");
	if (name && !strcmp(name, "cl_paused")) {
		sof2_menus_trace_step("next: key_catchers|=2 then menus/defines.rmf");
		HMODULE menus = GetModuleHandleA("Menusx86.dll");
		void** live = menus ? (void**)((uint8_t*)menus + kMenusImportRva) : nullptr;
		const int kc = g_game_base ? *(int*)(g_game_base + Sof2Mp103Profile::kRvaKeyCatchers) : -1;
		sof2_menus_trace_fmt("live import[41]=%p [61]=%p keyCatchers=%d", live ? live[41] : nullptr,
		                     live ? live[61] : nullptr, kc);
	}
	return r;
}

int __cdecl thunk_menu_font_reg(const char* name, int a, int b, int c) {
	static volatile LONG font_n = 0;
	const int fn = (int)InterlockedIncrement(&font_n);
	const Sof2EngineApi* e = sof2_engine();
	if (e && e->Com_Printf) e->Com_Printf("sof2bud: mnu defines font #%d %s\n", fn, name ? name : "?");
	return 1;
}

int __cdecl thunk_menu_shader_reg(const char* name, int flags) {
	static volatile LONG sh_n = 0;
	const int sn = (int)InterlockedIncrement(&sh_n);
	const Sof2EngineApi* e = sof2_engine();
	if (e && e->Com_Printf && (sn <= 24 || sn % 40 == 0))
		e->Com_Printf("sof2bud: mnu defines shader %s flags=%d\n", name ? name : "?", flags);
	sof2_menus_shader_progress(sn);
	if (name && !strcmp(name, "gfx/menus/misc/scroll_arrow")) {
		if (e && e->Com_Printf) e->Com_Printf("sof2bud: mnu defines shaders done (tint/const next)\n");
		sof2_menus_mark_past_defines_shaders();
	}
	return 1;
}

void* __cdecl thunk_menu_z_malloc(unsigned size, int tag, int zero) {
	const unsigned n = size ? size : 1u;
	void* p = malloc(n);
	if (p && zero) memset(p, 0, n);
	const Sof2EngineApi* e = sof2_engine();
	if (e && e->Com_Printf) e->Com_Printf("sof2bud: mnu Z_Malloc %u tag=%d -> %p\n", n, tag, p);
	return p;
}

void __cdecl thunk_menu_z_free(void* p) {
	if (!p) return;
	free(p);
}

void __cdecl thunk_menu_fs_free(void* p) {
	if (p) {
		const Sof2EngineApi* e = sof2_engine();
		if (e && e->Com_Printf) e->Com_Printf("sof2bud: mnu FS_FreeFile %p\n", p);
	}
	using Fn = void(__cdecl*)(void*);
	if (g_game_base) ((Fn)(g_game_base + Sof2Mp103Profile::kRvaFSFreeFile))(p);
}

char __cdecl thunk_menu_slot85(void) {
	sof2_menus_trace_step("import[85]");
	return 0;
}

/* import[84]: RMF_TokenAdvance lexer — advance *cursor, return byte */
int __cdecl thunk_rmf_lex_char(const char** cursor, char* side) {
	if (!cursor || !*cursor) return 0;
	const unsigned char c = (unsigned char)**cursor;
	if (side) *side = (char)c;
	++(*cursor);
	static volatile LONG lex_n = 0;
	const int n = (int)InterlockedIncrement(&lex_n);
	const int in_ba70 = sof2_menus_ba70_active();
	if (!in_ba70 && (n <= 48 || n % 256 == 0)) {
		const Sof2EngineApi* e = sof2_engine();
		if (e && e->Com_Printf) e->Com_Printf("sof2bud: mnu lex #%d byte=%d\n", n, (int)c);
	}
	if (!in_ba70) sof2_menus_lex_progress(n, (int)c);
	if (!in_ba70 && n == 10000) {
		const Sof2EngineApi* e = sof2_engine();
		if (e && e->Com_Printf) e->Com_Printf("sof2bud: mnu lex budget 10000 (spin?)\n");
	}
	if (!in_ba70) sof2_menus_log_lex_after_shaders((int)c);
	return (int)c;
}

/* import[64]: &name expansion in defines.rmf (identity until full const table wired) */
char* __cdecl thunk_rmf_define_lookup(char* key) {
	static char buf[384];
	const Sof2EngineApi* e = sof2_engine();
	if (!key) {
		buf[0] = '\0';
		return buf;
	}
	strncpy(buf, key, sizeof buf - 1);
	buf[sizeof buf - 1] = '\0';
	static volatile LONG lk = 0;
	const int ln = (int)InterlockedIncrement(&lk);
	if (e && e->Com_Printf && (ln <= 16 || ln % 40 == 0)) e->Com_Printf("sof2bud: mnu define_lookup #%d %s\n", ln, buf);
	return buf;
}

int __cdecl thunk_menu_pack_read(char* path, char** out) {
	const Sof2EngineApi* eng = sof2_engine();
	if (eng && eng->Com_Printf) eng->Com_Printf("sof2bud: pack_read %s\n", path ? path : "?");
	sof2_menus_trace_step("pack_read entry");
	if (!g_game_base || !path) {
		sof2_menus_trace_step("pack_read fail (no base/path)");
		return 0;
	}
	sof2_menus_trace_import(61, path);
	if (out) *out = nullptr;
	using Fn = int(__cdecl*)(char*, void**);
	void* buf = nullptr;
	if (eng && eng->Com_Printf) eng->Com_Printf("sof2bud: pack_read FS_ReadFile...\n");
	const int len = ((Fn)(g_game_base + Sof2Mp103Profile::kRvaFSReadFile))(path, &buf);
	if (eng && eng->Com_Printf) eng->Com_Printf("sof2bud: pack_read FS done len=%d buf=%p\n", len, buf);
	if (len > 0 && buf && out) {
		*out = (char*)buf;
		if (eng && eng->Com_Printf) {
			const char* s = (const char*)buf;
			eng->Com_Printf("sof2bud: pack_read stm=%p end=%p\n", (void*)strstr(s, "<stm"), (void*)strstr(s, "</stm>"));
		}
		sof2_menus_trace_fmt("pack_read ok len=%d buf=%p", len, buf);
		return len;
	}
	sof2_menus_trace_fmt("pack_read miss len=%d path=%s", len, path);
	return 0;
}

void fill_ref_imports(uint8_t* game, void** slots, size_t nslots) {
	if (!game || !ref_exports_ready(game)) return;
	const uint8_t* rip = game + Sof2Mp103Profile::kRvaRefGlExports;
	/* ref_gl GetRefAPI table — do not map +0x88/+0x14 onto Menus slots 86/94 (font/shader). */
	static const struct {
		size_t slot;
		uint32_t off;
	} kMap[] = {{88, 144}, {89, 148}, {90, 88}, {91, 92}, {92, 96}, {95, 152}, {97, 160}};
	for (const auto& m : kMap) {
		if (m.slot >= nslots || m.off + sizeof(uintptr_t) > kRefExportBytes) continue;
		const uintptr_t p = *(const uintptr_t*)(rip + m.off);
		if (p > 0x10000u && p < 0x7FFF0000u) slots[m.slot] = (void*)p;
	}
}

void fill_snd_ref_stubs(void** slots, size_t nslots) {
	if (74 < nslots && !slots[74]) slots[74] = (void*)thunk_menu_not_busy;
	if (79 < nslots && !slots[79]) slots[79] = (void*)thunk_ref_skip;
	const size_t void_slots[] = {72, 78, 80, 81, 82, 83};
	for (size_t i = 0; i < 6; ++i)
		if (void_slots[i] < nslots && !slots[void_slots[i]]) slots[void_slots[i]] = (void*)thunk_void_noop;
}

void fill_defines_parser_stubs(void** slots, size_t nslots) {
	/* defines.rmf: slots 86/94 are Menus-specific; ref_gl offsets are not 1:1 with import indices. */
	if (86 < nslots) slots[86] = (void*)thunk_menu_font_reg;
	if (94 < nslots) slots[94] = (void*)thunk_menu_shader_reg;
	if (96 < nslots) slots[96] = g_ui_lang_stub;
}

void fill_menu_imports(uint8_t* game, void** slots, size_t nslots) {
	memset(slots, 0, nslots * sizeof(void*));
	g_game_base = game;
	auto set = [&](size_t slot, uint32_t rva) {
		if (slot < nslots) slots[slot] = game_fn(game, rva);
	};
	using P = Sof2Mp103Profile;
	if (0 < nslots) slots[0] = (void*)thunk_menu_com_printf;
	if (1 < nslots) slots[1] = (void*)thunk_menu_com_printf;
	set(2, P::kRvaComDPrintf);
	if (3 < nslots) slots[3] = (void*)thunk_menu_sprintf;
	if (50 < nslots) slots[50] = (void*)thunk_menu_va;
	set(4, P::kRvaComError);
	set(10, P::kRvaComError);
	if (16 < nslots) slots[16] = (void*)thunk_sys_milliseconds;
	set(21, P::kRvaCbufAddText);
	set(22, P::kRvaCbufExecute);
	set(23, P::kRvaCmdAddCommand);
	set(24, P::kRvaCmdRemoveCommand);
	set(25, P::kRvaCmdArgc);
	set(26, P::kRvaCmdArgvBuffer);
	set(27, P::kRvaCmdArgs);
	if (28 < nslots) slots[28] = (void*)thunk_cvar_get3;
	if (29 < nslots) slots[29] = (void*)thunk_cvar_set3;
	if (30 < nslots) slots[30] = (void*)thunk_cvar_get3;
	if (32 < nslots) slots[32] = (void*)thunk_cvar_get3;
	if (31 < nslots) slots[31] = (void*)thunk_cvar_set_float3;
	if (35 < nslots) slots[35] = (void*)thunk_menu_cvar_gate;
	if (36 < nslots) slots[36] = (void*)thunk_menu_z_malloc;
	if (37 < nslots) slots[37] = (void*)thunk_menu_z_free;
	if (38 < nslots) slots[38] = (void*)thunk_menu_z_free;
	refresh_menu_screen_dims(game);
	if (39 < nslots) slots[39] = (void*)(intptr_t)g_menu_screen_w;
	if (40 < nslots) slots[40] = (void*)(intptr_t)g_menu_screen_h;
	if (41 < nslots) slots[41] = game + P::kRvaKeyCatchers;
	refresh_menu_cls_gate(game);
	if (42 < nslots) slots[42] = &g_menu_cls_gate;
	if (43 < nslots) slots[43] = game + P::kRvaKeyState;
	if (61 < nslots) slots[61] = (void*)thunk_menu_pack_read;
	if (62 < nslots) slots[62] = (void*)thunk_menu_fs_free;
	if (64 < nslots) slots[64] = (void*)thunk_rmf_define_lookup;
	if (84 < nslots) slots[84] = (void*)thunk_rmf_lex_char;
	if (73 < nslots) slots[73] = (void*)thunk_menu_snd_gate;
	if (77 < nslots && !slots[77]) slots[77] = (void*)thunk_int0_char;
	if (93 < nslots) slots[93] = (void*)thunk_void_noop;
	fill_ref_imports(game, slots, nslots);
	fill_snd_ref_stubs(slots, nslots);
	fill_defines_parser_stubs(slots, nslots);
	if (85 < nslots) slots[85] = (void*)thunk_menu_slot85;
}

void Cmd_menu_native_f(void) {
	const Sof2EngineApi* e = sof2_engine();
	if (!e || !e->Cmd_Argc || !e->Cmd_Argv) return;
	const char* pack = e->Cmd_Argc() > 1 ? e->Cmd_Argv(1) : "";
	const char* page = e->Cmd_Argc() > 2 ? e->Cmd_Argv(2) : "main";
	if (!pack[0]) {
		if (e->Com_Printf) e->Com_Printf("Usage: menu <name> [frame]\n");
		return;
	}
	sof2_menus_open(pack, page);
}

void Cmd_popmenu_native_f(void) {
	if (!g_menu_exports) return;
	using Fn = void (*)();
	Fn pop = (Fn)g_menu_exports[6];
	if (pop) pop();
}

void register_menu_console_cmds(void) {
	if (InterlockedCompareExchange(&g_menu_cmds_registered, 1, 0) != 0) return;
	const Sof2EngineApi* e = sof2_engine();
	if (!e || !e->Cmd_AddCommand) {
		InterlockedExchange(&g_menu_cmds_registered, 0);
		return;
	}
	e->Cmd_AddCommand("menu", Cmd_menu_native_f);
	e->Cmd_AddCommand("popmenu", Cmd_popmenu_native_f);
}

bool push_menu_imports(uint8_t* game, HMODULE menus_mod) {
	if (!game || !menus_mod) return false;
	uint8_t* menus = (uint8_t*)menus_mod;
	refresh_menu_screen_dims(game);
	refresh_menu_cls_gate(game);
	void* imports[kMenuImportSlots] = {};
	fill_menu_imports(game, imports, kMenuImportSlots);
	auto get_ui = (GetUiApiFn)GetProcAddress(menus_mod, "GetUIAPI");
	if (!get_ui) get_ui = (GetUiApiFn)(menus + 0x23830u);
	g_menu_exports = get_ui ? (void**)get_ui(kMenuApiVersion, imports) : nullptr;
	void** live = (void**)(menus + kMenusImportRva);
	if (live) {
		live[0] = imports[0];
		live[1] = imports[1];
		live[16] = imports[16];
		live[41] = game + Sof2Mp103Profile::kRvaKeyCatchers;
		live[42] = &g_menu_cls_gate;
		live[3] = imports[3];
		live[36] = imports[36];
		live[37] = imports[37];
		live[38] = imports[38];
		live[50] = imports[50];
		live[61] = imports[61];
		live[62] = imports[62];
		live[64] = imports[64];
		live[84] = imports[84];
		live[86] = imports[86];
		live[88] = imports[88];
		live[94] = imports[94];
		live[95] = imports[95];
	}
	const Sof2EngineApi* e = sof2_engine();
	if (e && e->Com_Printf)
		e->Com_Printf("sof2bud: mnu push imports cls@%p=%d slot86=%p slot94=%p slot61=%p\n", &g_menu_cls_gate,
		              g_menu_cls_gate, imports[86], imports[94], imports[61]);
	sof2_menus_repin_native_calls(menus_mod);
	return g_menu_exports && menus_imports_installed(menus);
}

bool bootstrap_menus(uint8_t* game) {
	if (!game || !sof2_mp103_matches(game) || !ref_exports_ready(game)) return false;
	HMODULE menus_mod = GetModuleHandleA("Menusx86.dll");
	if (!menus_mod) menus_mod = LoadLibraryA("Menusx86.dll");
	if (!menus_mod) return false;
	if (!push_menu_imports(game, menus_mod)) return false;
	sof2_menus_apply_mp_patches(menus_mod);
	register_menu_console_cmds();
	sof2_menus_install_debug_hooks(menus_mod);
	return g_menu_exports != nullptr;
}

}  // namespace

bool sof2_menus_ensure_ready(void) {
	if (InterlockedCompareExchange(&g_menus_ok, 1, 1) == 1) return true;
	if (InterlockedCompareExchange(&g_menus_init_done, 1, 0) != 0) return InterlockedCompareExchange(&g_menus_ok, 1, 1) == 1;
	uint8_t* game = sof2_game_base();
	if (!game || !bootstrap_menus(game)) {
		const Sof2EngineApi* e = sof2_engine();
		if (e && e->Com_Printf) e->Com_Printf("sof2bud: Menusx86 init failed (need renderer)\n");
		return false;
	}
	InterlockedExchange(&g_menus_ok, 1);
	const Sof2EngineApi* e = sof2_engine();
	if (e && e->Com_Printf) e->Com_Printf("sof2bud: Menusx86 ready\n");
	return true;
}

bool sof2_menus_open(const char* pack, const char* page) {
	if (!pack || !page || !sof2_menus_ensure_ready() || !g_menu_exports) return false;
	static volatile LONG menu_create_busy = 0;
	if (InterlockedCompareExchange(&menu_create_busy, 1, 0) != 0) {
		const Sof2EngineApi* e = sof2_engine();
		if (e && e->Com_Printf) e->Com_Printf("sof2bud: Menu_Create skipped (already running)\n");
		return false;
	}
	sof2_menus_reset_define_parse_state();
	using MenuCreateFn = int(__cdecl*)(char*, char*, char);
	MenuCreateFn create = (MenuCreateFn)g_menu_exports[4];
	if (!create) return false;
	char p[64], f[64];
	strncpy(p, pack, sizeof p - 1);
	p[sizeof p - 1] = '\0';
	strncpy(f, page, sizeof f - 1);
	f[sizeof f - 1] = '\0';
	uint8_t* game = sof2_game_base();
	HMODULE menus_mod = GetModuleHandleA("Menusx86.dll");
	if (game && menus_mod) push_menu_imports(game, menus_mod);
	const Sof2EngineApi* eng = sof2_engine();
	if (eng && eng->Com_Printf)
		eng->Com_Printf("sof2bud: Menu_Create %s / %s screen=%dx%d\n", p, f, g_menu_screen_w, g_menu_screen_h);
	sof2_menus_trace_begin();
	sof2_menus_trace_step("calling Menu_Create export");
	const int rc = create(p, f, 0);
	sof2_menus_trace_step("returned from Menu_Create export");
	sof2_menus_trace_end();
	InterlockedExchange(&menu_create_busy, 0);
	if (eng && eng->Com_Printf) eng->Com_Printf("sof2bud: Menu_Create done rc=%d\n", rc);
	return rc != 0;
}
