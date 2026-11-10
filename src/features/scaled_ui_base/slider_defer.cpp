#include "feature_config.h"

#if FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE

#include <windows.h>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <string>
#include "sof_compat.h"
#include "util.h"
#include "shared.h"
#include "sofbuddy_cfg.h"
#include "DetourXS/detourxs.h"
#if FEATURE_SCALED_MENU
extern void scaled_menu_reload_open_page(void);
#endif

namespace {

using tCvarSetValue = void(__cdecl*)(const char*, float);
using tSliderDraw = void(__thiscall*)(void*);

tCvarSetValue oCvar_SetValue = nullptr;
tSliderDraw oSlider_Draw = nullptr;
LPVOID g_detour = nullptr;
LPVOID g_slider_draw_detour = nullptr;
bool g_applying = false;
bool g_pending = false;
std::string g_pending_name;
cvar_t* g_pending_cvar = nullptr;
float g_pending_value = 0.0f;
DWORD g_pending_tick = 0;
bool g_reload_menu_pending = false;
DWORD g_reload_menu_tick = 0;
bool g_scaled_ui_ready = false;
bool g_reapply_scales_pending = false;

bool is_scale_cvar(const char* name) {
	if (!name) return false;
	return std::strncmp(name, "_sofbuddy_", 10) == 0 && std::strstr(name, "_scale") != nullptr;
}

bool is_menu_scale_cvar(const char* name) {
	if (!name) return false;
	return std::strcmp(name, "_sofbuddy_menu_scale") == 0 ||
	       std::strcmp(name, "_sofbuddy_menu_scale_pct") == 0;
}

bool caller_is_slider_update() {
	const uintptr_t ret = reinterpret_cast<uintptr_t>(__builtin_return_address(0));
	const uintptr_t start = reinterpret_cast<uintptr_t>(rvaToAbsExe(reinterpret_cast<void*>(0xD0E10)));
	return ret >= start && ret < start + 0x300;
}

void thiscall_noargs_void(void* self, void* fn) {
	if (!fn) return;
	__asm__ __volatile__(
		"movl %0, %%ecx\n\t"
		"call *%1"
		: : "m"(self), "m"(fn)
		: "ecx", "eax", "edx", "memory");
}

bool pending_preview(const cvar_t* cvar, float* value) {
	if (!g_pending || !cvar || !value) return false;
	if (cvar != g_pending_cvar) return false;
	*value = g_pending_value;
	return true;
}

#if defined(__GNUC__)
__attribute__((thiscall))
#endif
void hkSlider_Draw(void* slider) {
	if (!slider || !oSlider_Draw) return;
	cvar_t* cvar = *reinterpret_cast<cvar_t**>(static_cast<char*>(slider) + 0x50);
	float preview = 0.0f;
	if (pending_preview(cvar, &preview)) {
		const float saved = cvar->value;
		cvar->value = preview;
		thiscall_noargs_void(slider, reinterpret_cast<void*>(oSlider_Draw));
		cvar->value = saved;
		return;
	}
	thiscall_noargs_void(slider, reinterpret_cast<void*>(oSlider_Draw));
}

void __cdecl hkCvar_SetValue(const char* name, float value) {
	if (!g_applying && caller_is_slider_update() && is_scale_cvar(name)) {
		g_pending_name = name;
		g_pending_cvar = findCvar(const_cast<char*>(name));
		g_pending_value = value;
		g_pending_tick = GetTickCount();
		g_pending = true;
		return;
	}
	if (oCvar_SetValue) oCvar_SetValue(name, value);
}

} // namespace

void scaled_ui_slider_defer_flush(void) {
	if (!g_pending || !oCvar_SetValue) return;
	g_pending = false;
	g_pending_cvar = nullptr;
	g_applying = true;
	oCvar_SetValue(g_pending_name.c_str(), g_pending_value);
	g_applying = false;
}

void scaled_ui_request_scale_reapply(void) {
	if (!g_scaled_ui_ready) return;
	g_reapply_scales_pending = true;
}

void scaled_ui_request_menu_reload(void) {
#if FEATURE_SCALED_MENU
	if (!g_scaled_ui_ready) return;
	g_reload_menu_pending = true;
	g_reload_menu_tick = GetTickCount();
#endif
}

void scaled_ui_slider_defer_init(void) {
	g_scaled_ui_ready = true;
	if (!g_detour) {
		void* target = rvaToAbsExe(reinterpret_cast<void*>(0x22350));
		if (target) {
			g_detour = DetourCreate(target, reinterpret_cast<void*>(hkCvar_SetValue),
			                        DETOUR_TYPE_JMP, 7);
			if (g_detour) oCvar_SetValue = reinterpret_cast<tCvarSetValue>(g_detour);
		}
	}
	if (!g_slider_draw_detour) {
		void* draw_target = rvaToAbsExe(reinterpret_cast<void*>(0xD1110));
		if (draw_target) {
			g_slider_draw_detour = DetourCreate(draw_target, reinterpret_cast<void*>(hkSlider_Draw),
			                                    DETOUR_TYPE_JMP, DETOUR_LEN_AUTO);
			if (g_slider_draw_detour) oSlider_Draw = reinterpret_cast<tSliderDraw>(g_slider_draw_detour);
		}
	}
}

void scaled_ui_slider_defer_tick(int msec) {
	(void)msec;
	if (g_reapply_scales_pending) {
		g_reapply_scales_pending = false;
		reapply_scale_percentages();
		sofbuddy_cfg_save_now();
	}
#if FEATURE_SCALED_MENU
	if (g_reload_menu_pending) {
		if (g_pending && is_menu_scale_cvar(g_pending_name.c_str())) {
			g_reload_menu_pending = false;
		} else {
			const DWORD now = GetTickCount();
			const bool button_up = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) == 0;
			if ((button_up && now - g_reload_menu_tick >= 250) ||
			    now - g_reload_menu_tick >= 1000) {
				g_reload_menu_pending = false;
				scaled_menu_reload_open_page();
			}
		}
	}
#endif
	if (!g_pending || !oCvar_SetValue) return;
	if (GetTickCount() - g_pending_tick < 120) return;
	const bool menu_scale = is_menu_scale_cvar(g_pending_name.c_str());
	const bool persist = is_scale_cvar(g_pending_name.c_str());
#if FEATURE_SCALED_MENU
	const float menu_scale_before = menuScale;
#endif
	g_pending = false;
	g_pending_cvar = nullptr;
	g_applying = true;
	oCvar_SetValue(g_pending_name.c_str(), g_pending_value);
	g_applying = false;
	if (persist) sofbuddy_cfg_save_now();
#if FEATURE_SCALED_MENU
	if (menu_scale && std::fabs(menuScale - menu_scale_before) > 0.001f) {
		g_reload_menu_pending = true;
		g_reload_menu_tick = GetTickCount();
	}
#endif
}

#endif
