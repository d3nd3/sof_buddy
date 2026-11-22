#include "detour.h"
#include "engine_internal.h"
#include "game_module.h"
#include "mem_patch.h"
#include "profile_mp_103.h"
#include "sof2_api.h"
#include <algorithm>
#include <stdio.h>
#include <windows.h>

namespace {

struct VidModeEntry {
	const char* label;
	int width;
	int height;
	float aspect;
};

constexpr int kMaxModes = 96;
constexpr float kAspect = 1.0f;

VidModeEntry g_modes[kMaxModes];
char g_labels[kMaxModes][40];
int g_mode_count = 0;
volatile LONG g_installed = 0;
volatile LONG g_modeinfo_hooked = 0;

using R_GetModeInfo_t = int(__cdecl*)(int*, int*, float*, int);
static R_GetModeInfo_t R_GetModeInfo_orig = nullptr;

static bool desktop_pixels(int* w, int* h) {
	DEVMODEA dm = {};
	dm.dmSize = sizeof(dm);
	if (!EnumDisplaySettingsA(nullptr, ENUM_CURRENT_SETTINGS, &dm)) return false;
	*w = (int)dm.dmPelsWidth;
	*h = (int)dm.dmPelsHeight;
	return *w > 0 && *h > 0;
}

static int __cdecl R_GetModeInfo_hook(int* w, int* h, float* aspect, int mode) {
	if (mode == -1) {
		int dw = 0, dh = 0;
		if (desktop_pixels(&dw, &dh)) {
			*w = dw;
			*h = dh;
			*aspect = (float)dw / (float)dh;
			return 1;
		}
	}
	return R_GetModeInfo_orig(w, h, aspect, mode);
}

static bool install_r_mode_minus_one_hook(uint8_t* base) {
	if (InterlockedCompareExchange(&g_modeinfo_hooked, 1, 0) != 0) return true;
	void* tramp = nullptr;
	void* target = base + Sof2Mp103Profile::kRGetModeInfoRva;
	if (!sof2_hook_cdecl(target, (void*)R_GetModeInfo_hook, &tramp, Sof2Mp103Profile::kRGetModeInfoHookLen)) {
		InterlockedExchange(&g_modeinfo_hooked, 0);
		return false;
	}
	R_GetModeInfo_orig = (R_GetModeInfo_t)tramp;
	return true;
}

bool has_mode(int w, int h) {
	for (int i = 0; i < g_mode_count; ++i)
		if (g_modes[i].width == w && g_modes[i].height == h) return true;
	return false;
}

bool add_mode(int w, int h) {
	if (w < 320 || h < 240 || w > 8192 || h > 8192) return false;
	if (has_mode(w, h) || g_mode_count >= kMaxModes) return false;
	snprintf(g_labels[g_mode_count], sizeof g_labels[0], "Mode %2d: %dx%d", g_mode_count, w, h);
	g_modes[g_mode_count] = {g_labels[g_mode_count], w, h, kAspect};
	++g_mode_count;
	return true;
}

void collect_display_modes(void) {
	DEVMODEA dm = {};
	dm.dmSize = sizeof(dm);
	if (EnumDisplaySettingsA(nullptr, ENUM_CURRENT_SETTINGS, &dm))
		add_mode((int)dm.dmPelsWidth, (int)dm.dmPelsHeight);
	if (EnumDisplaySettingsA(nullptr, ENUM_REGISTRY_SETTINGS, &dm))
		add_mode((int)dm.dmPelsWidth, (int)dm.dmPelsHeight);
	for (DWORD i = 0; EnumDisplaySettingsA(nullptr, i, &dm); ++i)
		add_mode((int)dm.dmPelsWidth, (int)dm.dmPelsHeight);
	add_mode(GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
	static const int kCommon[][2] = {{1920, 1080}, {1600, 1024}, {1680, 1050}, {1366, 768},
	                                 {1280, 720},  {1280, 800},  {1440, 900},  {2560, 1440},
	                                 {1920, 1200}, {2560, 1080}, {1536, 864},  {3840, 2160}};
	for (const int* r : kCommon) add_mode(r[0], r[1]);
	const Sof2EngineApi* e = sof2_engine();
	if (e && e->Cvar_Get) {
		cvar_t* w = e->Cvar_Get("r_customwidth", "0", 0, nullptr);
		cvar_t* h = e->Cvar_Get("r_customheight", "0", 0, nullptr);
		if (w && h && w->value >= 320 && h->value >= 240)
			add_mode((int)w->value, (int)h->value);
	}
}

void sort_and_relabel(int stock_count) {
	if (g_mode_count <= stock_count + 1) return;
	std::sort(g_modes + stock_count, g_modes + g_mode_count,
	          [](const VidModeEntry& a, const VidModeEntry& b) {
		          return a.width * a.height > b.width * b.height;
	          });
	for (int i = stock_count; i < g_mode_count; ++i) {
		snprintf(g_labels[i], sizeof g_labels[0], "Mode %2d: %dx%d", i, g_modes[i].width,
		         g_modes[i].height);
		g_modes[i].label = g_labels[i];
	}
}

bool try_install(void) {
	uint8_t* base = sof2_game_base();
	if (!base || !sof2_mp103_matches(base)) return false;

	const int stock_count = *(const int*)(base + Sof2Mp103Profile::kVidModeCountRva);
	const VidModeEntry* stock = (const VidModeEntry*)(base + Sof2Mp103Profile::kVidModeTableRva);
	if (stock_count <= 0 || stock_count > 32) return false;

	g_mode_count = 0;
	for (int i = 0; i < stock_count; ++i) g_modes[g_mode_count++] = stock[i];
	collect_display_modes();
	sort_and_relabel(stock_count);
	if (g_mode_count <= stock_count) return false;

	if (!sof2_mem_write(base + Sof2Mp103Profile::kVidModeCountRva, &g_mode_count, sizeof g_mode_count))
		return false;

	const uint32_t table_va = (uint32_t)(uintptr_t)g_modes;
	if (!sof2_patch_imm32(base + Sof2Mp103Profile::kVidModeResolveAddRva, 0x05, table_va) ||
	    !sof2_patch_imm32(base + Sof2Mp103Profile::kVidModelistEdiRva, 0xBF, table_va)) {
		sof2_mem_write(base + Sof2Mp103Profile::kVidModeCountRva, &stock_count, sizeof stock_count);
		return false;
	}

	const uint32_t rmode_max = 0x42200000u;
	sof2_mem_write(base + Sof2Mp103Profile::kRModeMaxRva, &rmode_max, sizeof rmode_max);
	return true;
}

}  // namespace

int feature_video_modes_mode_count(void) {
	uint8_t* base = sof2_game_base();
	if (!base || !sof2_mp103_matches(base)) return 0;
	if (g_mode_count > 0) return g_mode_count;
	const int stock_count = *(const int*)(base + Sof2Mp103Profile::kVidModeCountRva);
	return stock_count > 0 && stock_count <= kMaxModes ? stock_count : 0;
}

const char* feature_video_modes_mode_label(int index) {
	if (index < 0 || index >= g_mode_count) {
		uint8_t* base = sof2_game_base();
		if (!base) return nullptr;
		const int stock_count = *(const int*)(base + Sof2Mp103Profile::kVidModeCountRva);
		const VidModeEntry* stock = (const VidModeEntry*)(base + Sof2Mp103Profile::kVidModeTableRva);
		if (index < 0 || index >= stock_count) return nullptr;
		return stock[index].label;
	}
	return g_modes[index].label;
}

void feature_video_modes_early_startup(void) {
	uint8_t* base = sof2_game_base();
	if (base && sof2_mp103_matches(base)) install_r_mode_minus_one_hook(base);
}

void feature_video_modes_post_cvar(void) {
	if (g_installed) return;
	if (!try_install()) return;
	InterlockedExchange(&g_installed, 1);
	sof2_engine_logf("sof2bud: modelist %d modes\n", g_mode_count);
}
