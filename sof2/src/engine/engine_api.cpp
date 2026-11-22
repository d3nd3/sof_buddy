#include "engine_internal.h"
#include "sof2_api.h"
#include "game_module.h"
#include "profile_mp_103.h"
#include <windows.h>
#include <psapi.h>
#include <stdio.h>
#include <stdarg.h>

static Sof2EngineApi g_engine = {};

bool sof2_engine_bind(Sof2EngineApi* out) {
	uint8_t* base = sof2_game_base();
	if (!base || !out) return false;
	MODULEINFO mi = {};
	if (!GetModuleInformation(GetCurrentProcess(), (HMODULE)base, &mi, sizeof(mi))) return false;
	if (!sof2_mp103_matches(base)) return false;

	g_engine.profile_id = "sof2mp_1_03_gold";
	g_engine.Com_Printf = (void (*)(const char*, ...)) (base + Sof2Mp103Profile::kRvaComPrintf);
	g_engine.Cvar_Get = (cvar_t* (*)(const char*, const char*, int, cvarcommand_t)) (base + Sof2Mp103Profile::kRvaCvarGet);
	g_engine.Cvar_Set2 = (void (*)(const char*, const char*, qboolean)) (base + Sof2Mp103Profile::kRvaCvarSet2);
	g_engine.Cbuf_AddText = (void (*)(const char*)) (base + Sof2Mp103Profile::kRvaCbufAddText);
	g_engine.Cbuf_Execute = (void (*)()) (base + Sof2Mp103Profile::kRvaCbufExecute);
	g_engine.Cmd_AddCommand = (void (*)(const char*, xcommand_t)) (base + Sof2Mp103Profile::kRvaCmdAddCommand);
	g_engine.Cmd_ExecuteString = (int (*)(const char*)) (base + Sof2Mp103Profile::kRvaCmdExecuteString);
	g_engine.Cmd_Argc = (int (*)()) (base + Sof2Mp103Profile::kRvaCmdArgc);
	g_engine.Cmd_Argv = (const char* (*)(int)) (base + Sof2Mp103Profile::kRvaCmdArgv);
	*out = g_engine;
	return true;
}

const Sof2EngineApi* sof2_engine(void) {
	return g_engine.Com_Printf ? &g_engine : nullptr;
}

void sof2_engine_set_cbuf_execute(void (*fn)(void)) {
	if (fn) g_engine.Cbuf_Execute = fn;
}

bool sof2_engine_cvars_ready(void) {
	uint8_t* base = sof2_game_base();
	if (!base) return false;
	return *(uintptr_t*)(base + Sof2Mp103Profile::kRvaCvarHashHead) != 0;
}

void sof2_engine_logf(const char* fmt, ...) {
	char buf[4096];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);
	const Sof2EngineApi* e = sof2_engine();
	if (e && e->Com_Printf) e->Com_Printf("%s", buf);
}
