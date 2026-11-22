#include "feature.h"
#include "engine_internal.h"
#include "game_module.h"
#include "sof2_api.h"
#include <windows.h>

static volatile LONG g_early_done = 0;
static volatile LONG g_post_cvar_done = 0;
static volatile LONG g_init_thread = 0;

static void run_early_once(void) {
	if (InterlockedCompareExchange(&g_early_done, 1, 0) != 0) return;
	Sof2EngineApi api = {};
	sof2_engine_bind(&api);
	sof2_features_early_startup();
}

static void run_post_cvar_once(void) {
	if (InterlockedCompareExchange(&g_post_cvar_done, 1, 0) != 0) return;
	if (!sof2_engine()) {
		Sof2EngineApi api = {};
		sof2_engine_bind(&api);
	}
	sof2_features_post_cvar();
}

static DWORD WINAPI init_thread(LPVOID) {
	for (int i = 0; i < 240; ++i) {
		uint8_t* base = sof2_game_base();
		if (base && sof2_mp103_matches(base)) break;
		Sleep(50);
	}
	run_early_once();
	for (int i = 0; i < 600; ++i) {
		if (g_post_cvar_done) return 0;
		if (sof2_engine_cvars_ready()) {
			run_post_cvar_once();
			return 0;
		}
		Sleep(50);
	}
	return 0;
}

static void ensure_init_thread(void) {
	if (InterlockedCompareExchange(&g_init_thread, 1, 0) != 0) return;
	if (!CreateThread(nullptr, 0, init_thread, nullptr, 0, nullptr))
		InterlockedExchange(&g_init_thread, 0);
}

void sof2_lifecycle_start(void) {
	ensure_init_thread();
}

void sof2_lifecycle_on_wsock_ready(void) {
	// Do not run features from WSAStartup — Cbuf/command/cvar work belongs on init_thread.
	ensure_init_thread();
}
