/*
	Intro / disclaimer startup scaling (intro.ghb + m_disclaimer red text).

	640×480 virtual page mapped with vid_w/640; letterbox FreeY on disclaimer;
	intro ghoul scale + y shift. Independent of Scaled Menu cvars.
*/

#include "feature_config.h"

#if FEATURE_INTRO_SCALER

#ifdef UI_MENU

#include "sof_compat.h"
#include "features.h"
#include "util.h"
#include "detours.h"
#include "runtime_features.h"
#include "generated_detours.h"
#include "shared.h"
#if FEATURE_SCALED_MENU
#include "features/scaled_menu/shared.h"
#endif

#include <cmath>
#include <cstdint>

namespace intro_scaler_detail {

#if defined(__GNUC__) && defined(__i386__)
static void thiscall_ghoul_draw(void* ghoul, void* fn) {
	if (!fn) return;
	__asm__ __volatile__(
		"movl %0, %%ecx\n\t"
		"call *%1"
		: : "r"(ghoul), "r"(fn)
		: "ecx", "edx", "memory");
}
#endif

constexpr uintptr_t kRvaMenuStmSetClip = 0x000EA2C0u;
constexpr uintptr_t kRvaDrawRenderRectIat = 0x004035FCu;
constexpr uintptr_t kRvaBackdropSetClipOpen = 0x000C3F2Eu;
constexpr uintptr_t kRvaGhoulDraw = 0x000D7AB0u;
constexpr uintptr_t kRvaGhoulVtbl = 0x001129E0u;

constexpr ptrdiff_t kRect_MenuPtr = 0x04;
constexpr ptrdiff_t kGhoul_Xoff = 0x14;
constexpr ptrdiff_t kGhoul_Yoff = 0x18;
constexpr ptrdiff_t kGhoul_CenterAdjY = 0x2C;
constexpr ptrdiff_t kGhoul_ModelScale = 0x1C;
constexpr ptrdiff_t kGhoul_VirtW = 0x20;
constexpr ptrdiff_t kGhoul_VirtH = 0x24;
constexpr ptrdiff_t kGhoul_ModelInst = 0x90;
constexpr ptrdiff_t kStm_GlobalPage = 0x89;
constexpr ptrdiff_t kMenuC_FreeY = 0xA0;
constexpr ptrdiff_t kMenuC_FreeX = 0x9C;
constexpr ptrdiff_t kMenuC_GlobalClipW = 0xA4;
constexpr ptrdiff_t kMenuC_GlobalClipH = 0xA8;
constexpr ptrdiff_t kMenuC_ResizeScreen = 0x8C;
constexpr ptrdiff_t kFrame_OffX = 0x40;
constexpr ptrdiff_t kFrame_OffY = 0x44;
constexpr ptrdiff_t kFrame_W = 0x48;
constexpr ptrdiff_t kFrame_H = 0x4C;

static bool intro_runtime_on(void) {
	return RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_INTRO_SCALER);
}

static bool menu_c_is_resize_popup(const void* menu_c) {
	return menu_c && *reinterpret_cast<const uint8_t*>(static_cast<const char*>(menu_c) + kMenuC_ResizeScreen) != 0;
}

static bool widget_is_ghoul(const void* widget) {
	if (!widget) return false;
	const void* vt = *static_cast<const void* const*>(widget);
	return vt == rvaToAbsExe(reinterpret_cast<void*>(kRvaGhoulVtbl));
}

// rect+0x04 → stm; stm+0 → frame; frame+0 → menu screen.
static void* menu_outer_from_widget(void* widget) {
	if (!widget) return nullptr;
	auto* stm = *reinterpret_cast<void**>(static_cast<char*>(widget) + kRect_MenuPtr);
	if (!stm) return nullptr;
	auto* frame = *reinterpret_cast<void**>(stm);
	return frame ? *reinterpret_cast<void**>(frame) : nullptr;
}

static bool sane_scale(float v) {
	return std::isfinite(v) && v > 0.01f && v < 64.0f;
}

static bool s_intro_cinematic_active = false;
static bool s_letterbox_menu_active = false;
static void* s_intro_menu_screen = nullptr;
static void* s_intro_ghoul_layout_done = nullptr;
static void* s_letterbox_canvas_menu = nullptr;
static int s_letterbox_canvas_saved_free_y = 0;

static void (*s_clip_extend)(int* x, int* w, const void* menu_c) = nullptr;

static float page_width_scale(void) {
	return current_vid_w > 0 ? static_cast<float>(current_vid_w) / 640.0f : 1.0f;
}

static int page_vertical_shift_px(void) {
	const float s = page_width_scale();
	if (current_vid_h <= 0 || !sane_scale(s)) return 0;
	return static_cast<int>(std::lround((static_cast<float>(current_vid_h) - 480.f * s) * 0.5f));
}

