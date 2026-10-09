/*
	VSync Toggle - Display refresh readout

	Measures the real display refresh rate via the system and compares it against the
	Framerate Cap, so the F12 CPU tab can show live values plus a mismatch warning.
*/

#include "feature_config.h"

#if FEATURE_VSYNC_TOGGLE

#include "sof_buddy.h"
#include "sof_compat.h"
#include "util.h"
#include "shared.h"
#include "generated_detours.h"

#include <windows.h>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <vector>

// Actual display refresh, system truth (not the requested cvar). EnumDisplaySettings first,
// GetDeviceCaps VREFRESH as fallback. Returns 0 when unreadable. Values <= 1 mean "default".
static int query_display_hz(void) {
	DEVMODE dm;
	ZeroMemory(&dm, sizeof(dm));
	dm.dmSize = sizeof(dm);
	if (EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &dm) && dm.dmDisplayFrequency > 1)
		return (int)dm.dmDisplayFrequency;
	HDC hdc = GetDC(NULL);
	int hz = 0;
	if (hdc) {
		hz = GetDeviceCaps(hdc, VREFRESH);
		ReleaseDC(NULL, hdc);
	}
	return (hz > 1) ? hz : 0;
}

// Refresh the two display cvars shown on F12 -> CPU. Runs on CPU-page open (and once at
// RefDllLoaded), so first paint already shows live values. All pointers are lazily resolved
// with engine-identical defaults, so call order against the renderer never matters.
void sofbuddy_refresh_display_info(void) {
	if (!detour_Cvar_Get::oCvar_Get || !detour_Cvar_Set2::oCvar_Set2) return;

	if (!gl_displayrefresh)
		gl_displayrefresh = detour_Cvar_Get::oCvar_Get("gl_displayrefresh", "0", 1, NULL);
	if (!vs_cl_maxfps)
		vs_cl_maxfps = detour_Cvar_Get::oCvar_Get("cl_maxfps", "30", 0, NULL);
	if (!_sb_internal_display_info)
		_sb_internal_display_info = detour_Cvar_Get::oCvar_Get("_sb_internal_display_info", "unknown", 0, NULL);
	if (!_sb_internal_fps_cap_mismatch)
		_sb_internal_fps_cap_mismatch = detour_Cvar_Get::oCvar_Get("_sb_internal_fps_cap_mismatch", "0", 0, NULL);
	if (!gl_displayrefresh || !vs_cl_maxfps ||
	    !_sb_internal_display_info || !_sb_internal_fps_cap_mismatch)
		return;

	const int actual = query_display_hz();

	char info[64];
	if (actual > 1)
		snprintf(info, sizeof(info), "%d Hz", actual);
	else
		snprintf(info, sizeof(info), "unknown");
	detour_Cvar_Set2::oCvar_Set2(
		const_cast<char*>("_sb_internal_display_info"), info, true);

	// Rebuild the refresh-request picker from the modes the driver actually reports for
	// the current resolution. Auto/0 first (safe fallback + driver decides), then ascending.
	// Capped so the generated quoted strings stay far under the ~240-byte menu parse limit.
	if (!_sb_internal_display_modes_rmf)
		_sb_internal_display_modes_rmf = detour_Cvar_Get::oCvar_Get("_sb_internal_display_modes_rmf", "", 0, NULL);
	if (_sb_internal_display_modes_rmf) {
		DEVMODE cur;
		ZeroMemory(&cur, sizeof(cur));
		cur.dmSize = sizeof(cur);
		int curW = 0, curH = 0;
		if (EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &cur)) {
			curW = (int)cur.dmPelsWidth;
			curH = (int)cur.dmPelsHeight;
		}
		std::set<int> rates;
		for (int i = 0; i < 2048; ++i) {
			DEVMODE dm;
			ZeroMemory(&dm, sizeof(dm));
			dm.dmSize = sizeof(dm);
			if (!EnumDisplaySettings(NULL, i, &dm))
				break;
			if (curW > 0 && ((int)dm.dmPelsWidth != curW || (int)dm.dmPelsHeight != curH))
				continue;
			if (dm.dmDisplayFrequency > 1 && (int)rates.size() < 32)
				rates.insert((int)dm.dmDisplayFrequency);
		}
		std::string labels = "Auto";
		std::string values = "0";
		for (int hz : rates) {
			char num[16];
			snprintf(num, sizeof(num), "%d", hz);
			labels += ',';
			labels += num;
			values += ',';
			values += num;
		}
		std::string rmf = "<list \"";
		rmf += labels;
		rmf += "\" match \"";
		rmf += values;
		rmf += "\" cvar gl_displayrefresh atext \"Requested Display Refresh Rate : \" key mouse1 \"vid_restart\" key mouse2 \"vid_restart\" noshade tip \"Request display refresh from the driver. Applies with vid_restart. Auto lets the driver decide.\"><hbr>";
		// sofplus-only border toggle: show it only when the cvar actually exists (so
		// vanilla installs never get a dead row) and only while windowed (borders are
		// meaningless fullscreen). findCvar reads without creating. Fail closed.
		if (!vid_fullscreen)
			vid_fullscreen = detour_Cvar_Get::oCvar_Get("vid_fullscreen", "1", 1, NULL);
		if (vid_fullscreen && vid_fullscreen->value == 0.0f &&
		    findCvar(const_cast<char*>("_sp_cl_vid_border"))) {
			rmf += "<list \"No,Yes\" match \"0,1\" cvar _sp_cl_vid_border atext \"Window Border : \" noshade tip \"Border around the SoF window (sofplus).\"><hbr>";
		}
		detour_Cvar_Set2::oCvar_Set2(
			const_cast<char*>("_sb_internal_display_modes_rmf"), const_cast<char*>(rmf.c_str()), true);
	}

	// Warning condition for RMF <cinclude>: VSync must actually be able to cap below the
	// display - swapinterval on, a real reading, and cap strictly below it. Uncapped
	// (<= 0) can never mismatch by definition.
	const float cap = vs_cl_maxfps->value;
	const bool vsync_on = gl_swapinterval && gl_swapinterval->value != 0.0f;
	const bool mismatch = vsync_on && actual > 1 && cap > 0.0f && cap < (float)actual;
	detour_Cvar_Set2::oCvar_Set2(
		const_cast<char*>("_sb_internal_fps_cap_mismatch"),
		const_cast<char*>(mismatch ? "1" : "0"), true);
}

// Console command so menu rows can recompute-then-refresh on click:
// key mouse1 "sofbuddy_display_refresh;refresh". Native list cycling runs first
// (same pattern as the Profile row's key mouse1 "refresh"), so by the time this
// runs the new value is already set.
void Cmd_SoFBuddy_Display_Refresh_f(void) {
	sofbuddy_refresh_display_info();
}

#endif // FEATURE_VSYNC_TOGGLE
