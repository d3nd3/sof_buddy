/*
	Menu scaling: content follows the vertical design scale (resY/480), frames keep the
	stock percent-of-V canvas scale (vid_w/640). Rect width/height/xoff/yoff are scaled
	once at RMF parse and stored, so drawing, layout and cursor hit-testing agree.

	Cvars:
	- _sofbuddy_menu_scale: menuScale (0.25..vid_h/480, 1 = vanilla). Values above the
	  resolution max use the max scale.
	- _sofbuddy_scale_round_auto / _sofbuddy_scale_round_ratio: optional scale snapping.

	See README.md for the full model, engine addresses and file-specific overrides.
*/

#include "feature_config.h"

#if FEATURE_SCALED_MENU

#ifdef UI_MENU

#include "sof_compat.h"
#include "features.h"
#include "util.h"
#include "detours.h"
#include "../scaled_ui_base/shared.h"
#include "shared.h"
#include "border_frame_dims.h"
#if FEATURE_INTRO_SCALER
#include "../intro_scaler/shared.h"
#endif
#include "runtime_features.h"
#include "generated_detours.h"

#include <cmath>
#include <cstdio>
#include <cctype>
#include <cstring>
#include <stdint.h>
#include <atomic>

float menuScale = 1.0f;

static char s_pushed_menu_name[64] = {0};
static bool s_join_scale_active = false;
static bool s_join_scale_pending = false;
static void* s_join_scale_menu = nullptr;
static bool s_menu_source_is_ss = false;
static bool s_menu_skip_frame_convert = false;
static bool s_page_is_resized = false;

static bool name_contains_ci(const char* s, const char* needle) {
	if (!s || !needle || !*needle) return false;
	for (const char* p = s; *p; ++p) {
		const char* a = p;
		const char* b = needle;
		while (*a && *b) {
			char ca = *a;
			char cb = *b;
			if (ca >= 'A' && ca <= 'Z') ca = static_cast<char>(ca - 'A' + 'a');
			if (cb >= 'A' && cb <= 'Z') cb = static_cast<char>(cb - 'A' + 'a');
			if (ca != cb) break;
			++a;
			++b;
		}
		if (!*b) return true;
	}
	return false;
}

static void* scaled_menu_current_menu(void) {
	auto* ms = *reinterpret_cast<void**>(rvaToAbsExe(reinterpret_cast<void*>(0x00406554)));
	return ms ? *reinterpret_cast<void**>(static_cast<char*>(ms) + 4) : nullptr;
}

static bool join_scale_active(void) {
	if (!s_join_scale_active) return false;
	if (s_join_scale_pending) return true;
	void* current = scaled_menu_current_menu();
	return current && current == s_join_scale_menu;
}

void scaled_menu_M_PushMenu_post(char const*, char const*, bool) {
	if (!s_join_scale_pending) return;
	if (void* current = scaled_menu_current_menu()) {
		s_join_scale_menu = current;
		s_join_scale_pending = false;
	}
}

void scaled_menu_set_pushed_menu(const char* name) {
	size_t i = 0;
	if (name) {
		for (; name[i] && i + 1 < sizeof(s_pushed_menu_name); ++i)
			s_pushed_menu_name[i] = name[i];
	}
	s_pushed_menu_name[i] = '\0';
	s_menu_source_is_ss = i >= 3 && s_pushed_menu_name[i - 3] == '_' &&
	                      s_pushed_menu_name[i - 2] == 's' &&
	                      s_pushed_menu_name[i - 1] == 's';
	s_menu_skip_frame_convert = false;
	s_page_is_resized = false;
	const bool is_join = std::strcmp(s_pushed_menu_name, "join") == 0 ||
	                     std::strcmp(s_pushed_menu_name, "join_ss") == 0 ||
	                     std::strcmp(s_pushed_menu_name, "join_ns") == 0;
	s_join_scale_active = is_join;
	s_join_scale_pending = is_join;
	s_join_scale_menu = nullptr;
}

void menuscale_change(cvar_t* cvar) {
	if (!cvar) return;
	const float maxScale = auto_scale_resolution_ratio();
	float v = cvar->value;
	const bool smooth_mode = scale_mode_is_smooth();
	if (g_applyingScalePercent || smooth_mode) {
		menuScale = (v > maxScale) ? maxScale : v;
		if (menuScale < 1.0f) menuScale = 1.0f;
	} else {
		if (v < 0.25f) v = 0.25f;
		if (v >= maxScale - 0.0001f) {
			menuScale = maxScale;
		} else {
			menuScale = round_scale_value(v);
			if (menuScale < 0.25f) menuScale = 0.25f;
			if (menuScale > maxScale) menuScale = maxScale;
			if (v >= 0.999f && v <= 1.001f) menuScale = 1.0f;
		}
	}
	menuScale = scale_mode_apply(menuScale);
	if (detour_Cvar_Set2::oCvar_Set2) {
		char buf[32];
		std::snprintf(buf, sizeof(buf), "%.2f", menuScale);
		detour_Cvar_Set2::oCvar_Set2(const_cast<char*>("_sb_internal_menu_scale_rounded"), buf, true);
	}
}