static void letterbox_restore_canvas_free_y(void) {
	if (!s_letterbox_canvas_menu) return;
	char* m = static_cast<char*>(s_letterbox_canvas_menu);
	*reinterpret_cast<int*>(m + kMenuC_FreeY) = s_letterbox_canvas_saved_free_y;
	s_letterbox_canvas_menu = nullptr;
}

static void letterbox_apply_canvas_free_y(void* menu) {
	if (!intro_runtime_on() || !s_letterbox_menu_active || s_intro_cinematic_active || !menu ||
	    menu_c_is_resize_popup(menu))
		return;
	const int shift = page_vertical_shift_px();
	if (shift == 0) return;
	if (s_letterbox_canvas_menu != menu) {
		letterbox_restore_canvas_free_y();
		auto* m = static_cast<char*>(menu);
		s_letterbox_canvas_saved_free_y = *reinterpret_cast<int*>(m + kMenuC_FreeY);
		s_letterbox_canvas_menu = menu;
	}
	*reinterpret_cast<int*>(static_cast<char*>(menu) + kMenuC_FreeY) =
	    s_letterbox_canvas_saved_free_y + shift;
}

static void intro_apply_ghoul_vertical_shift(void* ghoul) {
	if (!s_intro_cinematic_active || !ghoul) return;
	const int shift = page_vertical_shift_px();
	if (shift == 0) return;
	char* g = static_cast<char*>(ghoul);
	*reinterpret_cast<int*>(g + kGhoul_Yoff) += shift;
	int& adj = *reinterpret_cast<int*>(g + kGhoul_CenterAdjY);
	if (adj != 0) adj += shift;
}

static bool ghoul_is_intro_rmf_layout(const void* ghoul) {
	if (!ghoul) return false;
	const auto* g = static_cast<const char*>(ghoul);
	return *reinterpret_cast<const int*>(g + kGhoul_VirtW) == 640 &&
	       *reinterpret_cast<const int*>(g + kGhoul_VirtH) == 480;
}

static void intro_sync_ghoul_scale(void* ghoul) {
	if (!intro_runtime_on() || !ghoul || !widget_is_ghoul(ghoul) || !s_intro_cinematic_active) return;
	char* g = static_cast<char*>(ghoul);
	const float intro = page_width_scale();
	const float stock = 1.0f;
	if (!sane_scale(intro) || std::fabs(stock - intro) < 1e-4f) {
		*reinterpret_cast<float*>(g + kGhoul_ModelScale) = intro;
		return;
	}
	const float cur = *reinterpret_cast<float*>(g + kGhoul_ModelScale);
	const float r = intro / stock;
	if (std::fabs(cur - stock) < 0.08f || std::fabs(cur - 1.0f) < 0.08f) {
		auto mul = [&](ptrdiff_t off) {
			int* p = reinterpret_cast<int*>(g + off);
			if (*p != 0) *p = static_cast<int>(std::lround(static_cast<float>(*p) * r));
		};
		mul(kGhoul_Xoff);
		mul(kGhoul_Yoff);
	}
	*reinterpret_cast<float*>(g + kGhoul_ModelScale) = intro;
}

static void intro_on_ghoul_parsed(void* ghoul) {
	if (widget_is_ghoul(ghoul) && ghoul_is_intro_rmf_layout(ghoul)) s_intro_cinematic_active = true;
	void* screen = menu_outer_from_widget(ghoul);
	if (screen) s_intro_menu_screen = screen;
	intro_sync_ghoul_scale(ghoul);
	intro_apply_ghoul_vertical_shift(ghoul);
	s_intro_ghoul_layout_done = ghoul;
}

static void intro_ghoul_prepare_draw(void* ghoul) {
	if (!intro_runtime_on() || !s_intro_cinematic_active || !ghoul ||
	    ghoul == s_intro_ghoul_layout_done)
		return;
	if (!widget_is_ghoul(ghoul)) return;
	if (!*reinterpret_cast<void* const*>(static_cast<char*>(ghoul) + kGhoul_ModelInst)) return;
	intro_on_ghoul_parsed(ghoul);
}

using Draw_RenderRect_t = void(__cdecl*)(int x, int y, int w, int h);
static Draw_RenderRect_t Draw_RenderRect = nullptr;

static bool set_clip_from_backdrop_open() {
	void* ret = __builtin_return_address(0);
	return ret == rvaToAbsExe(reinterpret_cast<void*>(kRvaBackdropSetClipOpen));
}

