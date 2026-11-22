#include "buddy_menu.h"
#include "detour.h"
#include "engine_internal.h"
#include "game_module.h"
#include "menus_bootstrap.h"
#include "profile_mp_103.h"
#include "sof2_api.h"
#include <stdio.h>
#include <string.h>
#include <windows.h>

extern int feature_video_modes_mode_count(void);
extern const char* feature_video_modes_mode_label(int);

namespace {

#define SOF2BUD_VERSION "0.1.0-milestone"

volatile LONG g_cmds_registered = 0;
volatile LONG g_cbuf_hooked = 0;
volatile LONG g_cl_frame_hooked = 0;
static int g_bind_tick = 0;
static bool g_f12_down = false;

using CbufExecuteFn = void (*)();
using ClFrameFn = void (__cdecl*)(int);
static CbufExecuteFn Cbuf_Execute_orig = nullptr;
static ClFrameFn CL_Frame_orig = nullptr;
static volatile LONG g_in_cbuf_hook = 0;

void exec_menu(const char* pack, const char* page) {
	if (sof2_menus_open(pack, page)) return;
	const Sof2EngineApi* e = sof2_engine();
	if (e && e->Com_Printf)
		e->Com_Printf("sof2bud: could not open menu %s/%s (Menusx86)\n", pack, page ? page : "main");
}

void open_buddy_menu(const char* page) {
	sof2_menus_ensure_ready();
	exec_menu("sof_buddy", page ? page : "main");
}

void apply_f12_bind(void) {
	const Sof2EngineApi* e = sof2_engine();
	if (!e) return;
	if (e->Cmd_ExecuteString)
		e->Cmd_ExecuteString("bind F12 \"sof2buddy_f12_menu\"");
	else if (e->Cbuf_AddText)
		e->Cbuf_AddText("bind F12 \"sof2buddy_f12_menu\"\n");
}

void tick_f12_bind(void) {
	++g_bind_tick;
	if (g_bind_tick == 1 || g_bind_tick == 30 || g_bind_tick == 120 || g_bind_tick == 600) apply_f12_bind();
}

void Cmd_sof2buddy_menu_f(void) {
	const Sof2EngineApi* e = sof2_engine();
	if (!e || !e->Cmd_Argc || !e->Cmd_Argv) return;
	const char* page = "main";
	if (e->Cmd_Argc() >= 2) {
		const char* path = e->Cmd_Argv(1);
		if (path && path[0]) {
			const char* p = path;
			if (strncmp(p, "sof_buddy/", 10) == 0)
				p += 10;
			else {
				const char* slash = strchr(p, '/');
				if (slash) p = slash + 1;
			}
			page = p;
		}
	}
	open_buddy_menu(page);
}

void Cmd_sof2buddy_f12_menu_f(void) {
	open_buddy_menu("main");
}

void Cmd_sof2buddy_refresh_modes_f(void) {
	feature_buddy_menu_refresh_mode_list();
	open_buddy_menu("texture");
}

void Cmd_sof2buddy_apply_f12_f(void) {
	apply_f12_bind();
}

void register_buddy_commands(void) {
	if (InterlockedCompareExchange(&g_cmds_registered, 1, 0) != 0) return;
	const Sof2EngineApi* e = sof2_engine();
	if (e && e->Cmd_AddCommand) {
		e->Cmd_AddCommand("sof2buddy_menu", Cmd_sof2buddy_menu_f);
		e->Cmd_AddCommand("sof2buddy_f12_menu", Cmd_sof2buddy_f12_menu_f);
		e->Cmd_AddCommand("sof2buddy_refresh_modes", Cmd_sof2buddy_refresh_modes_f);
		e->Cmd_AddCommand("sof2buddy_apply_f12", Cmd_sof2buddy_apply_f12_f);
	} else
		InterlockedExchange(&g_cmds_registered, 0);
}

void buddy_main_thread_tick(void) {
	register_buddy_commands();
	tick_f12_bind();
	const bool f12 = (GetAsyncKeyState(VK_F12) & 0x8000) != 0;
	if (f12 && !g_f12_down) {
		const Sof2EngineApi* e = sof2_engine();
		if (e && e->Cbuf_AddText) e->Cbuf_AddText("sof2buddy_f12_menu\n");
	}
	g_f12_down = f12;
}

static void Cbuf_Execute_hook(void) {
	if (InterlockedCompareExchange(&g_in_cbuf_hook, 1, 0) != 0) {
		if (Cbuf_Execute_orig) Cbuf_Execute_orig();
		return;
	}
	buddy_main_thread_tick();
	if (Cbuf_Execute_orig) Cbuf_Execute_orig();
	InterlockedExchange(&g_in_cbuf_hook, 0);
}

bool install_cbuf_hook(uint8_t* base) {
	if (InterlockedCompareExchange(&g_cbuf_hooked, 1, 0) != 0) return Cbuf_Execute_orig != nullptr;
	void* tramp = nullptr;
	if (!sof2_hook_cdecl(base + Sof2Mp103Profile::kRvaCbufExecute, (void*)Cbuf_Execute_hook, &tramp,
	                     Sof2Mp103Profile::kCbufExecuteHookLen)) {
		InterlockedExchange(&g_cbuf_hooked, 0);
		return false;
	}
	Cbuf_Execute_orig = (CbufExecuteFn)tramp;
	sof2_engine_set_cbuf_execute(Cbuf_Execute_orig);
	return true;
}

static void __cdecl CL_Frame_hook(int msec) {
	buddy_main_thread_tick();
	if (CL_Frame_orig) CL_Frame_orig(msec);
}

bool install_cl_frame_hook(uint8_t* base) {
	if (InterlockedCompareExchange(&g_cl_frame_hooked, 1, 0) != 0) return CL_Frame_orig != nullptr;
	void* tramp = nullptr;
	if (!sof2_hook_cdecl(base + Sof2Mp103Profile::kRvaClFrame, (void*)CL_Frame_hook, &tramp,
	                     Sof2Mp103Profile::kClFrameHookLen)) {
		InterlockedExchange(&g_cl_frame_hooked, 0);
		return false;
	}
	CL_Frame_orig = (ClFrameFn)tramp;
	return true;
}

}  // namespace