namespace scaled_menu_detail {

#if defined(__GNUC__) && defined(__i386__)
// MinGW does not always pass ECX for indirect __thiscall; game hooks must set it explicitly.
// MSVC thiscall callees use ret N — do not add esp after call (double-clean corrupts stack).
static void* thiscall_menu_find_frame(void* menu_c, const char* name, void* fn) {
	void* out = nullptr;
	if (!fn || !menu_c) return out;
	__asm__ __volatile__(
		"movl %1, %%ecx\n\t"
		"pushl %2\n\t"
		"call *%3\n\t"
		"movl %%eax, %0"
		: "=m"(out)
		: "m"(menu_c), "m"(name), "m"(fn)
		: "ecx", "eax", "edx", "memory");
	return out;
}

// sub_200C46D0(border, char* dimA, char* dimB, void* tint, float scale):
// [border+0] = atol(dimA) * scale, [border+4] = atol(dimB), [border+8] = *(int*)tint.
// scale is 1.0 (rect/actual-px tags) or the menu canvas scale (frame defaults).
static void* thiscall_border_dims_init(void* border, char* dimA, char* dimB, void* tint, float scale,
                                       void* fn) {
	void* ret = border;
	if (!fn) return ret;
	uint32_t sc_bits = 0;
	std::memcpy(&sc_bits, &scale, sizeof(sc_bits));
	__asm__ __volatile__(
		"pushl %5\n\t"
		"pushl %4\n\t"
		"pushl %3\n\t"
		"pushl %2\n\t"
		"movl %1, %%ecx\n\t"
		"call *%6\n\t"
		"movl %%eax, %0"
		: "=m"(ret)
		: "m"(border), "m"(dimA), "m"(dimB), "m"(tint), "m"(sc_bits), "m"(fn)
		: "ecx", "eax", "edx", "memory");
	return ret;
}

static const char* thiscall_token(void* token, int index, void* fn) {
	const char* out = nullptr;
	if (!fn || !token) return out;
	__asm__ __volatile__(
		"pushl %2\n\t"
		"movl %1, %%ecx\n\t"
		"call *%3\n\t"
		"movl %%eax, %0"
		: "=m"(out)
		: "m"(token), "m"(index), "m"(fn)
		: "ecx", "eax", "edx", "memory");
	return out;
}

static bool thiscall_rect_parse(void* rect, void* token, void* fn) {
	bool out = false;
	if (!fn) return out;
	__asm__ __volatile__(
		"pushl %2\n\t"
		"movl %1, %%ecx\n\t"
		"call *%3\n\t"
		"movb %%al, %0"
		: "=m"(out)
		: "m"(rect), "m"(token), "m"(fn)
		: "ecx", "eax", "edx", "memory");
	return out;
}

static bool thiscall_rect_get_coords(void* rect, int* x, int* y, void* fn) {
	bool out = false;
	if (!fn || !rect) return out;
	__asm__ __volatile__(
		"pushl %3\n\t"
		"pushl %2\n\t"
		"movl %1, %%ecx\n\t"
		"call *%4\n\t"
		"movb %%al, %0"
		: "=m"(out)
		: "m"(rect), "m"(x), "m"(y), "m"(fn)
		: "ecx", "eax", "edx", "memory");
	return out;
}

static void thiscall_noargs_void(void* self, void* fn) {
	if (!fn) return;
	__asm__ __volatile__(
		"movl %0, %%ecx\n\t"
		"call *%1"
		: : "m"(self), "m"(fn)
		: "ecx", "eax", "edx", "memory");
}

static int thiscall_noargs_int(void* self, void* fn) {
	int out = 0;
	if (!fn || !self) return out;
	__asm__ __volatile__(
		"movl %1, %%ecx\n\t"
		"call *%2\n\t"
		"movl %%eax, %0"
		: "=m"(out)
		: "m"(self), "m"(fn)
		: "ecx", "eax", "edx", "memory");
	return out;
}

static void thiscall_2args_void(void* self, void* a1, int a2, void* fn) {
	if (!fn) return;
	__asm__ __volatile__(
		"pushl %2\n\t"
		"pushl %1\n\t"
		"movl %0, %%ecx\n\t"
		"call *%3"
		: : "m"(self), "m"(a1), "m"(a2), "m"(fn)
		: "ecx", "eax", "edx", "memory");
}

static void thiscall_1arg_void(void* self, void* a1, void* fn) {
	if (!fn) return;
	__asm__ __volatile__(
		"pushl %1\n\t"
		"movl %0, %%ecx\n\t"
		"call *%2"
		: : "m"(self), "m"(a1), "m"(fn)
		: "ecx", "eax", "edx", "memory");
}


static void thiscall_rect_set_height(void* rect, int height, void* fn) {
	if (!fn || !rect) return;
	__asm__ __volatile__(
		"pushl %1\n\t"
		"movl %0, %%ecx\n\t"
		"call *%2"
		: : "m"(rect), "m"(height), "m"(fn)
		: "ecx", "edx", "memory");
}

#endif

constexpr uintptr_t kRvaMenuFindFrame = 0x000CA770u;
constexpr uintptr_t kRvaMenuStmSetClip = 0x000EA2C0u;
constexpr uintptr_t kRvaDrawRenderRectIat = 0x004035FCu;
constexpr uintptr_t kRvaBackdropSetClipOpen = 0x000C3F2Eu;
constexpr uintptr_t kRvaMenuBorderDimsInit = 0x000C46D0u;
constexpr uintptr_t kRvaRectParse = 0x000CF890u;
constexpr uintptr_t kRvaTickerInit = 0x000D1B30u;
constexpr uintptr_t kRvaRectGetCoords = 0x000CF570u;
constexpr uintptr_t kRvaRectHoverMark = 0x000CEB60u;
constexpr uintptr_t kRvaMenuCreateTip = 0x000CA9D0u;
constexpr uintptr_t kRvaTipRender = 0x000EA8A0u;
constexpr uintptr_t kRvaMenuSysGetMouseX = 0x000CBE80u;
constexpr uintptr_t kRvaMenuSysGetMouseY = 0x000CBE90u;
constexpr uintptr_t kRvaStmHoverPass = 0x000E9F30u;
constexpr uintptr_t kRvaRectHoverPrep = 0x000CEB40u;
constexpr uintptr_t kRvaFrameGetMouseX = 0x000C66A0u;
constexpr uintptr_t kRvaFrameGetMouseY = 0x000C66D0u;
constexpr uintptr_t kRvaToken = 0x000EB440u;
constexpr uintptr_t kRvaTextVtbl = 0x00112A40u;
constexpr uintptr_t kRvaInputVtbl = 0x001128C8u;
constexpr uintptr_t kRvaTickerVtbl = 0x00112808u;
constexpr uintptr_t kRvaGhoulVtbl = 0x001129E0u;
constexpr uintptr_t kRvaPicVtbl = 0x00112848u;
constexpr int kExtendCacheMax = 12;

constexpr ptrdiff_t kRect_MenuPtr = 0x04;
constexpr ptrdiff_t kRect_Ox = 0x0C;
constexpr ptrdiff_t kRect_Oy = 0x10;
constexpr ptrdiff_t kRect_Xoff = 0x14;
constexpr ptrdiff_t kRect_Yoff = 0x18;   // documented; not scaled (see dim fields)
constexpr ptrdiff_t kRect_WidgetScale = 0x1C;
constexpr ptrdiff_t kRect_W = 0x20;
constexpr ptrdiff_t kRect_H = 0x24;
constexpr ptrdiff_t kRect_OuterW = 0x28;
constexpr ptrdiff_t kRect_OuterH = 0x2C;
constexpr ptrdiff_t kRect_Border = 0x68;
constexpr ptrdiff_t kRect_NoScale = 0x66;
constexpr ptrdiff_t kRect_Bolted = 0x62;     // set by <bolt>/<bbolt>
constexpr ptrdiff_t kRect_VirtualX = 0x63;
constexpr ptrdiff_t kRect_VirtualY = 0x64;
constexpr ptrdiff_t kRect_BoltAnchor = 0x7C;
constexpr ptrdiff_t kRect_Align = 0x4C;      // 1=center 2=left 3=right (from layout state)
constexpr ptrdiff_t kBorder_ThickPx = 0x04;

constexpr ptrdiff_t kMenuC_ScreenScale = 0xAC;

using Draw_RenderRect_t = void(__cdecl*)(int x, int y, int w, int h);
static Draw_RenderRect_t Draw_RenderRect = nullptr;

constexpr ptrdiff_t kStm_GlobalPage = 0x89;
constexpr ptrdiff_t kMenuC_FreeX = 0x9C;
constexpr ptrdiff_t kMenuC_FreeY = 0xA0;
constexpr ptrdiff_t kMenuC_GlobalClipW = 0xA4;
constexpr ptrdiff_t kMenuC_GlobalClipH = 0xA8;
constexpr ptrdiff_t kMenuC_ResizeScreen = 0x8C;
constexpr ptrdiff_t kFrame_OffX = 0x40;
constexpr ptrdiff_t kFrame_OffY = 0x44;
constexpr ptrdiff_t kFrame_W = 0x48;
constexpr ptrdiff_t kFrame_H = 0x4C;
constexpr ptrdiff_t kFrame_NamePtr = 0x10;
constexpr ptrdiff_t kFrame_NameLen = 0x14;
constexpr ptrdiff_t kFrame_Menu = 0x00;
constexpr uintptr_t kRvaFrameResize = 0x000C6640u;
constexpr uintptr_t kRvaFrameFinalizeDims = 0x000C6500u;
constexpr uintptr_t kRvaFrameCtor = 0x000C60A0u;
constexpr uintptr_t kRvaFrameLink = 0x000C6530u;
constexpr uintptr_t kRvaTokeCtor = 0x000EAC70u;
constexpr uintptr_t kRvaTokeInsert = 0x000EAF60u;

static bool menu_c_is_resize_popup(const void* menu_c) {
	return menu_c && *reinterpret_cast<const uint8_t*>(static_cast<const char*>(menu_c) + kMenuC_ResizeScreen) != 0;
}

static bool menu_resize_canvas_is_scaled(const void* menu_c) {
	if (!menu_c || !menu_c_is_resize_popup(menu_c)) return false;
	const float canvas =
	    *reinterpret_cast<const float*>(static_cast<const char*>(menu_c) + kMenuC_ScreenScale);
	float max_expected = scaled_menu_screen_scale();
	if (screen_x_scale > max_expected) max_expected = screen_x_scale;
	if (screen_y_scale > max_expected) max_expected = screen_y_scale;
	return resize_canvas_scale_is_applied(canvas, max_expected);
}

// <text> labels are the widgets whose RMF xoff/yoff are hand-tuned centring offsets.
static bool widget_is_text(const void* widget) {
	if (!widget) return false;
	const void* vt = *static_cast<const void* const*>(widget);
	return vt == rvaToAbsExe(reinterpret_cast<void*>(kRvaTextVtbl));
}

// <input> sizes itself from measurement (textW/textH × scale + borders), like <text>.
static bool widget_is_input(const void* widget) {
	if (!widget) return false;
	const void* vt = *static_cast<const void* const*>(widget);
	return vt == rvaToAbsExe(reinterpret_cast<void*>(kRvaInputVtbl));
}

// <pic>/<image> dimensions are actual pixels; resize pages need their authored
// dimensions scaled so hit testing follows the scaled art.
static bool widget_is_pic(const void* widget) {
	if (!widget) return false;
	const void* vt = *static_cast<const void* const*>(widget);
	return vt == rvaToAbsExe(reinterpret_cast<void*>(kRvaPicVtbl));
}

// <ticker> sets its band height from R_StrHeight, which we already scale.
static bool widget_is_ticker(const void* widget) {
	if (!widget) return false;
	const void* vt = *static_cast<const void* const*>(widget);
	return vt == rvaToAbsExe(reinterpret_cast<void*>(kRvaTickerVtbl));
}

// <ghoul>/<bghoul> are staged by the engine (and intro_scaler for the intro). Their
// xoff/yoff/model scale live in the canvas/art space, not the menuScale space, so
// scaling the rect displaces the whole staged scene (e.g. the amain background
// ghoul drifts left/up by (canvasScale - menuScale) * offset).
static bool widget_is_ghoul(const void* widget) {
	if (!widget) return false;
	const void* vt = *static_cast<const void* const*>(widget);
	return vt == rvaToAbsExe(reinterpret_cast<void*>(kRvaGhoulVtbl));
}

static bool menu_actual_pixel_scale_active(const void* menu_c) {
	if (!RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_SCALED_MENU)) return false;
	if (!scaled_menu_uniform_actual_active()) return false;
	if (!menu_c) return false;
	// Resize pages are pinned to actual pixels; hk_frame_resize raises their canvas to
	// menuScale so the page itself carries the scale, and scaling the contents again would
	// overflow. Only skip the per-widget factor when the canvas really holds such a raised
	// scale. A re-parsed/reused menu tree can keep the +0x8C flag with a zeroed/1.0 canvas,
	// in which case the per-widget factor is the only scaling the contents get.
	if (menu_resize_canvas_is_scaled(menu_c)) return false;
	return true;
}

