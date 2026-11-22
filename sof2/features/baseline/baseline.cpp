#include "game_module.h"
#include "mem_patch.h"
#include "profile_mp_103.h"
#include <windows.h>
#include <psapi.h>
#include <string.h>

static volatile LONG g_patches_applied = 0;

static bool patch_gfx_extensions(uint8_t* base) {
	const uint8_t* esp_insn = base + Sof2Mp103Profile::kGfxEspAddRva;
	if (esp_insn[0] != 0x83 || esp_insn[1] != 0xC4) return false;
	if (esp_insn[2] == 0x38) return true;
	if (esp_insn[2] != 0x44) return false;
	/* 1fx gold: NOP out second ri.Printf (GL_EXTENSIONS) through 0x4BB353 */
	static const uint8_t kSkip[18] = {
		0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
		0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
	if (!sof2_mem_write(base + Sof2Mp103Profile::kGfxSkipRva, kSkip, sizeof kSkip)) return false;
	static const uint8_t kAddEsp38[3] = {0x83, 0xC4, 0x38};
	return sof2_mem_write(base + Sof2Mp103Profile::kGfxEspAddRva, kAddEsp38, 3);
}

static bool apply_mp_baseline(uint8_t* base) {
	MODULEINFO mi = {};
	if (!GetModuleInformation(GetCurrentProcess(), (HMODULE)base, &mi, sizeof(mi))) return false;
	if (!sof2_mp103_matches(base)) return false;
	if (!patch_gfx_extensions(base)) return false;
	const uint32_t rmode_max = 0x41300000u;
	const uint32_t rmode_min = 0xBF800000u;
	if (!sof2_mem_write(base + Sof2Mp103Profile::kRModeMaxRva, &rmode_max, 4)) return false;
	if (!sof2_mem_write(base + Sof2Mp103Profile::kRModeMinRva, &rmode_min, 4)) return false;
	const uint32_t rmode_def = Sof2Mp103Profile::kRModeDefaultMinusOneVa;
	if (!sof2_mem_write(base + Sof2Mp103Profile::kRModeDefaultImmRva, &rmode_def, 4)) return false;
	char master[32];
	memset(master, 0, sizeof master);
	lstrcpyA(master, "master.1fxmod.org");
	return sof2_mem_write(base + Sof2Mp103Profile::kMasterRva, master, sizeof master);
}

void feature_baseline_early_startup(void) {
	if (InterlockedCompareExchange(&g_patches_applied, 1, 0) != 0) return;
	uint8_t* base = sof2_game_base();
	if (!base || !apply_mp_baseline(base)) InterlockedExchange(&g_patches_applied, 0);
}