void feature_buddy_menu_refresh_mode_list(void) {
	const Sof2EngineApi* e = sof2_engine();
	if (!e || !e->Cvar_Set2 || !e->Cvar_Get) return;
	const int n = feature_video_modes_mode_count();
	if (n <= 0) return;

	char labels[1800];
	char values[512];
	char* lp = labels;
	char* vp = values;
	const char* lend = labels + sizeof labels - 1;
	const char* vend = values + sizeof values - 1;
	*labels = *values = '\0';

	for (int i = 0; i < n; ++i) {
		const char* lab = feature_video_modes_mode_label(i);
		if (!lab) continue;
		char short_lab[48];
		const char* colon = strstr(lab, ": ");
		snprintf(short_lab, sizeof short_lab, "%s", colon ? colon + 2 : lab);
		for (char* p = short_lab; *p; ++p)
			if (*p == '"') *p = '\'';
		char num[8];
		snprintf(num, sizeof num, "%d", i);
		if (lp > labels) {
			if (lp + 1 >= lend) break;
			*lp++ = ',';
		}
		size_t ll = strlen(short_lab);
		if (lp + ll >= lend) break;
		memcpy(lp, short_lab, ll + 1);
		lp += ll;
		if (vp > values) {
			if (vp + 1 >= vend) break;
			*vp++ = ',';
		}
		size_t vl = strlen(num);
		if (vp + vl >= vend) break;
		memcpy(vp, num, vl + 1);
		vp += vl;
	}

	char rmf[2200];
	snprintf(rmf, sizeof rmf,
	         "<list \"%s\" match \"%s\" cvar r_mode atext \"Resolution : \" "
	         "key mouse1 \"vid_restart\" key mouse2 \"vid_restart\" noshade "
	         "tip \"r_mode index (sof2bud extended list). Use Vid Restart.\"><br>",
	         labels, values);
	e->Cvar_Set2("_sof2bud_mode_list_rmf", rmf, true);
	e->Cvar_Get("r_mode", "-1", CVAR_ARCHIVE, nullptr);
	e->Cvar_Get("r_fullscreen", "1", CVAR_ARCHIVE, nullptr);
}

void feature_buddy_menu_post_cvar(void) {
	const Sof2EngineApi* e = sof2_engine();
	if (!e || !e->Cvar_Get) return;
	register_buddy_commands();
	uint8_t* base = sof2_game_base();
	if (base && sof2_mp103_matches(base)) {
		const bool cbuf = install_cbuf_hook(base);
		const bool cl = install_cl_frame_hook(base);
		if (e->Com_Printf)
			e->Com_Printf("sof2bud: hooks cbuf=%d cl_frame=%d\n", cbuf ? 1 : 0, cl ? 1 : 0);
	}
	if (e->Cbuf_AddText) e->Cbuf_AddText("bind F12 \"sof2buddy_f12_menu\"\n");
	e->Cvar_Get("_sof2bud_version", SOF2BUD_VERSION, CVAR_ARCHIVE | CVAR_NOSET, nullptr);
	e->Cvar_Get("_sof2bud_build", SOF2BUD_VERSION, CVAR_ARCHIVE, nullptr);
	e->Cvar_Get("_sof2bud_mode_list_rmf",
	            "<text \"(mode list loading…)\" noshade><br>", 0, nullptr);
	feature_buddy_menu_refresh_mode_list();
}