// rect+0x04 → stm; stm+0 → frame; frame+0 → menu screen (GetCoords 0xCF5DB).
static void* menu_outer_from_widget(void* widget) {
	if (!widget) return nullptr;
	auto* stm = *reinterpret_cast<void**>(static_cast<char*>(widget) + kRect_MenuPtr);
	if (!stm) return nullptr;
	auto* frame = *reinterpret_cast<void**>(stm);
	return frame ? *reinterpret_cast<void**>(frame) : nullptr;
}

static float menu_actual_rect_dim_factor(void* rect) {
	if (!scaled_menu_uniform_actual_active()) return 1.f;
	if (!rect) return 1.f;
	if (*reinterpret_cast<uint8_t*>(static_cast<char*>(rect) + kRect_NoScale)) return 1.f;
	if (widget_is_ghoul(rect)) return 1.f;
	void* menu = menu_outer_from_widget(rect);
	if (!menu_actual_pixel_scale_active(menu)) {
		// Resize pages carry the page scale in their canvas, but text/input boxes
		// keep authored widths unless their fixed dimensions get the normal factor.
		if (!(widget_is_text(rect) || widget_is_input(rect)))
			return 1.f;
	}
	const float v = *reinterpret_cast<float*>(static_cast<char*>(rect) + kRect_WidgetScale);
	if (v <= 1e-6f || !std::isfinite(v)) return 1.f;
	const float s = scaled_menu_screen_scale();
	if (!std::isfinite(s) || s <= 1e-6f) return 1.f;
	return s / v;
}

static void* menu_find_default_frame(void* menu_c);

using Menu_FindFrame_t = void*(__thiscall*)(void* menu_c, const char* name);

static void* menu_find_default_frame(void* menu_c) {
	if (!menu_c) return nullptr;
	auto find_frame = reinterpret_cast<Menu_FindFrame_t>(
		rvaToAbsExe(reinterpret_cast<void*>(kRvaMenuFindFrame)));
	if (!find_frame) return nullptr;
#if defined(__GNUC__) && defined(__i386__)
	return thiscall_menu_find_frame(menu_c, "default", reinterpret_cast<void*>(find_frame));
#else
	return find_frame(menu_c, "default");
#endif
}

using Menu_BorderDimsInit_t = void*(__thiscall*)(void* border, char* pad, char* thick, void* menu_c, float scale);
static Menu_BorderDimsInit_t oMenu_BorderDimsInit = nullptr;

static bool scaled_menu_skip_for_intro_disclaimer(void) {
#if FEATURE_INTRO_SCALER
	return intro_scaler_disclaimer_menu_text_active() || intro_scaler_intro_cinematic_active();
#else
	return false;
#endif
}

static float border_dims_init_use_scale(float scale, bool active, bool frame_border) {
	if (!active) return 1.f;
	if (frame_border) {
		// Frame borders use the same virtual authoring space as frame W/H.
		// Apply the vertical virtual-space correction only, not the manual menu scale.
		return (current_vid_h > 0) ? (static_cast<float>(current_vid_h) / 480.0f) : 1.f;
	}
	(void)scale;
	return scaled_menu_screen_scale();
}

// sub_200C46D0 is __thiscall (this in ECX + 4 stack args, callee pops 0x10):
// hk must carry __attribute__((thiscall)) so ECX/slots line up when JMP'd into.
#if defined(__GNUC__)
__attribute__((thiscall))
#endif
static void* hk_menu_border_dims_init(void* border, char* dimA, char* dimB, void* tint, float scale) {
	bool active = scaled_menu_uniform_actual_active();
	if (active && scaled_menu_skip_for_intro_disclaimer()) active = false;
	const bool frame_border = std::fabs(scale - 1.0f) > 0.001f;
	if (!active) {
#if defined(__GNUC__) && defined(__i386__)
		return oMenu_BorderDimsInit
		           ? thiscall_border_dims_init(border, dimA, dimB, tint, scale,
		                                       reinterpret_cast<void*>(oMenu_BorderDimsInit))
		           : border;
#else
		return oMenu_BorderDimsInit ? oMenu_BorderDimsInit(border, dimA, dimB, tint, scale) : border;
#endif
	}
	const float use = border_dims_init_use_scale(scale, active, frame_border);
#if defined(__GNUC__) && defined(__i386__)
	void* ret = oMenu_BorderDimsInit
	                ? thiscall_border_dims_init(border, dimA, dimB, tint, use,
	                                            reinterpret_cast<void*>(oMenu_BorderDimsInit))
	                : border;
#else
	void* ret = oMenu_BorderDimsInit ? oMenu_BorderDimsInit(border, dimA, dimB, tint, use) : border;
#endif
	if (border && !frame_border) {
		const float ms = scaled_menu_screen_scale();
		// [border+4] is the raw (actual-pixel) thickness the stock call site never scales.
		int& t = *reinterpret_cast<int*>(static_cast<char*>(border) + kBorder_ThickPx);
		if (t > 0) t = static_cast<int>(std::lround(static_cast<float>(t) * ms));
	}
	return ret;
}

using Rect_Parse_t = bool(__thiscall*)(void* rect, void* token);
static Rect_Parse_t oRect_Parse = nullptr;

static void scale_widget_dims(void* widget, const ptrdiff_t* fields, size_t count, float factor) {
	if (!widget || !std::isfinite(factor) || std::fabs(factor - 1.f) <= 0.001f) return;
	char* r = static_cast<char*>(widget);
	for (size_t i = 0; i < count; ++i) {
		int& v = *reinterpret_cast<int*>(r + fields[i]);
		if (v > 0) v = static_cast<int>(std::lround(static_cast<float>(v) * factor));
	}
}

