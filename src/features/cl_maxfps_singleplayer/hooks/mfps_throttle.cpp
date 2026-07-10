#include "feature_config.h"

#if FEATURE_CL_MAXFPS_SINGLEPLAYER

#include "generated_detours.h"
#include "sof_compat.h"
#include "../shared.h"

// Mirrors CL_Frame throttle checks (SoF.exe @ 0x2000D960..0x2000D992).
static int* mfps_extratime(void) { return (int*)rvaToAbsExe((void*)0x001E7578); }
static int* mfps_cls_state(void) { return (int*)rvaToAbsExe((void*)0x001C1F00); }
static bool g_mfps_throttle_frame = false;

static bool mfps_cl_frame_would_throttle(int msec)
{
	int* et = mfps_extratime();
	if (!et) {
		return false;
	}

	// CL_Frame adds msec to extratime before its throttle checks.
	const int etAfter = *et + msec;

	int* cs = mfps_cls_state();
	if (cs && *cs == 7 && etAfter < 100) {
		return true;
	}

	if (!detour_Cvar_Get::oCvar_Get) {
		return false;
	}

	cvar_t* timedemo = detour_Cvar_Get::oCvar_Get("timedemo", "0", 0, nullptr);
	if (timedemo && timedemo->value) {
		return false;
	}

	cvar_t* maxfps = detour_Cvar_Get::oCvar_Get("cl_maxfps", "0", 0, nullptr);
	if (!maxfps || maxfps->value <= 0.0f) {
		return false;
	}

	int minMsec = (int)(1000.0f / maxfps->value);
	if (minMsec < 1) {
		minMsec = 1;
	}
	return etAfter < minMsec;
}

// Qcommon_Frame Pre: decide before SV_Frame runs so server sim stays paired with CL throttle.
void mfps_qcommon_frame_pre(int& msec)
{
	g_mfps_throttle_frame = mfps_cl_frame_would_throttle(msec);
}

void mfps_sv_frame_override(int msec, detour_SV_Frame::tSV_Frame original)
{
	if (g_mfps_throttle_frame) {
		return;
	}
	original(msec);
}

#endif // FEATURE_CL_MAXFPS_SINGLEPLAYER
