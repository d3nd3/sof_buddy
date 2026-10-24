#include "feature_config.h"

#if FEATURE_CL_MAXFPS_SINGLEPLAYER

#include "detours.h"
#include "sof_compat.h"
#include "util.h"
#include "sof_buddy.h"
#include "../shared.h"
#if FEATURE_MEDIA_TIMERS
#include "features/media_timers/shared.h"
#endif

#include <climits>
#include <cstdlib>
#include <cstring>

// CL_Frame maxfps gate (SoF.exe): fild..jz @ RVA 0xD975 (35 bytes).
static constexpr uintptr_t kClFrameGateRva = 0x0000D975;
static constexpr uintptr_t kClFrameGateExitRva = 0x0000DD0C;
static constexpr uintptr_t kExtratimeRva = 0x001E7578;
static constexpr uintptr_t kExeClMaxfpsRva = 0x00225050;
static constexpr size_t kClFrameGateLen = 35;
static constexpr uintptr_t kClFrameSpSkipRva = 0x0000D973;

static const unsigned char kClFrameGateVanilla[kClFrameGateLen] = {
    0xDB, 0x05, 0x78, 0x75, 0x1E, 0x20, 0xA1, 0x50, 0x50, 0x22, 0x20,
    0xD9, 0x05, 0xA4, 0x14, 0x11, 0x20, 0xD8, 0x70, 0x18, 0xDE, 0xD9,
    0xDF, 0xE0, 0x25, 0x00, 0x41, 0x00, 0x00, 0x0F, 0x84, 0x74, 0x03,
    0x00, 0x00,
};

static unsigned char g_cl_frame_gate_saved[kClFrameGateLen];
static bool g_cl_frame_gate_saved_valid = false;
static bool g_cl_frame_gate_patched = false;

#if FEATURE_MEDIA_TIMERS
static const int* g_cl_frame_extratime = nullptr;
static cvar_t* g_exe_cl_maxfps = nullptr;
#endif

static void write_exe_bytes(uintptr_t rva, const void* data, size_t len)
{
	unsigned char* const site =
	    static_cast<unsigned char*>(rvaToAbsExe(reinterpret_cast<void*>(rva)));
	if (!site || !data || !len) return;
	DWORD prot = 0;
	if (!VirtualProtect(site, len, PAGE_EXECUTE_READWRITE, &prot)) return;
	memcpy(site, data, len);
	VirtualProtect(site, len, prot, &prot);
}

static bool site_is_our_gate_patch(const unsigned char* site)
{
	return site && site[0] == 0xE8 && site[5] == 0x85 && site[6] == 0xC0 &&
	       site[7] == 0x0F && site[8] == 0x84;
}

static void mfps_unpatch_cl_frame_gate(void)
{
	if (!g_cl_frame_gate_patched) return;
	const unsigned char* const restore =
	    g_cl_frame_gate_saved_valid ? g_cl_frame_gate_saved : kClFrameGateVanilla;
	write_exe_bytes(kClFrameGateRva, restore, kClFrameGateLen);
	g_cl_frame_gate_patched = false;
}

void mfps_restore_cl_frame_maxfps_gate(void)
{
	unsigned char* const site =
	    static_cast<unsigned char*>(rvaToAbsExe(reinterpret_cast<void*>(kClFrameGateRva)));
	if (!site) return;
	if (site_is_our_gate_patch(site)) {
		mfps_unpatch_cl_frame_gate();
		return;
	}
	if (!memcmp(site, kClFrameGateVanilla, kClFrameGateLen)) return;
	if (site[0] != 0xE8) return;

	write_exe_bytes(kClFrameGateRva, kClFrameGateVanilla, kClFrameGateLen);
	g_cl_frame_gate_patched = false;
	PrintOut(PRINT_LOG,
	         "cl_maxfps_singleplayer: restored vanilla CL_Frame maxfps gate\n");
}

void mfps_apply_cl_frame_sp_throttle_patch(void)
{
	cvar_t** const pcl =
	    reinterpret_cast<cvar_t**>(rvaToAbsExe(reinterpret_cast<void*>(kExeClMaxfpsRva)));
	if (!pcl || !*pcl) {
		PrintOut(PRINT_DEV,
		         "cl_maxfps_singleplayer: defer SP throttle patch (cl_maxfps not ready)\n");
		return;
	}
	unsigned char* const jnz =
	    static_cast<unsigned char*>(rvaToAbsExe(reinterpret_cast<void*>(kClFrameSpSkipRva)));
	if (!jnz || jnz[0] == 0x90) return;
	WriteByte(jnz, 0x90);
	WriteByte(jnz + 1, 0x90);
	PrintOut(PRINT_LOG, "cl_maxfps_singleplayer: CL_Frame SP throttle patch applied\n");
}