#if defined(__GNUC__)
__attribute__((thiscall))
#endif
static bool hk_rect_parse(void* rect, void* token) {
	const char* keyword = nullptr;
#if defined(__GNUC__) && defined(__i386__)
	keyword = thiscall_token(token, 0, rvaToAbsExe(reinterpret_cast<void*>(kRvaToken)));
#endif
	const bool fixed_width = keyword && std::strcmp(keyword, "width") == 0;
	const bool fixed_height = keyword && std::strcmp(keyword, "height") == 0;
	const bool fixed_xoff = keyword && std::strcmp(keyword, "xoff") == 0;
	const bool fixed_yoff = keyword && std::strcmp(keyword, "yoff") == 0;
#if defined(__GNUC__) && defined(__i386__)
	const bool parsed = oRect_Parse
		                    ? thiscall_rect_parse(rect, token, reinterpret_cast<void*>(oRect_Parse))
		                    : false;
#else
	const bool parsed = oRect_Parse ? oRect_Parse(rect, token) : false;
#endif
	if (!parsed || (!fixed_width && !fixed_height && !fixed_xoff && !fixed_yoff))
		return parsed;
	char* r = static_cast<char*>(rect);
	if (!scaled_menu_uniform_actual_active() || scaled_menu_skip_for_intro_disclaimer() ||
	    *reinterpret_cast<uint8_t*>(r + kRect_NoScale)) {
		return parsed;
	}
	float factor = menu_actual_rect_dim_factor(rect);
	if (widget_is_ticker(rect)) {
		// Tickers scroll inside canvas/art-space panels (usually a ghoul part), so their
		// offsets follow the canvas scale, not menuScale. Keep the authored width: the
		// ticker init applies the canvas scale to W the way the engine does for bolted ones.
		if (fixed_width) {
			factor = 1.f;
		} else {
			char* menu = static_cast<char*>(menu_outer_from_widget(rect));
			float canvas = menu ? *reinterpret_cast<float*>(menu + kMenuC_ScreenScale) : 1.f;
			float ws = *reinterpret_cast<float*>(r + kRect_WidgetScale);
			if (!std::isfinite(canvas) || canvas <= 1e-6f) canvas = 1.f;
			if (!std::isfinite(ws) || ws <= 1e-6f) ws = 1.f;
			factor = canvas / ws;
		}
	}
	if (std::fabs(factor - 1.f) <= 0.001f) return parsed;
	if (fixed_width || fixed_height) {
		const ptrdiff_t value_offset = fixed_width ? kRect_W : kRect_H;
		const ptrdiff_t outer_offset = fixed_width ? kRect_OuterW : kRect_OuterH;
		int& value = *reinterpret_cast<int*>(r + value_offset);
		int& outer = *reinterpret_cast<int*>(r + outer_offset);
		void* border = *reinterpret_cast<void**>(r + kRect_Border);
		const int padding = border ? *reinterpret_cast<int*>(static_cast<char*>(border)) : 0;
		const float widget_scale = *reinterpret_cast<float*>(r + kRect_WidgetScale);
		const auto scaled = scale_fixed_rect_dimension(value, widget_scale, padding, factor);
		value = scaled.value;
		outer = scaled.outer;
	} else {
		int& px = *reinterpret_cast<int*>(r + (fixed_xoff ? kRect_Xoff : kRect_Yoff));
		if (px != 0) {
			char* menu = static_cast<char*>(menu_outer_from_widget(rect));
			float canvas = menu ? *reinterpret_cast<float*>(menu + kMenuC_ScreenScale) : 1.f;
			if (!std::isfinite(canvas) || canvas <= 1e-6f) canvas = 1.f;
			float f = factor;
			bool keep_authored = false;
			if (widget_is_ticker(rect)) {
				// Ticker x lives on the canvas/art-space panel, so it follows the canvas.
				// The y nudge is a pixel positioning offset authored against the band: it
				// must stay vanilla (scaling it lifted the tape ~(menuScale-1)*|yoff| px,
				// 13 px at 1080p, which is the visible "few pixels too high").
				float ws = *reinterpret_cast<float*>(r + kRect_WidgetScale);
				if (!std::isfinite(ws) || ws <= 1e-6f) ws = 1.f;
				if (fixed_xoff) {
					f = canvas / ws;
				} else {
					keep_authored = true;
				}
			}
			const bool hard_path =
			    *reinterpret_cast<int*>(r + kRect_Align) == 2 ||
			    *reinterpret_cast<int*>(r + kRect_Align) == 3 ||
			    *reinterpret_cast<void**>(r + kRect_BoltAnchor) != nullptr;
			const uint8_t virtual_axis =
			    *reinterpret_cast<uint8_t*>(r + (fixed_xoff ? kRect_VirtualX : kRect_VirtualY));
			// The engine multiplies virtual offsets by the canvas itself in the hard path
			// (sub_200CF350), so divide it out here to avoid double scaling.
			if (!keep_authored && hard_path && virtual_axis) f /= canvas;
			if (!keep_authored && std::fabs(f - 1.f) > 0.001f)
				px = static_cast<int>(std::lround(static_cast<float>(px) * f));
		}
	}
	return parsed;
}

// frame_c::Resize(pad, thick) — <stm resize W H> support.
// Stock pins the page to W×H actual pixels, centres it via (vid - W)/2, and forces
// canvas scale (menu_c+0xAC) to 1.0. Left alone at high resolution the dialog renders
// tiny in the middle of the screen. Scale the page canvas and its default frame box by
// the same factor, then centre that actual-pixel size.
using Frame_Resize_t = void(__thiscall*)(void* frame, char* w, char* h);
static Frame_Resize_t oFrame_Resize = nullptr;

#if defined(__GNUC__)
__attribute__((thiscall))
#endif
static void hk_frame_resize(void* frame, char* w, char* h) {
	if (!frame || !oFrame_Resize) return;
#if defined(__GNUC__) && defined(__i386__)
	// thiscall via explicit ECX (MinGW does not reliably pass it for indirect calls).
	__asm__ __volatile__(
		"pushl %2\n\t"
		"pushl %1\n\t"
		"movl %0, %%ecx\n\t"
		"call *%3"
		: : "m"(frame), "m"(w), "m"(h), "m"(oFrame_Resize)
		: "ecx", "eax", "edx", "memory");
#else
	oFrame_Resize(frame, w, h);
#endif
	if (!scaled_menu_uniform_actual_active() || scaled_menu_skip_for_intro_disclaimer()) return;
	const float s = scaled_menu_screen_scale();
	if (!std::isfinite(s) || std::fabs(s - 1.f) < 0.001f) return;
	char* f = static_cast<char*>(frame);
	// Stock pinned this page to W×H actual pixels with canvas scale 1.0. Scale both the
	// canvas (contents) and the default frame box, then recentre the resulting size.
	if (char* menu = *reinterpret_cast<char**>(f + kFrame_Menu)) {
		*reinterpret_cast<float*>(menu + kMenuC_ScreenScale) = s;
	}
	s_page_is_resized = true;
	// W/H are authored in virtual units. The canvas scales the contents, so scale the
	// default frame box too and centre the resulting actual-pixel size.
	int& fw = *reinterpret_cast<int*>(f + kFrame_W);
	int& fh = *reinterpret_cast<int*>(f + kFrame_H);
	const int scaled_w = static_cast<int>(std::lround(static_cast<float>(fw) * s));
	const int scaled_h = static_cast<int>(std::lround(static_cast<float>(fh) * s));
	fw = scaled_w;
	fh = scaled_h;
	*reinterpret_cast<int*>(f + kFrame_OffX) =
	    (current_vid_w - scaled_w) / 2;
	*reinterpret_cast<int*>(f + kFrame_OffY) =
	    (current_vid_h - scaled_h) / 2;
}

using Frame_FinalizeDims_t = void(__thiscall*)(void* frame);
static Frame_FinalizeDims_t oFrame_FinalizeDims = nullptr;

static void thiscall_frame_finalize_dims(void* frame, void* fn) {
#if defined(__GNUC__) && defined(__i386__)
	if (!fn) return;
	__asm__ __volatile__(
		"movl %0, %%ecx\n\t"
		"call *%1"
		: : "r"(frame), "r"(fn)
		: "ecx", "eax", "edx", "memory");
#else
	if (fn) reinterpret_cast<Frame_FinalizeDims_t>(fn)(frame);
#endif
}

#if defined(__GNUC__)
__attribute__((thiscall))
#endif
static void hk_frame_finalize_dims(void* frame) {
	thiscall_frame_finalize_dims(frame, reinterpret_cast<void*>(oFrame_FinalizeDims));
}

