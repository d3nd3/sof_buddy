#pragma once

#include "sof2_types.h"
#include <stdarg.h>

struct Sof2EngineApi {
	const char* profile_id;
	void (*Com_Printf)(const char* fmt, ...);
	cvar_t* (*Cvar_Get)(const char* name, const char* value, int flags, cvarcommand_t callback);
	void (*Cvar_Set2)(const char* name, const char* value, qboolean force);
	void (*Cbuf_AddText)(const char* text);
	void (*Cbuf_Execute)(void);
	void (*Cmd_AddCommand)(const char* name, xcommand_t cmd);
	int (*Cmd_ExecuteString)(const char* text);
	int (*Cmd_Argc)(void);
	const char* (*Cmd_Argv)(int arg);
};

bool sof2_engine_bind(Sof2EngineApi* out);
const Sof2EngineApi* sof2_engine(void);
void sof2_engine_set_cbuf_execute(void (*fn)(void));