#if FEATURE_MEDIA_TIMERS

extern cvar_t* _sofbuddy_cl_maxfps_exact;

// Integer-only: no x87 — must match winmain Bresenham budget (g_cl_maxfps_target_msec).
extern "C" int __cdecl mfps_cl_frame_gate_decide(void)
{
	if (!g_cl_frame_extratime) return 1;
	if (g_exe_cl_maxfps && g_exe_cl_maxfps->string &&
	    strcmp(g_exe_cl_maxfps->string, "923") == 0)
		return 1;
	const int et = *g_cl_frame_extratime;
	return et >= sofbuddy_exact_frame_budget_ms() ? 1 : 0;
}

static bool mfps_exact_mode_on(void)
{
	return _sofbuddy_cl_maxfps_exact && _sofbuddy_cl_maxfps_exact->value != 0.0f;
}

static void mfps_cache_gate_pointers(void)
{
	g_cl_frame_extratime =
	    static_cast<const int*>(rvaToAbsExe(reinterpret_cast<void*>(kExtratimeRva)));
	cvar_t** const pcl =
	    reinterpret_cast<cvar_t**>(rvaToAbsExe(reinterpret_cast<void*>(kExeClMaxfpsRva)));
	g_exe_cl_maxfps = (pcl && *pcl) ? *pcl : nullptr;
}

static void mfps_apply_cl_frame_gate_call_patch(void)
{
	if (g_cl_frame_gate_patched) return;
	mfps_cache_gate_pointers();
	if (!g_cl_frame_extratime || !g_exe_cl_maxfps) return;

	unsigned char* const site =
	    static_cast<unsigned char*>(rvaToAbsExe(reinterpret_cast<void*>(kClFrameGateRva)));
	if (!site) return;

	if (!g_cl_frame_gate_saved_valid) {
		memcpy(g_cl_frame_gate_saved, site, kClFrameGateLen);
		g_cl_frame_gate_saved_valid = true;
	}

	void* const decide = reinterpret_cast<void*>(&mfps_cl_frame_gate_decide);
	const ptrdiff_t call_rel = static_cast<unsigned char*>(decide) - (site + 5);
	if (call_rel < INT_MIN || call_rel > INT_MAX) {
		PrintOut(PRINT_BAD,
		         "cl_maxfps_singleplayer: CL_Frame gate call out of range; skipped\n");
		return;
	}

	const int32_t jrel = static_cast<int32_t>(
	    kClFrameGateExitRva - (kClFrameGateRva + 13));

	DWORD prot = 0;
	if (!VirtualProtect(site, kClFrameGateLen, PAGE_EXECUTE_READWRITE, &prot)) return;

	WriteE8Call(site, decide);
	site[5] = 0x85;
	site[6] = 0xC0;
	site[7] = 0x0F;
	site[8] = 0x84;
	memcpy(site + 9, &jrel, 4);
	memset(site + 13, 0x90, kClFrameGateLen - 13);
	VirtualProtect(site, kClFrameGateLen, prot, &prot);

	g_cl_frame_gate_patched = true;
	PrintOut(PRINT_LOG, "cl_maxfps_singleplayer: CL_Frame exact gate patch installed\n");
}

void mfps_install_cl_frame_exact_gate_detour(void)
{
	if (mfps_exact_mode_on())
		mfps_apply_cl_frame_gate_call_patch();
	else
		mfps_unpatch_cl_frame_gate();
}

void mfps_set_exact_cl_frame_patch(bool enable)
{
	if (enable)
		mfps_apply_cl_frame_gate_call_patch();
	else
		mfps_unpatch_cl_frame_gate();
}

#else

void mfps_install_cl_frame_exact_gate_detour(void) {}
void mfps_set_exact_cl_frame_patch(bool enable) { (void)enable; }

#endif // FEATURE_MEDIA_TIMERS

#endif // FEATURE_CL_MAXFPS_SINGLEPLAYER