// Non-_ss menus author their frame boxes in 480p coordinates. The stock ctor multiplies
// them by the canvas (vid_w/640), which overflows a 16:9 screen; convert them with the
// vertical factor (resY/480) instead. _ss (and _ns is 480p-authored, so it converts)
// menus that are already authored for the wide layout keep the stock canvas scaling.
static bool tail_equals_ci(const char* s, size_t len, const char* suffix) {
	const size_t slen = std::strlen(suffix);
	if (len < slen) return false;
	for (size_t i = 0; i < slen; ++i) {
		char a = s[len - slen + i];
		char b = suffix[i];
		if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
		if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
		if (a != b) return false;
	}
	return true;
}

static void menu_source_note_name(const char* name, bool reset_if_unknown) {
	if (!name || !*name) return;
	const char* base = name;
	for (const char* p = name; *p; ++p) {
		if (*p == '/' || *p == '\\') base = p + 1;
	}
	if (name_contains_ci(name, "sof_buddy/") || name_contains_ci(name, "loading/"))
		s_menu_skip_frame_convert = true;
	size_t len = std::strlen(base);
	if (len >= 4 && tail_equals_ci(base, len, ".rmf")) len -= 4;
	if (tail_equals_ci(base, len, "_ss")) {
		s_menu_source_is_ss = true;
	} else if (tail_equals_ci(base, len, "_ns")) {
		s_menu_source_is_ss = false;
	} else if (reset_if_unknown) {
		s_menu_source_is_ss = false;
	}
}

struct FrameDimsRec {
	void* menu;
	void* frame;
	int w;
	int h;
	bool ss;
};

static FrameDimsRec s_frame_dims[128];
static int s_frame_dims_count = 0;
static int s_frame_dims_rr = 0;

static FrameDimsRec* frame_dims_find(void* menu, void* frame) {
	if (!menu || !frame) return nullptr;
	for (int i = 0; i < s_frame_dims_count; ++i) {
		if (s_frame_dims[i].menu == menu && s_frame_dims[i].frame == frame)
			return &s_frame_dims[i];
	}
	return nullptr;
}

static FrameDimsRec* frame_dims_store(void* menu, void* frame) {
	if (!menu || !frame) return nullptr;
	FrameDimsRec* rec = frame_dims_find(menu, frame);
	if (rec) return rec;
	if (s_frame_dims_count < static_cast<int>(sizeof(s_frame_dims) / sizeof(s_frame_dims[0]))) {
		rec = &s_frame_dims[s_frame_dims_count++];
	} else {
		rec = &s_frame_dims[s_frame_dims_rr];
		s_frame_dims_rr = (s_frame_dims_rr + 1) %
		                  static_cast<int>(sizeof(s_frame_dims) / sizeof(s_frame_dims[0]));
	}
	rec->menu = menu;
	rec->frame = frame;
	rec->w = 0;
	rec->h = 0;
	rec->ss = false;
	return rec;
}

using Frame_Ctor_t = void*(__thiscall*)(void* frame, void* menu, const char* w, const char* h,
                                        const char* name);
static Frame_Ctor_t oFrame_Ctor = nullptr;

#if defined(__GNUC__)
__attribute__((thiscall))
#endif
static void* hk_frame_ctor(void* frame, void* menu, const char* w, const char* h,
                           const char* name) {
	void* ret = oFrame_Ctor ? oFrame_Ctor(frame, menu, w, h, name) : frame;
	if (!frame) return ret;
	char* f = static_cast<char*>(frame);
	void* owner = *reinterpret_cast<void**>(f + kFrame_Menu);
	if (FrameDimsRec* rec = frame_dims_store(owner ? owner : menu, frame)) {
		rec->w = *reinterpret_cast<int*>(f + kFrame_W);
		rec->h = *reinterpret_cast<int*>(f + kFrame_H);
		rec->ss = s_menu_source_is_ss || s_menu_skip_frame_convert;
	}
	return ret;
}

using Frame_Link_t = void(__thiscall*)(void* child, void* parent);
static Frame_Link_t oFrame_Link = nullptr;

#if defined(__GNUC__)
__attribute__((thiscall))
#endif
static void hk_frame_link(void* child, void* parent) {
	FrameDimsRec* cr = nullptr;
	if (child) {
		char* c = static_cast<char*>(child);
		void* menu = *reinterpret_cast<void**>(c + kFrame_Menu);
		cr = frame_dims_find(menu, child);
		FrameDimsRec* pr = parent ? frame_dims_find(menu, parent) : nullptr;
		const bool ss = (cr && cr->ss) || (pr && pr->ss);
		if (cr) {
			cr->ss = ss;
			if (!ss && menu && scaled_menu_uniform_actual_active() &&
			    !scaled_menu_skip_for_intro_disclaimer()) {
				const float canvas =
				    *reinterpret_cast<float*>(static_cast<char*>(menu) + kMenuC_ScreenScale);
				const float target = scaled_menu_screen_scale();
				if (std::isfinite(canvas) && canvas > 1e-6f && std::isfinite(target) &&
				    target > 0.f) {
					const float factor = target / canvas;
					if (std::fabs(factor - 1.f) > 0.001f) {
						int& fw = *reinterpret_cast<int*>(c + kFrame_W);
						int& fh = *reinterpret_cast<int*>(c + kFrame_H);
						if (cr->w != 0)
							fw = static_cast<int>(
							    std::lround(static_cast<float>(cr->w) * factor));
						if (cr->h != 0)
							fh = static_cast<int>(
							    std::lround(static_cast<float>(cr->h) * factor));
					}
				}
			}
		}
	}
	if (oFrame_Link) oFrame_Link(child, parent);
}

using Toke_Ctor_t = void*(__thiscall*)(void* toke, const char* name);
static Toke_Ctor_t oToke_Ctor = nullptr;

#if defined(__GNUC__)
__attribute__((thiscall))
#endif
static void* hk_toke_ctor(void* toke, const char* name) {
	menu_source_note_name(name, false);
	return oToke_Ctor ? oToke_Ctor(toke, name) : toke;
}

using Toke_Insert_t = void(__thiscall*)(void* toke, const char* name);
static Toke_Insert_t oToke_Insert = nullptr;

#if defined(__GNUC__)
__attribute__((thiscall))
#endif
static void hk_toke_insert(void* toke, const char* name) {
	menu_source_note_name(name, false);
	if (oToke_Insert) oToke_Insert(toke, name);
}

using Ticker_Init_t = void(__thiscall*)(void* ticker);
static Ticker_Init_t oTicker_Init = nullptr;

static void thiscall_ticker_init(void* ticker, void* fn) {
#if defined(__GNUC__) && defined(__i386__)
	if (!fn) return;
	__asm__ __volatile__(
		"movl %0, %%ecx\n\t"
		"call *%1"
		: : "r"(ticker), "r"(fn)
		: "ecx", "eax", "edx", "memory");
#else
	if (fn) reinterpret_cast<Ticker_Init_t>(fn)(ticker);
#endif
}

