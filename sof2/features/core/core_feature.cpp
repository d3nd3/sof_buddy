#include "sof2_api.h"
#include <windows.h>

#define SOF2BUD_VERSION "0.1.0-milestone"
static const char kRateValue[] = "20000";

static volatile LONG g_rate_applied = 0;

static void apply_rate_20000(const Sof2EngineApi* e) {
	if (InterlockedCompareExchange(&g_rate_applied, 1, 0) != 0) return;
	if (e->Cbuf_AddText && e->Cbuf_Execute) {
		e->Cbuf_AddText("set rate 20000\n");
		e->Cbuf_Execute();
	}
	if (e->Cvar_Set2) e->Cvar_Set2("rate", kRateValue, 1);
}

void feature_core_post_cvar(void) {
	const Sof2EngineApi* e = sof2_engine();
	if (!e || !e->Cvar_Get || !e->Com_Printf) return;
	e->Cvar_Get("_sof2bud_version", SOF2BUD_VERSION, CVAR_ARCHIVE | CVAR_NOSET, nullptr);
	apply_rate_20000(e);
	e->Com_Printf("sof2bud %s loaded (%s)\n", SOF2BUD_VERSION, e->profile_id);
}