static void __cdecl hk_menu_stm_set_clip(void* stm, int, int* outX, int* outY, int* outW, int* outH) {
	if (!stm || !outX || !outY || !outW || !outH) return;
	auto* st = static_cast<char*>(stm);
	auto* frame = *reinterpret_cast<char**>(stm);
	if (!frame) return;
	auto* menu = *reinterpret_cast<char**>(frame);
	if (!menu) return;
	if (s_letterbox_menu_active && !s_intro_cinematic_active) {
		s_intro_menu_screen = menu;
		letterbox_apply_canvas_free_y(menu);
	}
	const int ox = *reinterpret_cast<int*>(frame + kFrame_OffX);
	const int oy = *reinterpret_cast<int*>(frame + kFrame_OffY);
	int x, y, w, h;
	if (st[kStm_GlobalPage]) {
		x = *reinterpret_cast<int*>(menu + kMenuC_FreeX) + ox;
		y = *reinterpret_cast<int*>(menu + kMenuC_FreeY) + oy;
		w = *reinterpret_cast<int*>(menu + kMenuC_GlobalClipW);
		h = *reinterpret_cast<int*>(menu + kMenuC_GlobalClipH);
	} else {
		x = *reinterpret_cast<int*>(menu + kMenuC_FreeX) + ox;
		y = *reinterpret_cast<int*>(menu + kMenuC_FreeY) + oy;
		w = *reinterpret_cast<int*>(frame + kFrame_W);
		h = *reinterpret_cast<int*>(frame + kFrame_H);
	}
	if (set_clip_from_backdrop_open() && !s_intro_cinematic_active) {
		if (s_letterbox_menu_active && current_vid_w > 0 && current_vid_h > 0) {
			x = 0;
			y = 0;
			w = current_vid_w;
			h = current_vid_h;
		} else {
#if FEATURE_SCALED_MENU
			scaled_menu_backdrop_clip_anchor(stm, menu, &x, &y, &w, &h);
#endif
			if (s_clip_extend) s_clip_extend(&x, &w, menu);
		}
	}
	*outX = x;
	*outY = y;
	*outW = w;
	*outH = h;
	if (Draw_RenderRect) Draw_RenderRect(x, y, w, h);
}

using Ghoul_Draw_t = void(__thiscall*)(void* ghoul);
static Ghoul_Draw_t oGhoul_Draw = nullptr;

#if defined(__GNUC__)
__attribute__((thiscall))
#endif
static void hk_ghoul_draw(void* ghoul) {
	intro_ghoul_prepare_draw(ghoul);
#if defined(__GNUC__) && defined(__i386__)
	if (oGhoul_Draw) thiscall_ghoul_draw(ghoul, reinterpret_cast<void*>(oGhoul_Draw));
#else
	if (oGhoul_Draw) oGhoul_Draw(ghoul);
#endif
}

static bool install_one(void* rva, void* hook, void** orig, const char* name, size_t detour_len = 0) {
	return GetDetourSystem().ApplyDetourAtAddress(
		rvaToAbsExe(rva), hook, orig, name, detour_len);
}

} // namespace intro_scaler_detail

using namespace intro_scaler_detail;

void intro_scaler_set_clip_extend(void (*fn)(int* x, int* w, const void* menu_c)) {
	s_clip_extend = fn;
}

void intro_scaler_on_startup_menu_pushed(bool disclaimer, bool intro) {
	if (!intro_runtime_on()) return;
	if (!disclaimer && !intro) {
		letterbox_restore_canvas_free_y();
		s_letterbox_menu_active = false;
		s_intro_cinematic_active = false;
		s_intro_menu_screen = nullptr;
		s_intro_ghoul_layout_done = nullptr;
		return;
	}
	s_letterbox_menu_active = true;
	s_intro_cinematic_active = intro;
	if (intro) letterbox_restore_canvas_free_y();
}

bool intro_scaler_intro_cinematic_active(void) {
	return intro_runtime_on() && s_intro_cinematic_active;
}

bool intro_scaler_disclaimer_menu_text_active(void) {
	return intro_runtime_on() && s_letterbox_menu_active && !s_intro_cinematic_active;
}

float intro_scaler_page_width_scale(void) {
	return page_width_scale();
}

void intro_scaler_letterbox_apply_in_stm_set_clip(void* menu) {
	letterbox_apply_canvas_free_y(menu);
}

bool intro_scaler_install_hooks(void) {
	if (!intro_runtime_on()) return true;
	auto** rr = reinterpret_cast<void**>(rvaToAbsExe(reinterpret_cast<void*>(kRvaDrawRenderRectIat)));
	if (rr && *rr) Draw_RenderRect = reinterpret_cast<Draw_RenderRect_t>(*rr);
	const bool ok_stm = install_one(
		reinterpret_cast<void*>(kRvaMenuStmSetClip),
		reinterpret_cast<void*>(&hk_menu_stm_set_clip),
		nullptr,
		"intro_menu_stm_set_clip");
	const bool ok_ghoul = install_one(
		reinterpret_cast<void*>(kRvaGhoulDraw),
		reinterpret_cast<void*>(&hk_ghoul_draw),
		reinterpret_cast<void**>(&oGhoul_Draw),
		"intro_ghoul_draw");
	return ok_stm && ok_ghoul && Draw_RenderRect && oGhoul_Draw;
}

#endif // UI_MENU

#endif // FEATURE_INTRO_SCALER