#if defined(__GNUC__)
__attribute__((thiscall))
#endif
static void hk_ticker_init(void* ticker) {
	bool restore_delay = false;
	float authored_delay = 0.f;
	float speed_scale = 1.f;
	if (ticker && scaled_menu_uniform_actual_active() && !scaled_menu_skip_for_intro_disclaimer()) {
		char* t = static_cast<char*>(ticker);
		if (!*reinterpret_cast<uint8_t*>(t + kRect_NoScale)) {
			// Tickers live in the canvas/art space (they are usually bolted to a ghoul part
			// drawn at canvas scale), while their text width comes from R_StrLen at
			// menuScale. The engine only multiplies W by the canvas for bolted tickers;
			// apply the same canvas scale ourselves when it does not.
			char* menu = static_cast<char*>(menu_outer_from_widget(ticker));
			const float canvas = menu ? *reinterpret_cast<float*>(menu + kMenuC_ScreenScale) : 1.f;
			const bool engine_scales =
			    *reinterpret_cast<void**>(t + kRect_BoltAnchor) != nullptr;
			if (!engine_scales && std::isfinite(canvas) && canvas > 1e-6f &&
			    std::fabs(canvas - 1.f) > 0.001f) {
				int& w = *reinterpret_cast<int*>(t + kRect_W);
				if (w > 0) w = static_cast<int>(std::lround(static_cast<float>(w) * canvas));
				int& ow = *reinterpret_cast<int*>(t + kRect_OuterW);
				if (ow > 0) ow = static_cast<int>(std::lround(static_cast<float>(ow) * canvas));
			}
			if (engine_scales) {
				authored_delay = *reinterpret_cast<float*>(t + 0xB8);
				restore_delay = std::isfinite(authored_delay);
			}
			const float height_scale = screen_y_scale;
			if (std::isfinite(height_scale) && height_scale > 1e-6f)
				speed_scale = height_scale;
			// The authored yoff keeps vanilla's baseline, but the glyphs grow around the
			// run start by (scale-1)*glyphH. Shift by half that growth in the direction of
			// the authored nudge so the scaled tape keeps vanilla's visual centre.
			const float s = scaled_menu_screen_scale();
			if (std::isfinite(s) && s > 1.001f) {
				const char* font = t + 0x8C;
				const char* fontName = *reinterpret_cast<char* const*>(const_cast<char*>(font) + 4);
				const int glyph_h = font_glyph_px(
				    fontName ? getRealFontEnum(fontName) : REALFONT_TITLE);
				int& yo = *reinterpret_cast<int*>(t + kRect_Yoff);
				if (glyph_h > 0 && yo != 0) {
					const int delta = static_cast<int>(
					    std::lround(static_cast<float>(glyph_h) * (s - 1.f) * 0.5f));
					yo += (yo < 0) ? -delta : delta;
				}
			}
		}
	}
	thiscall_ticker_init(ticker, reinterpret_cast<void*>(oTicker_Init));
	if (restore_delay)
		*reinterpret_cast<float*>(static_cast<char*>(ticker) + 0xB8) = authored_delay;
	if (ticker && std::fabs(speed_scale - 1.f) > 0.001f) {
		int& speed = *reinterpret_cast<int*>(static_cast<char*>(ticker) + 0xAC);
		if (speed != 0)
			speed = static_cast<int>(std::lround(static_cast<float>(speed) * speed_scale));
	}
}

using Menu_CreateTip_t = void(__thiscall*)(void* menu, void* rect, int stm_y);
static Menu_CreateTip_t oMenu_CreateTip = nullptr;

using Tip_Render_t = void(__thiscall*)(void* tip, void* menu);
static Tip_Render_t oTip_Render = nullptr;


using Rect_HoverPrep_t = void(__thiscall*)(void* rect);
static Rect_HoverPrep_t oRect_HoverPrep = nullptr;

using Stm_HoverPass_t = void(__thiscall*)(void* stm);
static Stm_HoverPass_t oStm_HoverPass = nullptr;

struct HoverSave {
	void* rect;
	int ox;
	int oy;
	int w, h, ow, oh;
	bool pic_scaled;
};
static HoverSave s_hover_saved[512];
static int s_hover_saved_count = 0;
static bool s_in_hover_pass = false;

constexpr ptrdiff_t kMenu_HoveredRect = 0xBC;

struct TipSource {
	void* menu;
	void* tip;
	void* rect;
};
static TipSource s_tip_sources[64];
static int s_tip_source_next = 0;

static void tip_source_store(void* menu, void* tip, void* rect) {
	if (!menu || !tip || !rect) return;
	const int count = static_cast<int>(sizeof(s_tip_sources) / sizeof(s_tip_sources[0]));
	for (int i = 0; i < count; ++i) {
		if (s_tip_sources[i].menu == menu && s_tip_sources[i].tip == tip) {
			s_tip_sources[i].rect = rect;
			return;
		}
	}
	TipSource& entry = s_tip_sources[s_tip_source_next++ % count];
	entry.menu = menu;
	entry.tip = tip;
	entry.rect = rect;
}

static void* tip_source_lookup(void* menu, void* tip) {
	if (!menu || !tip) return nullptr;
	const int count = static_cast<int>(sizeof(s_tip_sources) / sizeof(s_tip_sources[0]));
	for (int i = 0; i < count; ++i) {
		if (s_tip_sources[i].menu == menu && s_tip_sources[i].tip == tip)
			return s_tip_sources[i].rect;
	}
	return nullptr;
}

static void hover_save_and_adjust(void* rect) {
	if (s_hover_saved_count >= static_cast<int>(sizeof(s_hover_saved) / sizeof(s_hover_saved[0]))) return;
	HoverSave& sv = s_hover_saved[s_hover_saved_count++];
	sv.rect = rect;
	char* r = static_cast<char*>(rect);
	sv.ox = *reinterpret_cast<int*>(r + kRect_Ox);
	sv.oy = *reinterpret_cast<int*>(r + kRect_Oy);
	sv.pic_scaled = false;
	void* stm = *reinterpret_cast<void**>(static_cast<char*>(rect) + kRect_MenuPtr);
	void* frame = stm ? *reinterpret_cast<void**>(stm) : nullptr;
	void* menu = frame ? *reinterpret_cast<void**>(frame) : nullptr;
	void* ms = menu ? *reinterpret_cast<void**>(static_cast<char*>(menu) + 0x14) : nullptr;
	void* gc = rvaToAbsExe(reinterpret_cast<void*>(kRvaRectGetCoords));
	void* fmx = rvaToAbsExe(reinterpret_cast<void*>(kRvaFrameGetMouseX));
	void* fmy = rvaToAbsExe(reinterpret_cast<void*>(kRvaFrameGetMouseY));
	void* smx = rvaToAbsExe(reinterpret_cast<void*>(kRvaMenuSysGetMouseX));
	void* smy = rvaToAbsExe(reinterpret_cast<void*>(kRvaMenuSysGetMouseY));
	if (!stm || !frame || !menu || !ms || !gc || !fmx || !fmy || !smx || !smy) return;
	int x = 0, y = 0;
	if (!thiscall_rect_get_coords(rect, &x, &y, gc)) return;
	const int stm_y = *reinterpret_cast<int*>(static_cast<char*>(stm) + 0x80);
	const int kx = thiscall_noargs_int(ms, smx) - thiscall_noargs_int(frame, fmx);
	const int ky = thiscall_noargs_int(ms, smy) -
	               (thiscall_noargs_int(frame, fmy) + stm_y);
	*reinterpret_cast<int*>(r + kRect_Ox) = x - kx;
	*reinterpret_cast<int*>(r + kRect_Oy) = y - ky;
	if (s_page_is_resized && widget_is_pic(rect) &&
	    !*reinterpret_cast<uint8_t*>(r + kRect_NoScale)) {
		sv.w = *reinterpret_cast<int*>(r + kRect_W);
		sv.h = *reinterpret_cast<int*>(r + kRect_H);
		sv.ow = *reinterpret_cast<int*>(r + kRect_OuterW);
		sv.oh = *reinterpret_cast<int*>(r + kRect_OuterH);
		static constexpr ptrdiff_t fields[] = {kRect_W, kRect_H, kRect_OuterW, kRect_OuterH};
		scale_widget_dims(rect, fields, sizeof(fields) / sizeof(fields[0]),
		                  scaled_menu_screen_scale());
		sv.pic_scaled = true;
	}
}

#if defined(__GNUC__)
__attribute__((thiscall))
#endif
static void hk_rect_hover_prep(void* rect) {
	if (rect && s_in_hover_pass && scaled_menu_uniform_actual_active() &&
	    !scaled_menu_skip_for_intro_disclaimer()) {
		hover_save_and_adjust(rect);
	}
	if (oRect_HoverPrep) {
#if defined(__GNUC__) && defined(__i386__)
		thiscall_noargs_void(rect, reinterpret_cast<void*>(oRect_HoverPrep));
#else
		oRect_HoverPrep(rect);
#endif
	}
}

#if defined(__GNUC__)
__attribute__((thiscall))
#endif
static void hk_stm_hover_pass(void* stm) {
	s_hover_saved_count = 0;
	s_in_hover_pass = true;
	if (oStm_HoverPass) {
#if defined(__GNUC__) && defined(__i386__)
		thiscall_noargs_void(stm, reinterpret_cast<void*>(oStm_HoverPass));
#else
		oStm_HoverPass(stm);
#endif
	}
	s_in_hover_pass = false;
	for (int i = 0; i < s_hover_saved_count; ++i) {
		HoverSave& sv = s_hover_saved[i];
		char* r = static_cast<char*>(sv.rect);
		*reinterpret_cast<int*>(r + kRect_Ox) = sv.ox;
		*reinterpret_cast<int*>(r + kRect_Oy) = sv.oy;
		if (sv.pic_scaled) {
			*reinterpret_cast<int*>(r + kRect_W) = sv.w;
			*reinterpret_cast<int*>(r + kRect_H) = sv.h;
			*reinterpret_cast<int*>(r + kRect_OuterW) = sv.ow;
			*reinterpret_cast<int*>(r + kRect_OuterH) = sv.oh;
		}
	}
	s_hover_saved_count = 0;
}

#if defined(__GNUC__)
__attribute__((thiscall))
#endif
static void hk_menu_create_tip(void* menu, void* rect, int stm_y) {
	if (oMenu_CreateTip) {
#if defined(__GNUC__) && defined(__i386__)
		thiscall_2args_void(menu, rect, stm_y, reinterpret_cast<void*>(oMenu_CreateTip));
#else
		oMenu_CreateTip(menu, rect, stm_y);
#endif
	}
	if (!menu || !rect) return;
	char* tip = *reinterpret_cast<char**>(static_cast<char*>(menu) + 0x5C);
	if (!tip) return;
	tip_source_store(menu, tip, rect);
	if (_sofbuddy_menu_tip_appear) {
		int appear = static_cast<int>(_sofbuddy_menu_tip_appear->value);
		if (appear < 0) appear = 0;
		const int duration = *reinterpret_cast<int*>(tip + 0x1C);
		if (duration > 0) {
			const int gate = duration / 4;
			const int created = *reinterpret_cast<int*>(tip + 0x18);
			*reinterpret_cast<int*>(tip + 0x18) = created - gate + appear;
		}
	}
	if (!scaled_menu_uniform_actual_active() || scaled_menu_skip_for_intro_disclaimer()) return;
	void* gc = rvaToAbsExe(reinterpret_cast<void*>(kRvaRectGetCoords));
	if (!gc) return;
	int x = 0, y = 0;
	if (!thiscall_rect_get_coords(rect, &x, &y, gc)) return;
	const int oh = *reinterpret_cast<int*>(static_cast<char*>(rect) + kRect_OuterH);
	*reinterpret_cast<int*>(tip + 0x14) = y + oh;
}

#if defined(__GNUC__)
__attribute__((thiscall))
#endif
static void hk_tip_render(void* tip, void* menu) {
	if (!tip || !menu) return;
	void* source = tip_source_lookup(menu, tip);
	if (source) {
		void* hovered = *reinterpret_cast<void**>(static_cast<char*>(menu) + kMenu_HoveredRect);
		if (hovered != source) return;
	}
	if (oTip_Render) {
#if defined(__GNUC__) && defined(__i386__)
		thiscall_1arg_void(tip, menu, reinterpret_cast<void*>(oTip_Render));
#else
		oTip_Render(tip, menu);
#endif
	}
}

struct MenuExtendCache {
	void* menu = nullptr;
	int canvasX = 0;
	int canvasRight = 0;
};

static MenuExtendCache s_extend_cache[kExtendCacheMax];

static void extend_cache_store(void* menu_c, const void* def_frame) {
	if (!menu_c || !def_frame) return;
	const auto* fr = static_cast<const char*>(def_frame);
	const int cx = *reinterpret_cast<const int*>(fr + kFrame_OffX);
	const int right = cx + *reinterpret_cast<const int*>(fr + kFrame_W);
	for (int i = 0; i < kExtendCacheMax; ++i) {
		if (s_extend_cache[i].menu == menu_c) {
			s_extend_cache[i].canvasX = cx;
			s_extend_cache[i].canvasRight = right;
			return;
		}
	}
	for (int i = 0; i < kExtendCacheMax; ++i) {
		if (!s_extend_cache[i].menu) {
			s_extend_cache[i].menu = menu_c;
			s_extend_cache[i].canvasX = cx;
			s_extend_cache[i].canvasRight = right;
			return;
		}
	}
	s_extend_cache[0].menu = menu_c;
	s_extend_cache[0].canvasX = cx;
	s_extend_cache[0].canvasRight = right;
}

static bool extend_cache_lookup(const void* menu_c, int* canvasX, int* canvasRight) {
	if (!menu_c) return false;
	for (int i = 0; i < kExtendCacheMax; ++i) {
		if (s_extend_cache[i].menu == menu_c) {
			*canvasX = s_extend_cache[i].canvasX;
			*canvasRight = s_extend_cache[i].canvasRight;
			return true;
		}
	}
	return false;
}

static void menu_extend_clip_xw(int* x, int* w, const void* menu_c) {
	if (!x || !w || !menu_c || menu_c_is_resize_popup(menu_c)) return;
	if (current_vid_w <= 0) return;
	int cx = 0, right = 0;
	if (!extend_cache_lookup(menu_c, &cx, &right)) {
		void* def = menu_find_default_frame(const_cast<void*>(menu_c));
		if (!def) return;
		extend_cache_store(const_cast<void*>(menu_c), def);
		if (!extend_cache_lookup(menu_c, &cx, &right)) return;
	}
	if (*x + *w >= right) *w = current_vid_w - *x;
	if (*x <= cx) {
		*w += *x;
		*x = 0;
	}
}

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
#if FEATURE_INTRO_SCALER
	const bool intro_cine = intro_scaler_intro_cinematic_active();
#else
	const bool intro_cine = false;
#endif
	if (RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_SCALED_MENU) &&
	    set_clip_from_backdrop_open() && !intro_cine)
		menu_extend_clip_xw(&x, &w, menu);
	*outX = x;
	*outY = y;
	*outW = w;
	*outH = h;
	if (Draw_RenderRect) Draw_RenderRect(x, y, w, h);
}

static bool install_one(void* rva, void* hook, void** orig, const char* name, size_t detour_len = 0) {
	return GetDetourSystem().ApplyDetourAtAddress(
		rvaToAbsExe(rva), hook, orig, name, detour_len);
}

} // namespace scaled_menu_detail

using namespace scaled_menu_detail;

void scaled_menu_backdrop_clip_extend(int* x, int* w, const void* menu_c) {
	if (!RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_SCALED_MENU)) return;
	menu_extend_clip_xw(x, w, menu_c);
}

void scaled_menu_reload_open_page(void) {
	if (!detour_Cmd_ExecuteString::oCmd_ExecuteString) return;
	if (!scaled_menu_current_menu()) return;
	if (!detour_Cvar_Get::oCvar_Get) return;
	cvar_t* cv = detour_Cvar_Get::oCvar_Get("_sb_internal_menu_last_page", "", 0, nullptr);
	if (!cv || !cv->string || !cv->string[0] ||
	    std::strncmp(cv->string, "sof_buddy/", 10) != 0)
		return;
	char cmd[160];
	std::snprintf(cmd, sizeof(cmd), "sofbuddy_menu %s", cv->string);
	detour_Cmd_ExecuteString::oCmd_ExecuteString(cmd);
}

void scaled_menu_on_vid_layout_changed(void) {
	// Rects size themselves at RMF parse time from R_Strlen/R_StrHeight, so a menu that is
	// already open keeps the scale it was parsed with. Menus are re-parsed on next open
	// (see scaled_menu_M_PushMenu_pre), so nothing to rebuild here.
}

void scaled_menu_sync_scale_from_engine(void) {
	// Cheap and idempotent: recompute screen ratios from the engine's live vid size and
	// re-clamp menuScale to the new resolution max. Called before menu layout is measured
	// so the first open uses the same scale the second open does.
	static int s_lastW = -1, s_lastH = -1;
	static bool s_done = false;
	extern int* viddef_width;
	extern int* viddef_height;
	if (!viddef_width) viddef_width = static_cast<int*>(rvaToAbsExe(reinterpret_cast<void*>(0x0040365C)));
	if (!viddef_height) viddef_height = static_cast<int*>(rvaToAbsExe(reinterpret_cast<void*>(0x00403660)));
	if (!viddef_width || !viddef_height) return;
	const int w = *viddef_width, h = *viddef_height;
	if (w <= 0 || h <= 0) return;
	if (s_done && w == s_lastW && h == s_lastH) return;
	s_lastW = w;
	s_lastH = h;
	s_done = true;
	current_vid_w = w;
	current_vid_h = h;
	screen_y_scale = static_cast<float>(h) / 480.0f;
	screen_x_scale = static_cast<float>(w) / 640.0f;
	cvar_t* menu_cv = findCvar(const_cast<char*>("_sofbuddy_menu_scale"));
	if (menu_cv) menuscale_change(menu_cv);
}

void scaled_menu_refresh_open_canvases(void) {}

bool scaled_menu_install_hooks(void) {
	auto** rr = reinterpret_cast<void**>(rvaToAbsExe(reinterpret_cast<void*>(kRvaDrawRenderRectIat)));
	if (rr && *rr) Draw_RenderRect = reinterpret_cast<Draw_RenderRect_t>(*rr);
	bool ok = true;
#if !FEATURE_INTRO_SCALER
	ok = install_one(
		reinterpret_cast<void*>(kRvaMenuStmSetClip),
		reinterpret_cast<void*>(&hk_menu_stm_set_clip),
		nullptr,
		"menu_stm_set_clip");
#endif
	ok = install_one(
		reinterpret_cast<void*>(kRvaMenuBorderDimsInit),
		reinterpret_cast<void*>(&hk_menu_border_dims_init),
		reinterpret_cast<void**>(&oMenu_BorderDimsInit),
		"menu_border_dims_init") && ok;
	ok = install_one(
		reinterpret_cast<void*>(kRvaRectParse),
		reinterpret_cast<void*>(&hk_rect_parse),
		reinterpret_cast<void**>(&oRect_Parse),
		"rect_parse") && ok;
	ok = install_one(
		reinterpret_cast<void*>(kRvaFrameResize),
		reinterpret_cast<void*>(&hk_frame_resize),
		reinterpret_cast<void**>(&oFrame_Resize),
		"frame_resize") && ok;
	ok = install_one(
		reinterpret_cast<void*>(kRvaFrameFinalizeDims),
		reinterpret_cast<void*>(&hk_frame_finalize_dims),
		reinterpret_cast<void**>(&oFrame_FinalizeDims),
		"frame_finalize_dims") && ok;
	ok = install_one(
		reinterpret_cast<void*>(kRvaFrameCtor),
		reinterpret_cast<void*>(&hk_frame_ctor),
		reinterpret_cast<void**>(&oFrame_Ctor),
		"frame_ctor") && ok;
	ok = install_one(
		reinterpret_cast<void*>(kRvaFrameLink),
		reinterpret_cast<void*>(&hk_frame_link),
		reinterpret_cast<void**>(&oFrame_Link),
		"frame_link") && ok;
	ok = install_one(
		reinterpret_cast<void*>(kRvaTokeCtor),
		reinterpret_cast<void*>(&hk_toke_ctor),
		reinterpret_cast<void**>(&oToke_Ctor),
		"toke_ctor") && ok;
	ok = install_one(
		reinterpret_cast<void*>(kRvaTokeInsert),
		reinterpret_cast<void*>(&hk_toke_insert),
		reinterpret_cast<void**>(&oToke_Insert),
		"toke_insert") && ok;
	ok = install_one(
		reinterpret_cast<void*>(kRvaTickerInit),
		reinterpret_cast<void*>(&hk_ticker_init),
		reinterpret_cast<void**>(&oTicker_Init),
		"ticker_init") && ok;
	ok = install_one(
		reinterpret_cast<void*>(kRvaStmHoverPass),
		reinterpret_cast<void*>(&hk_stm_hover_pass),
		reinterpret_cast<void**>(&oStm_HoverPass),
		"stm_hover_pass") && ok;
	ok = install_one(
		reinterpret_cast<void*>(kRvaRectHoverPrep),
		reinterpret_cast<void*>(&hk_rect_hover_prep),
		reinterpret_cast<void**>(&oRect_HoverPrep),
		"rect_hover_prep") && ok;
	ok = install_one(
		reinterpret_cast<void*>(kRvaMenuCreateTip),
		reinterpret_cast<void*>(&hk_menu_create_tip),
		reinterpret_cast<void**>(&oMenu_CreateTip),
		"menu_create_tip") && ok;
	ok = install_one(
		reinterpret_cast<void*>(kRvaTipRender),
		reinterpret_cast<void*>(&hk_tip_render),
		reinterpret_cast<void**>(&oTip_Render),
		"tip_render") && ok;
	return ok && Draw_RenderRect && oMenu_BorderDimsInit && oRect_Parse &&
	       oFrame_FinalizeDims && oFrame_Ctor && oFrame_Link && oToke_Ctor &&
	       oToke_Insert && oTicker_Init && oStm_HoverPass && oRect_HoverPrep &&
	       oMenu_CreateTip && oTip_Render;
}

static float menu_effective_scale(float s) {
	float effective = effective_auto_scale(s);
	const float maxScale = auto_scale_resolution_ratio();
	if (effective > maxScale) effective = maxScale;
	return effective;
}

bool scaled_menu_uniform_actual_active(void) {
	if (!RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_SCALED_MENU)) return false;
	const float s = menu_effective_scale(menuScale > 0.0f ? menuScale : 1.0f);
	return std::fabs(s - 1.0f) > 0.001f;
}

static bool menu_page_skips_scale(void) {
	const char* n = s_pushed_menu_name;
	return std::strcmp(n, "load") == 0 || std::strcmp(n, "load_ss") == 0 ||
	       std::strcmp(n, "load_ns") == 0 || std::strcmp(n, "save") == 0 ||
	       std::strcmp(n, "save_ss") == 0 || std::strcmp(n, "save_ns") == 0 ||
	       std::strcmp(n, "m_load") == 0 || std::strcmp(n, "m_save") == 0;
}

float scaled_menu_screen_scale(void) {
	if (menu_page_skips_scale()) return 1.0f;
	float s = menuScale > 0.0f ? menuScale : 1.0f;
	if (join_scale_active() && s > 1.0f) {
		float join_scale = s * 0.75f;
		if (join_scale < 1.0f) join_scale = 1.0f;
		return join_scale;
	}
	return menu_effective_scale(s);
}

float scaled_menu_snapped_text_scale(realFontEnum_t font) {
	return snapped_text_scale(scaled_menu_screen_scale(), font_glyph_px(font));
}

#endif // UI_MENU



void scaled_menu_slider_setup_post(void* slider) {
	if (!slider || !scaled_menu_uniform_actual_active() ||
	    scaled_menu_skip_for_intro_disclaimer()) return;
	char* r = static_cast<char*>(slider);
	if (*reinterpret_cast<uint8_t*>(r + kRect_NoScale)) return;
	void* menu = menu_outer_from_widget(slider);
	if (!menu || !menu_actual_pixel_scale_active(menu)) return;
	float ws = *reinterpret_cast<float*>(r + kRect_WidgetScale);
	if (!std::isfinite(ws) || ws <= 1e-6f) ws = 1.0f;
	const float s = scaled_menu_screen_scale();
	if (!std::isfinite(s) || s <= 1e-6f) return;
	const float factor = s / ws;
	if (std::fabs(factor - 1.0f) <= 0.001f) return;
	constexpr ptrdiff_t fields[] = {
	    kRect_W, kRect_H, kRect_OuterW, kRect_OuterH,
	    0xA4, 0xA8, 0xBC, 0xC0, 0xD4, 0xD8,
	};
	for (ptrdiff_t off : fields) {
		int& v = *reinterpret_cast<int*>(r + off);
		if (v > 0) v = static_cast<int>(std::lround(static_cast<float>(v) * factor));
	}
}

#endif // FEATURE_SCALED_MENU
