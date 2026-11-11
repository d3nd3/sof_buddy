/*
	Menu scaling: content follows the vertical design scale (resY/480), frames keep the
	stock percent-of-V canvas scale (vid_w/640), and frame borders use the vertical
	design ratio on both axes. Rect width/height/xoff/yoff are scaled once at RMF parse
	and stored, so drawing, layout and cursor hit-testing agree.

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
#include "menu_variant.h"
#include "border_frame_dims.h"
#include "scroll_restore.h"
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
bool g_scaledMenuLoadboxDrawActive = false;
int g_scaledMenuLoadboxTextDy = 0;
float g_scaledMenuLoadboxScale = 1.0f;

static char s_pushed_menu_name[64] = {0};
static bool s_skip_vertical_shrink = false;
static bool s_resize_page_active = false;

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

void scaled_menu_M_PushMenu_post(char const*, char const*, bool) {}

namespace scaled_menu_detail {
static void tip_source_reset(void);
}

void scaled_menu_set_pushed_menu(const char* name) {
	scaled_menu_detail::tip_source_reset();
	scaled_menu_variant_reset();
	char redirected[64];
	name = scaled_menu_variant_redirect(name, redirected, sizeof(redirected));
	size_t i = 0;
	if (name) {
		for (; name[i] && i + 1 < sizeof(s_pushed_menu_name); ++i)
			s_pushed_menu_name[i] = name[i];
	}
	s_pushed_menu_name[i] = '\0';
	s_skip_vertical_shrink = false;
	s_resize_page_active = false;
}

namespace scaled_menu_detail {
static void patch_vbar_thumb_scale(void);
static void patch_loadbox_layout_scale(void);
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
	scaled_menu_detail::patch_vbar_thumb_scale();
	scaled_menu_detail::patch_loadbox_layout_scale();
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
static void* call_border_dims_init(void* border, char* dimA, char* dimB, void* tint, float scale,
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
constexpr uintptr_t kRvaTextDraw = 0x000D8B70u;
constexpr uintptr_t kRvaHrParse = 0x000DCF40u;
constexpr uintptr_t kRvaTickerInit = 0x000D1B30u;
constexpr uintptr_t kRvaTickerDraw = 0x000D1C80u;
constexpr uintptr_t kRvaVbarCtor = 0x000D5250u;
constexpr uintptr_t kRvaVbarCleanup = 0x000D5610u;
constexpr uintptr_t kRvaLoadboxDraw = 0x000DC250u;
constexpr uintptr_t kRvaVbarThumbWidth = 0x000D5976u;
constexpr uintptr_t kRvaVbarThumbInset = 0x000D5979u;
constexpr uintptr_t kRvaHbarParse = 0x000D51D0u;
constexpr uintptr_t kRvaHbarDraw = 0x000D5050u;
constexpr uintptr_t kRvaBackdropDraw = 0x000C3EF0u;
constexpr uintptr_t kRvaBackdropParse = 0x000C4220u;
constexpr uintptr_t kRvaBorderRender = 0x000C48F0u;
constexpr uintptr_t kRvaDrawBeveledBoxIat = 0x004035E8u;
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
constexpr uintptr_t kRvaLoadboxVtbl = 0x00112B80u;
constexpr uintptr_t kRvaPicSetup = 0x000D2290u;
constexpr int kExtendCacheMax = 12;

// rect_c / widget layout
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
constexpr ptrdiff_t kRect_Align = 0x4C;      // 1=center 2=left 3=right (from layout state)
constexpr ptrdiff_t kRect_Bolted = 0x62;     // set by <bolt>/<bbolt>
constexpr ptrdiff_t kRect_VirtualX = 0x63;
constexpr ptrdiff_t kRect_VirtualY = 0x64;
constexpr ptrdiff_t kRect_NoScale = 0x66;
constexpr ptrdiff_t kRect_Border = 0x68;
constexpr ptrdiff_t kRect_BoltAnchor = 0x7C;

// border_c
constexpr ptrdiff_t kBorder_ThickPx = 0x04;

// text_c strings: +0x8C is the font standard object, +0x9C the label.
constexpr ptrdiff_t kText_FontStd = 0x8C;
constexpr ptrdiff_t kText_Label = 0x9C;

// hr (setkey_c) layout
constexpr ptrdiff_t kHr_Width = 0x8C;
constexpr ptrdiff_t kHr_Line = 0x90;

// vbar_c layout
constexpr ptrdiff_t kVbar_PicW = 0xB4;
constexpr ptrdiff_t kVbar_PicH = 0xB8;

// hbar_c (picture progress bar). W/H come from the cap picture and Outer from W, so the
// generic rect parse hook never sees a <width>/<height> token for the stock tags.
constexpr ptrdiff_t kHbar_Cvar = 0x50;
constexpr ptrdiff_t kHbar_CapName = 0x8C;   // std::string: data at +4, size at +8
constexpr ptrdiff_t kHbar_TileName = 0x9C;
constexpr ptrdiff_t kHbar_Bevel = 0xAC;
constexpr ptrdiff_t kHbar_Invisible = 0xAD;
constexpr ptrdiff_t kHbar_TileW = 0xB0;
constexpr ptrdiff_t kHbar_TileH = 0xB4;

// backdrop_c (tile/stretch/center/right/left background): +0 is its stm, +4 is the
// mode (1 = tile), and +0x1C/+0x20 are the tile picture size that also sets the tiled
// repeat pitch.
constexpr ptrdiff_t kBackdrop_Stm = 0x00;
constexpr ptrdiff_t kBackdrop_Mode = 0x04;
constexpr ptrdiff_t kBackdrop_TileW = 0x1C;
constexpr ptrdiff_t kBackdrop_TileH = 0x20;
constexpr int kBackdropModeTile = 1;
constexpr ptrdiff_t kStdString_Data = 0x04;
constexpr ptrdiff_t kStdString_Size = 0x08;

// menu_c layout
constexpr ptrdiff_t kMenuC_ResizeScreen = 0x8C;
constexpr ptrdiff_t kMenuC_FreeX = 0x9C;
constexpr ptrdiff_t kMenuC_FreeY = 0xA0;
constexpr ptrdiff_t kMenuC_GlobalClipW = 0xA4;
constexpr ptrdiff_t kMenuC_GlobalClipH = 0xA8;
constexpr ptrdiff_t kMenuC_ScreenScale = 0xAC;

// stm_c / frame_c layout
constexpr ptrdiff_t kStm_GlobalPage = 0x89;
constexpr ptrdiff_t kStm_ScrollTotal = 0x7C;
constexpr ptrdiff_t kStm_ScrollY = 0x80;
constexpr ptrdiff_t kFrame_Menu = 0x00;
constexpr ptrdiff_t kFrame_Page = 0x04;
constexpr ptrdiff_t kFrame_NamePtr = 0x10;
constexpr ptrdiff_t kFrame_NameLen = 0x14;
constexpr ptrdiff_t kFrame_OffX = 0x40;
constexpr ptrdiff_t kFrame_OffY = 0x44;
constexpr ptrdiff_t kFrame_W = 0x48;
constexpr ptrdiff_t kFrame_H = 0x4C;
constexpr ptrdiff_t kMenuC_FrameList = 0x1C;
constexpr ptrdiff_t kListNode_Next = 0x00;
constexpr ptrdiff_t kListNode_Frame = 0x08;
constexpr int kFrameScrollMax = 12;

// engine entry points
constexpr uintptr_t kRvaFrameResize = 0x000C6640u;
constexpr uintptr_t kRvaFrameCtor = 0x000C60A0u;
constexpr uintptr_t kRvaFrameLink = 0x000C6530u;
constexpr uintptr_t kRvaTokeCtor = 0x000EAC70u;
constexpr uintptr_t kRvaTokeInsert = 0x000EAF60u;
constexpr uintptr_t kRvaStmClampScroll = 0x000EA160u;

using Draw_RenderRect_t = void(__cdecl*)(int x, int y, int w, int h);
static Draw_RenderRect_t Draw_RenderRect = nullptr;
static Draw_RenderRect_t oDraw_RenderRect = nullptr;
static bool s_ticker_draw_bolted = false;

static void __cdecl hk_draw_render_rect(int x, int y, int w, int h) {
	if (oDraw_RenderRect && s_ticker_draw_bolted && current_vid_w > 0) {
		x = 0;
		w = current_vid_w;
	}
	if (oDraw_RenderRect) oDraw_RenderRect(x, y, w, h);
}

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
static bool widget_is_loadbox(const void* widget) {
	if (!widget) return false;
	const void* vt = *static_cast<const void* const*>(widget);
	return vt == rvaToAbsExe(reinterpret_cast<void*>(kRvaLoadboxVtbl));
}

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

// <text ... talign center> opts a label into draw-time centring. Leading spaces used to
// fake this, but one space is a whole glyph and the old hard-coded width table assumed
// 3 px per space while the medium font advances ~6 px, which pushed labels into the
// right border at 1.25x. Draw-time centring uses the engine's own measurement and leaves
// the rect (border, hitbox, layout flow) untouched.
struct TextAlignEntry {
	void* text;
	void* stm;
	const char* label;
};
static TextAlignEntry s_text_align[128];
static int s_text_align_next = 0;

static void text_align_mark(void* text) {
	if (!text) return;
	char* t = static_cast<char*>(text);
	void* stm = *reinterpret_cast<void**>(t + kRect_MenuPtr);
	const char* label =
	    *reinterpret_cast<char* const*>(t + kText_Label + kStdString_Data);
	if (!label) return;
	for (TextAlignEntry& entry : s_text_align) {
		if (entry.text == text) {
			entry.stm = stm;
			entry.label = label;
			return;
		}
	}
	TextAlignEntry& entry =
	    s_text_align[s_text_align_next++ %
	                 (sizeof(s_text_align) / sizeof(s_text_align[0]))];
	entry.text = text;
	entry.stm = stm;
	entry.label = label;
}

static void text_align_unmark(void* text) {
	if (!text) return;
	for (TextAlignEntry& entry : s_text_align) {
		if (entry.text == text) {
			entry.text = nullptr;
			entry.stm = nullptr;
			entry.label = nullptr;
			return;
		}
	}
}

static bool text_align_is_center(const void* text) {
	if (!text) return false;
	const char* t = static_cast<const char*>(text);
	void* stm = *reinterpret_cast<void**>(const_cast<char*>(t) + kRect_MenuPtr);
	const char* label =
	    *reinterpret_cast<char* const*>(const_cast<char*>(t) + kText_Label + kStdString_Data);
	if (!label) return false;
	for (const TextAlignEntry& entry : s_text_align) {
		if (entry.text == text && entry.stm == stm && entry.label == label) return true;
	}
	return false;
}

// A parse-only marker token is consumed in two pieces: "talign" then its value, because
// rect_c::Parse sees one token per call and text_c::Parse advances after each one.
static void* s_text_align_pending_token = nullptr;
static void* s_text_align_pending_rect = nullptr;

static bool text_align_parse_token(void* rect, void* token, const char* keyword) {
	if (!rect || !token || !keyword || !widget_is_text(rect)) return false;
	if (std::strcmp(keyword, "talign") == 0) {
		const char* value = thiscall_token(
		    token, 1, rvaToAbsExe(reinterpret_cast<void*>(kRvaToken)));
		if (value && std::strcmp(value, "center") == 0)
			text_align_mark(rect);
		else
			text_align_unmark(rect);
		s_text_align_pending_token = token;
		s_text_align_pending_rect = rect;
		return true;
	}
	if (s_text_align_pending_token == token && s_text_align_pending_rect == rect &&
	    (std::strcmp(keyword, "center") == 0 || std::strcmp(keyword, "left") == 0 ||
	     std::strcmp(keyword, "right") == 0)) {
		s_text_align_pending_token = nullptr;
		s_text_align_pending_rect = nullptr;
		return true;
	}
	s_text_align_pending_token = nullptr;
	s_text_align_pending_rect = nullptr;
	return false;
}

using Text_Draw_t = int(__thiscall*)(void* text);
static Text_Draw_t oText_Draw = nullptr;

static int call_text_draw(void* text, void* fn) {
	int out = 0;
	if (!fn || !text) return out;
#if defined(__GNUC__) && defined(__i386__)
	__asm__ __volatile__(
		"movl %1, %%ecx\n\t"
		"call *%2\n\t"
		"movl %%eax, %0"
		: "=m"(out)
		: "m"(text), "m"(fn)
		: "ecx", "eax", "edx", "memory");
#else
	out = reinterpret_cast<Text_Draw_t>(fn)(text);
#endif
	return out;
}

#if defined(__GNUC__)
__attribute__((thiscall))
#endif
static int hk_text_draw(void* text) {
	if (!text || !text_align_is_center(text))
		return call_text_draw(text, reinterpret_cast<void*>(oText_Draw));

	char* t = static_cast<char*>(text);
	const char* label =
	    *reinterpret_cast<char* const*>(t + kText_Label + kStdString_Data);
	const int box_w = *reinterpret_cast<int*>(t + kRect_W);
	if (!label || !*label || box_w <= 0)
		return call_text_draw(text, reinterpret_cast<void*>(oText_Draw));

	const int text_w =
	    scaled_menu_measure_text_width(label, t + kText_FontStd);
	if (text_w <= 0)
		return call_text_draw(text, reinterpret_cast<void*>(oText_Draw));

	int delta = centered_text_offset(box_w, text_w);
	if (delta == 0)
		return call_text_draw(text, reinterpret_cast<void*>(oText_Draw));

	// GetCoords multiplies virtual xoff by the canvas on the hard (bolted/aligned)
	// path; divide it out so the visual offset is still delta pixels.
	void* stm = *reinterpret_cast<void**>(t + kRect_MenuPtr);
	void* frame = stm ? *reinterpret_cast<void**>(stm) : nullptr;
	void* menu = frame ? *reinterpret_cast<void**>(frame) : nullptr;
	float canvas =
	    menu ? *reinterpret_cast<float*>(static_cast<char*>(menu) + kMenuC_ScreenScale) : 1.f;
	if (!std::isfinite(canvas) || canvas <= 1e-6f) canvas = 1.f;
	const bool hard_path =
	    *reinterpret_cast<int*>(t + kRect_Align) == 2 ||
	    *reinterpret_cast<int*>(t + kRect_Align) == 3 ||
	    *reinterpret_cast<void**>(t + kRect_BoltAnchor) != nullptr;
	if (hard_path && *reinterpret_cast<uint8_t*>(t + kRect_VirtualX) &&
	    std::fabs(canvas - 1.f) > 0.001f) {
		delta = static_cast<int>(std::lround(static_cast<float>(delta) / canvas));
	}

	int& xoff = *reinterpret_cast<int*>(t + kRect_Xoff);
	const int saved = xoff;
	xoff = saved + delta;
	const int ret = call_text_draw(text, reinterpret_cast<void*>(oText_Draw));
	xoff = saved;
	return ret;
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

// Same ratio as the frame-height shrink: vertical target over the menu canvas.
// At 1920x1080 this is 640 * (1080/1920) / 480 = 0.75.
static float vertical_shrink_ratio(float canvas) {
	const float target = auto_scale_resolution_ratio();
	if (!std::isfinite(target) || target <= 0.0f || !std::isfinite(canvas) || canvas <= 1e-6f)
		return 1.0f;
	return target / canvas;
}

// sub_200C46D0 is __thiscall (this in ECX + 4 stack args, callee pops 0x10):
// hk must carry __attribute__((thiscall)) so ECX/slots line up when JMP'd into.
#if defined(__GNUC__)
__attribute__((thiscall))
#endif
static void* hk_menu_border_dims_init(void* border, char* dimA, char* dimB, void* tint, float scale) {
	const bool skip = scaled_menu_skip_for_intro_disclaimer();
	const bool active = scaled_menu_uniform_actual_active() && !skip;
	// Rect borders pass 1.0; frame borders pass the stock canvas scale (vid_w/640).
	const bool frame_border = std::fabs(scale - 1.0f) > 0.001f;
	if (skip || (!active && !frame_border)) {
#if defined(__GNUC__) && defined(__i386__)
		return oMenu_BorderDimsInit
		           ? call_border_dims_init(border, dimA, dimB, tint, scale,
		                                       reinterpret_cast<void*>(oMenu_BorderDimsInit))
		           : border;
#else
		return oMenu_BorderDimsInit ? oMenu_BorderDimsInit(border, dimA, dimB, tint, scale) : border;
#endif
	}
	// Frame borders always use the vertical design ratio on both axes; at 1x the
	// stock path would otherwise use the wider horizontal canvas (vid_w/640).
	const float use = scaled_menu_detail::border_dims_init_scale(
	    frame_border, scaled_menu_screen_scale(), auto_scale_resolution_ratio(), active);
#if defined(__GNUC__) && defined(__i386__)
	void* ret = oMenu_BorderDimsInit
	                ? call_border_dims_init(border, dimA, dimB, tint, use,
	                                            reinterpret_cast<void*>(oMenu_BorderDimsInit))
	                : border;
#else
	void* ret = oMenu_BorderDimsInit ? oMenu_BorderDimsInit(border, dimA, dimB, tint, use) : border;
#endif
	if (border) {
		// [border+4] is the raw thickness the stock call site never scales.
		int& t = *reinterpret_cast<int*>(static_cast<char*>(border) + kBorder_ThickPx);
		if (t > 0) t = static_cast<int>(std::lround(static_cast<float>(t) * use));
	}
	return ret;
}

static void* s_pic_fixed_scaled[128] = {};
static int s_pic_fixed_next = 0;

static void pic_fixed_mark(void* pic) {
	if (!pic) return;
	s_pic_fixed_scaled[s_pic_fixed_next++ %
	                   (sizeof(s_pic_fixed_scaled) / sizeof(s_pic_fixed_scaled[0]))] = pic;
}

static bool pic_fixed_consume(void* pic) {
	if (!pic) return false;
	for (void*& entry : s_pic_fixed_scaled) {
		if (entry == pic) {
			entry = nullptr;
			return true;
		}
	}
	return false;
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
	if (text_align_parse_token(rect, token, keyword)) return true;
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
	// The loadbox width needs the canvas divided out even at 1x. Its Parse multiplies the
	// stored width by the canvas, and the scaled-menu wrapper puts the page in a narrower
	// 4:3 centre frame; without this the list overflows the frame and the layout rejects
	// the whole loadbox as too wide to place.
	const bool loadbox = widget_is_loadbox(rect);
	const bool loadbox_width_fix =
	    fixed_width && loadbox &&
	    RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_SCALED_MENU);
	if ((!scaled_menu_uniform_actual_active() && !loadbox_width_fix) ||
	    scaled_menu_skip_for_intro_disclaimer() ||
	    *reinterpret_cast<uint8_t*>(r + kRect_NoScale)) {
		return parsed;
	}
	float factor = menu_actual_rect_dim_factor(rect);
	if (loadbox) {
		char* menu = static_cast<char*>(menu_outer_from_widget(rect));
		float canvas = menu ? *reinterpret_cast<float*>(menu + kMenuC_ScreenScale) : 1.f;
		if (!std::isfinite(canvas) || canvas <= 1e-6f) canvas = 1.f;
		factor = scaled_menu_detail::loadbox_width_parse_factor(
		    scaled_menu_screen_scale(), canvas);
	}
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
		if (widget_is_pic(rect)) pic_fixed_mark(rect);
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

using Pic_Setup_t = char(__thiscall*)(void* pic);
static Pic_Setup_t oPic_Setup = nullptr;

#if defined(__GNUC__)
__attribute__((thiscall))
#endif
static char hk_pic_setup(void* pic) {
	char ret = 0;
	if (oPic_Setup) {
#if defined(__GNUC__) && defined(__i386__)
		ret = static_cast<char>(thiscall_noargs_int(pic, reinterpret_cast<void*>(oPic_Setup)));
#else
		ret = oPic_Setup(pic);
#endif
	}
	if (pic && !scaled_menu_skip_for_intro_disclaimer()) {
		char* p = static_cast<char*>(pic);
		const bool bolted = *reinterpret_cast<uint8_t*>(p + kRect_Bolted) ||
		                    *reinterpret_cast<void**>(p + kRect_BoltAnchor);
		bool fixed = false;
		if (!bolted) fixed = pic_fixed_consume(pic);
		if (!bolted && !fixed) {
			const float factor = menu_actual_rect_dim_factor(pic);
			static constexpr ptrdiff_t fields[] = {kRect_W, kRect_H, kRect_OuterW, kRect_OuterH};
			scale_widget_dims(pic, fields, sizeof(fields) / sizeof(fields[0]), factor);
		}
	}
	return ret;
}

// <hr> is a setkey_c object with its own parser; its width/line tokens never reach
// rect_c::Parse. Scale the stored fields after the original parse.
using Hr_Parse_t = char(__thiscall*)(void* hr, void* token);
static Hr_Parse_t oHr_Parse = nullptr;

#if defined(__GNUC__)
__attribute__((thiscall))
#endif
static char hk_hr_parse(void* hr, void* token) {
	const bool parsed = oHr_Parse
	                        ? thiscall_rect_parse(hr, token, reinterpret_cast<void*>(oHr_Parse))
	                        : false;
	if (!parsed || !hr || scaled_menu_skip_for_intro_disclaimer()) return parsed ? 1 : 0;
	char* p = static_cast<char*>(hr);
	if (*reinterpret_cast<uint8_t*>(p + kRect_NoScale)) return 1;
	const float factor = menu_actual_rect_dim_factor(hr);
	if (!std::isfinite(factor) || factor <= 1.001f) return 1;
	int& width = *reinterpret_cast<int*>(p + kHr_Width);
	int& line = *reinterpret_cast<int*>(p + kHr_Line);
	if (width > 0) width = static_cast<int>(std::lround(static_cast<float>(width) * factor));
	if (line > 0) line = static_cast<int>(std::lround(static_cast<float>(line) * factor));
	return 1;
}

// <hbar> sizes itself from its pictures in the ctor (cap W/H, repeated-tile W/H), so the
// generic rect parse hook never sees <width>/<height>. Scale those fields after the
// hbar's own Parse; hk_hbar_draw then stretches the pictures to the scaled fields.
using Hbar_Parse_t = bool(__thiscall*)(void* hbar, void* token);
static Hbar_Parse_t oHbar_Parse = nullptr;

static void hbar_scale_dims(void* hbar) {
	if (!hbar) return;
	// Picture tiles are stretched by the draw hook; without ref_gl's stretch draw the
	// scaled pitch would overflow an unscaled cap, so leave the widget stock instead.
	if (!detour_Draw_StretchPic::oDraw_StretchPic) return;
	char* r = static_cast<char*>(hbar);
	if (*reinterpret_cast<uint8_t*>(r + kRect_NoScale)) return;
	void* menu = menu_outer_from_widget(hbar);
	if (!menu_actual_pixel_scale_active(menu)) return;
	const float s = scaled_menu_screen_scale();
	if (!std::isfinite(s) || s <= 1e-6f || std::fabs(s - 1.f) <= 0.001f) return;
	float ws = *reinterpret_cast<float*>(r + kRect_WidgetScale);
	if (!std::isfinite(ws) || ws <= 1e-6f) ws = 1.f;
	void* border = *reinterpret_cast<void**>(r + kRect_Border);
	const int padding = border ? *reinterpret_cast<int*>(static_cast<char*>(border)) : 0;
	int& w = *reinterpret_cast<int*>(r + kRect_W);
	int& h = *reinterpret_cast<int*>(r + kRect_H);
	int& ow = *reinterpret_cast<int*>(r + kRect_OuterW);
	int& oh = *reinterpret_cast<int*>(r + kRect_OuterH);
	const auto sw = scale_fixed_rect_dimension(w, ws, padding, s);
	const auto sh = scale_fixed_rect_dimension(h, ws, padding, s);
	w = sw.value;
	ow = sw.outer;
	h = sh.value;
	oh = sh.outer;
	if (!*reinterpret_cast<uint8_t*>(r + kHbar_Bevel)) {
		// Picture mode: the engine repeats the tile at the native pitch and the cap at
		// native size. Keep the scaled pitch/size so the draw hook can stretch them.
		int& tw = *reinterpret_cast<int*>(r + kHbar_TileW);
		int& th = *reinterpret_cast<int*>(r + kHbar_TileH);
		if (tw > 0) tw = static_cast<int>(std::lround(static_cast<float>(tw) * s));
		if (th > 0) th = static_cast<int>(std::lround(static_cast<float>(th) * s));
	}
}

#if defined(__GNUC__)
__attribute__((thiscall))
#endif
static bool hk_hbar_parse(void* hbar, void* token) {
	const bool parsed = oHbar_Parse
	                        ? thiscall_rect_parse(hbar, token, reinterpret_cast<void*>(oHbar_Parse))
	                        : false;
	if (!parsed || !hbar) return parsed;
	if (!scaled_menu_uniform_actual_active() || scaled_menu_skip_for_intro_disclaimer())
		return parsed;
	hbar_scale_dims(hbar);
	return parsed;
}

static void* backdrop_menu(void* backdrop) {
	if (!backdrop) return nullptr;
	void* stm = *reinterpret_cast<void**>(static_cast<char*>(backdrop) + kBackdrop_Stm);
	void* frame = stm ? *reinterpret_cast<void**>(stm) : nullptr;
	return frame ? *reinterpret_cast<void**>(frame) : nullptr;
}

static bool backdrop_scale_active(void* backdrop) {
	if (!scaled_menu_uniform_actual_active() || scaled_menu_skip_for_intro_disclaimer())
		return false;
	void* menu = backdrop_menu(backdrop);
	return !menu || menu_actual_pixel_scale_active(menu);
}

static void thiscall_backdrop_draw(void* backdrop, void* stm, void* fn) {
	if (!fn) return;
	__asm__ __volatile__(
		"pushl %1\n\t"
		"movl %0, %%ecx\n\t"
		"call *%2\n\t"
		:
		: "m"(backdrop), "m"(stm), "m"(fn)
		: "ecx", "eax", "edx", "memory");
}

using Backdrop_Draw_t = void(__thiscall*)(void* backdrop, void* stm);
static Backdrop_Draw_t oBackdrop_Draw = nullptr;

#if defined(__GNUC__)
__attribute__((thiscall))
#endif
static void hk_backdrop_draw(void* backdrop, void* stm) {
	const bool prev = g_tiledBackdropDrawing;
	if (backdrop &&
	    *reinterpret_cast<int*>(static_cast<char*>(backdrop) + kBackdrop_Mode) ==
	        kBackdropModeTile &&
	    backdrop_scale_active(backdrop))
		g_tiledBackdropDrawing = true;
	thiscall_backdrop_draw(backdrop, stm, reinterpret_cast<void*>(oBackdrop_Draw));
	g_tiledBackdropDrawing = prev;
}

// <backdrop tile "backdrop/web_bg"> stores the native picture size in +0x1C/+0x20 and
// repeats at that pitch. Scale the stored size after parsing so the tiled background
// follows the menu scale instead of staying at its authored 128 px pitch.
using Backdrop_Parse_t = bool(__thiscall*)(void* backdrop, void* token);
static Backdrop_Parse_t oBackdrop_Parse = nullptr;

#if defined(__GNUC__)
__attribute__((thiscall))
#endif
static bool hk_backdrop_parse(void* backdrop, void* token) {
	const bool parsed = oBackdrop_Parse
	                        ? thiscall_rect_parse(backdrop, token,
	                                              reinterpret_cast<void*>(oBackdrop_Parse))
	                        : false;
	if (!parsed || !backdrop) return parsed;
	char* b = static_cast<char*>(backdrop);
	if (*reinterpret_cast<int*>(b + kBackdrop_Mode) != kBackdropModeTile) return parsed;
	if (!backdrop_scale_active(backdrop)) return parsed;
	const float s = scaled_menu_screen_scale();
	if (!std::isfinite(s) || s <= 1.001f) return parsed;
	int& tw = *reinterpret_cast<int*>(b + kBackdrop_TileW);
	int& th = *reinterpret_cast<int*>(b + kBackdrop_TileH);
	if (tw > 0) tw = static_cast<int>(std::lround(static_cast<float>(tw) * s));
	if (th > 0) th = static_cast<int>(std::lround(static_cast<float>(th) * s));
	return parsed;
}

static void write_code_byte(uintptr_t rva, uint8_t value) {
	void* addr = rvaToAbsExe(reinterpret_cast<void*>(rva));
	if (!addr) return;
	DWORD old = 0;
	if (VirtualProtect(addr, 1, PAGE_EXECUTE_READWRITE, &old)) {
		*static_cast<uint8_t*>(addr) = value;
		VirtualProtect(addr, 1, old, &old);
	}
}

static void write_code_dword(uintptr_t rva, uint32_t value) {
	void* addr = rvaToAbsExe(reinterpret_cast<void*>(rva));
	if (!addr) return;
	DWORD old = 0;
	if (VirtualProtect(addr, 4, PAGE_EXECUTE_READWRITE, &old)) {
		*static_cast<uint32_t*>(addr) = value;
		VirtualProtect(addr, 4, old, &old);
	}
}

static void write_code_bytes(uintptr_t rva, const uint8_t* bytes, size_t count) {
	void* addr = rvaToAbsExe(reinterpret_cast<void*>(rva));
	if (!addr || !bytes || count == 0) return;
	DWORD old = 0;
	if (VirtualProtect(addr, count, PAGE_EXECUTE_READWRITE, &old)) {
		std::memcpy(addr, bytes, count);
		VirtualProtect(addr, count, old, &old);
	}
}

// `menu_c::DrawCursor` (0x200CA8C0) sizes the software cursor picture and its hotspot by
// `menu_c+0xAC`, the page's canvas scale. `<stm resize>` pins that field to the page's
// absolute canvas (1.0 at 1x), so the cursor drops to stock pixels while a resize dialog
// is up (96 px -> 32 px at 1080p). Normal menus already use the width canvas scale, so
// point those four reads at screen_x_scale (vid_w/640) and the cursor stops changing size.
static void patch_menu_cursor_scale(void) {
	const uint32_t scale =
	    static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&screen_x_scale));
	struct Patch {
		uintptr_t rva;
		uint8_t opcode;
		uint8_t original_modrm;
		uint8_t patched_modrm;
	};
	static constexpr Patch kPatches[] = {
	    {0x000CA901u, 0xD9, 0x86, 0x05}, // fld  dword ptr [esi+0ACh]
	    {0x000CA92Cu, 0xD9, 0x86, 0x05},
	    {0x000CA95Eu, 0xD8, 0x8E, 0x0D}, // fmul dword ptr [esi+0ACh]
	    {0x000CA970u, 0xD8, 0x8E, 0x0D},
	};
	for (const Patch& patch : kPatches) {
		void* addr = rvaToAbsExe(reinterpret_cast<void*>(patch.rva));
		if (!addr) continue;
		const uint8_t* code = static_cast<const uint8_t*>(addr);
		// Only accept the stock 6-byte form: <op> 86/8E AC 00 00 00.
		if (code[0] != patch.opcode || code[1] != patch.original_modrm || code[2] != 0xAC ||
		    code[3] != 0x00 || code[4] != 0x00 || code[5] != 0x00)
			continue;
		const uint8_t patched[6] = {
		    patch.opcode,
		    patch.patched_modrm,
		    static_cast<uint8_t>(scale & 0xFFu),
		    static_cast<uint8_t>((scale >> 8) & 0xFFu),
		    static_cast<uint8_t>((scale >> 16) & 0xFFu),
		    static_cast<uint8_t>((scale >> 24) & 0xFFu),
		};
		write_code_bytes(patch.rva, patched, sizeof(patched));
	}
}

static void patch_loadbox_layout_scale(void) {
	const float s = scaled_menu_screen_scale();
	int icon = 128;
	int pad = 32;
	if (std::isfinite(s) && s > 0.f) {
		icon = static_cast<int>(std::lround(128.f * s));
		pad = static_cast<int>(std::lround(32.f * s));
	}
	if (icon < 1) icon = 1;
	if (pad < 1) pad = 1;
	if (pad > 127) pad = 127;
	write_code_dword(0x000DBF21u, static_cast<uint32_t>(icon));
	write_code_dword(0x000DC9D9u, static_cast<uint32_t>(icon));
	write_code_byte(0x000DC002u, static_cast<uint8_t>(pad));
	write_code_byte(0x000DC9F0u, static_cast<uint8_t>(pad));
	write_code_byte(0x000DC4E9u, static_cast<uint8_t>(pad));
}

static void patch_vbar_thumb_scale(void) {
	const float s = scaled_menu_screen_scale();
	int width = 10;
	int inset = 3;
	if (std::isfinite(s) && s > 0.f) {
		width = static_cast<int>(std::lround(10.f * s));
		inset = static_cast<int>(std::lround(3.f * s));
	}
	if (width < 1) width = 1;
	if (width > 127) width = 127;
	if (inset < 0) inset = 0;
	if (inset > 127) inset = 127;
	write_code_byte(kRvaVbarThumbWidth, static_cast<uint8_t>(width));
	write_code_byte(kRvaVbarThumbInset, static_cast<uint8_t>(inset));
}

static void scale_vbar_for_menu(void* vbar) {
	if (!vbar || !scaled_menu_uniform_actual_active() || scaled_menu_skip_for_intro_disclaimer())
		return;
	char* p = static_cast<char*>(vbar);
	if (*reinterpret_cast<uint8_t*>(p + kRect_NoScale)) return;
	const float s = scaled_menu_screen_scale();
	if (!std::isfinite(s) || s <= 1.001f) return;

	int& w = *reinterpret_cast<int*>(p + kRect_W);
	int& ow = *reinterpret_cast<int*>(p + kRect_OuterW);
	int& xo = *reinterpret_cast<int*>(p + kRect_Xoff);
	int& pic_w = *reinterpret_cast<int*>(p + kVbar_PicW);
	int& pic_h = *reinterpret_cast<int*>(p + kVbar_PicH);
	float widget_scale = *reinterpret_cast<float*>(p + kRect_WidgetScale);
	if (!std::isfinite(widget_scale) || widget_scale <= 1e-6f) widget_scale = 1.0f;
	void* border = *reinterpret_cast<void**>(p + kRect_Border);
	const int border_w = border ? *reinterpret_cast<int*>(border) : 0;

	const int old_outer_w = ow;
	if (w > 0) w = static_cast<int>(std::lround(static_cast<float>(w) * s));
	if (pic_w > 0) pic_w = static_cast<int>(std::lround(static_cast<float>(pic_w) * s));
	if (pic_h > 0) pic_h = static_cast<int>(std::lround(static_cast<float>(pic_h) * s));
	if (w > 0)
		ow = static_cast<int>(std::lround(static_cast<float>(w) * widget_scale)) + 2 * border_w;
	else if (ow > 0)
		ow = static_cast<int>(std::lround(static_cast<float>(ow) * s));
	xo -= (ow - old_outer_w);
}

using Vbar_Ctor_t = void*(__thiscall*)(void* vbar, void* a1, void* a2);
static Vbar_Ctor_t oVbar_Ctor = nullptr;

#if defined(__GNUC__)
__attribute__((thiscall))
#endif
static void* hk_vbar_ctor(void* vbar, void* a1, void* a2) {
	if (oVbar_Ctor)
		thiscall_2args_void(vbar, a1, reinterpret_cast<int>(a2),
		                    reinterpret_cast<void*>(oVbar_Ctor));
	scale_vbar_for_menu(vbar);
	return vbar;
}

using Vbar_Cleanup_t = char(__thiscall*)(void* vbar, void* toke);
static Vbar_Cleanup_t oVbar_Cleanup = nullptr;

#if defined(__GNUC__)
__attribute__((thiscall))
#endif
static char hk_vbar_cleanup(void* vbar, void* toke) {
	if (!vbar || !oVbar_Cleanup)
		return 0;
	char* v = static_cast<char*>(vbar);
	char* stm = *reinterpret_cast<char**>(v + kRect_MenuPtr);
	char* frame = stm ? *reinterpret_cast<char**>(stm) : nullptr;
	int vbar_h = *reinterpret_cast<int*>(v + kRect_H);
	if (!stm || !frame || vbar_h > 0)
		return thiscall_rect_parse(vbar, toke, reinterpret_cast<void*>(oVbar_Cleanup)) ? 1 : 0;

	// Internal F12 frames use height 0 to mean "full screen". The stock vbar derives
	// its height from the frame, so it ends up 0 there. Size it to the content instead
	// (clamped to the screen), and let Cleanup compute the thumb against that height.
	const int total = *reinterpret_cast<int*>(stm + kStm_ScrollTotal);
	int screen_h = current_vid_h > 0 ? current_vid_h : 480;
	if (total <= 0 || screen_h <= 0)
		return thiscall_rect_parse(vbar, toke, reinterpret_cast<void*>(oVbar_Cleanup)) ? 1 : 0;
	int effective_h = total < screen_h ? total : screen_h;
	if (effective_h < 1) effective_h = 1;

	const int frame_h = *reinterpret_cast<int*>(frame + kFrame_H);
	const int parent_pad = (frame_h - vbar_h) / 2;
	*reinterpret_cast<int*>(frame + kFrame_H) = effective_h + 2 * parent_pad;
	const char ok =
	    thiscall_rect_parse(vbar, toke, reinterpret_cast<void*>(oVbar_Cleanup)) ? 1 : 0;
	*reinterpret_cast<int*>(frame + kFrame_H) = frame_h;
	if (!ok)
		return 0;

	float widget_scale = *reinterpret_cast<float*>(v + kRect_WidgetScale);
	if (!std::isfinite(widget_scale) || widget_scale <= 1e-6f) widget_scale = 1.0f;
	void* border = *reinterpret_cast<void**>(v + kRect_Border);
	const int border_h = border ? *reinterpret_cast<int*>(border) : 0;
	*reinterpret_cast<int*>(v + kRect_H) = effective_h;
	*reinterpret_cast<int*>(v + kRect_OuterH) =
	    static_cast<int>(std::lround(static_cast<float>(effective_h) * widget_scale)) + 2 * border_h;
	return 1;
}

using Loadbox_Draw_t = void(__thiscall*)(void* loadbox);
static Loadbox_Draw_t oLoadbox_Draw = nullptr;

#if defined(__GNUC__)
__attribute__((thiscall))
#endif
static void hk_loadbox_draw(void* loadbox) {
	const bool prev_active = g_scaledMenuLoadboxDrawActive;
	const int prev_dy = g_scaledMenuLoadboxTextDy;
	const float prev_scale = g_scaledMenuLoadboxScale;
	const float s = scaled_menu_screen_scale();
	g_scaledMenuLoadboxScale = s;
	g_scaledMenuLoadboxTextDy =
	    (std::isfinite(s) && s > 1.001f) ? static_cast<int>(std::lround(96.f * (s - 1.f))) : 0;
	g_scaledMenuLoadboxDrawActive = true;
	thiscall_noargs_void(loadbox, reinterpret_cast<void*>(oLoadbox_Draw));
	g_scaledMenuLoadboxDrawActive = prev_active;
	g_scaledMenuLoadboxTextDy = prev_dy;
	g_scaledMenuLoadboxScale = prev_scale;
}

using Hbar_Draw_t = void(__thiscall*)(void* hbar);
static Hbar_Draw_t oHbar_Draw = nullptr;

using Hbar_DrawBeveledBox_t = void(__cdecl*)(int x, int y, int w, int h, void* tint);

static const char* hbar_string_data(const char* string_obj) {
	if (!string_obj) return nullptr;
	const char* data = *reinterpret_cast<char* const*>(string_obj + kStdString_Data);
	return (data && *data) ? data : nullptr;
}

static int hbar_string_size(const char* string_obj) {
	return string_obj ? *reinterpret_cast<const int*>(string_obj + kStdString_Size) : 0;
}

static void hbar_draw_stretch_pic(int x, int y, int w, int h, const char* pic) {
	if (!pic || !*pic || w <= 0 || h <= 0) return;
	if (!detour_Draw_StretchPic::oDraw_StretchPic) return;
	using tDrawStretchPic = detour_Draw_StretchPic::tDraw_StretchPic;
	char path[160];
	std::snprintf(path, sizeof(path), "pics/menus/%s", pic);
	// ref_gl's Draw_StretchPic takes the palette as a pointer; the detour typedef keeps
	// the same 4-byte argument slot, so pass the address of a local palette.
	int palette = -1;
	reinterpret_cast<tDrawStretchPic>(detour_Draw_StretchPic::oDraw_StretchPic)(
	    x, y, w, h, static_cast<int>(reinterpret_cast<intptr_t>(&palette)), path, 0);
}

#if defined(__GNUC__)
__attribute__((thiscall))
#endif
static void hk_hbar_draw(void* hbar) {
	if (!hbar) return;
	bool active = scaled_menu_uniform_actual_active();
	if (active && scaled_menu_skip_for_intro_disclaimer()) active = false;
	char* r = static_cast<char*>(hbar);
	if (!active || *reinterpret_cast<uint8_t*>(r + kRect_NoScale) ||
	    !detour_Draw_StretchPic::oDraw_StretchPic) {
		thiscall_noargs_void(hbar, reinterpret_cast<void*>(oHbar_Draw));
		return;
	}
	int x = 0, y = 0;
	if (!thiscall_rect_get_coords(hbar, &x, &y,
	                              rvaToAbsExe(reinterpret_cast<void*>(kRvaRectGetCoords))))
		return;
	float progress = 0.f;
	if (void* cv = *reinterpret_cast<void**>(r + kHbar_Cvar))
		progress = *reinterpret_cast<float*>(static_cast<char*>(cv) + 0x18);
	if (!std::isfinite(progress)) progress = 0.f;
	if (progress < 0.f) progress = 0.f;
	if (progress > 1.f) progress = 1.f;
	if (*reinterpret_cast<uint8_t*>(r + kHbar_Invisible) && progress <= 0.f) return;
	const int w = *reinterpret_cast<int*>(r + kRect_W);
	const int h = *reinterpret_cast<int*>(r + kRect_H);
	if (*reinterpret_cast<uint8_t*>(r + kHbar_Bevel)) {
		if (progress > 0.f) {
			auto** iat = reinterpret_cast<void**>(rvaToAbsExe(
			    reinterpret_cast<void*>(kRvaDrawBeveledBoxIat)));
			void* fn = iat ? *iat : nullptr;
			if (fn) {
				const int fill_w = static_cast<int>(static_cast<float>(w) * progress);
				reinterpret_cast<Hbar_DrawBeveledBox_t>(fn)(x, y, fill_w, h, r + 0x38);
			}
		}
	} else {
		const int tile_w = *reinterpret_cast<int*>(r + kHbar_TileW);
		const int tile_h = *reinterpret_cast<int*>(r + kHbar_TileH);
		if (tile_w > 0) {
			// Round so a fractional menuScale still fills the last tile at 100%;
			// truncating the rounded pitch can leave the bar one tile short.
			const int count = static_cast<int>(std::lround(static_cast<float>(w) * progress /
			                                              static_cast<float>(tile_w)));
			const char* tile = hbar_string_data(r + kHbar_TileName);
			for (int i = 0; i < count; ++i)
				hbar_draw_stretch_pic(x + i * tile_w, y, tile_w, tile_h, tile);
		}
		const char* cap = hbar_string_data(r + kHbar_CapName);
		if (cap && hbar_string_size(r + kHbar_CapName) > 0)
			hbar_draw_stretch_pic(x, y, w, h, cap);
	}
	if (void* border = *reinterpret_cast<void**>(r + kRect_Border))
		thiscall_1arg_void(border, hbar,
		                   rvaToAbsExe(reinterpret_cast<void*>(kRvaBorderRender)));
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
	// `<stm resize W H>` pins the page to absolute pixels at every menu scale. The engine
	// sets menu->screenScale to 1.0 here; hk_frame_link keys off menu_c_is_resize_popup()
	// so the 4:3->wide frame ratio never grows these frames by vid_h/480 (the stretched
	// precache/player dialog). Mark the session flag before the scale check as well.
	s_resize_page_active = true;
	if (!scaled_menu_uniform_actual_active() || scaled_menu_skip_for_intro_disclaimer()) return;
	const float s = scaled_menu_screen_scale();
	if (!std::isfinite(s) || std::fabs(s - 1.f) < 0.001f) return;
	char* f = static_cast<char*>(frame);
	// Stock pinned this page to W×H actual pixels with canvas scale 1.0. Scale both the
	// canvas (contents) and the default frame box, then recentre the resulting size.
	if (char* menu = *reinterpret_cast<char**>(f + kFrame_Menu)) {
		*reinterpret_cast<float*>(menu + kMenuC_ScreenScale) = s;
	}
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

// Internal sof_buddy pages keep the stock frame canvas, so their frames skip the
// 4:3 -> wide height correction. Other menus follow the normal frame rules.
static void note_menu_source(const char* name) {
	if (!name || !*name) return;
	if (name_contains_ci(name, "sof_buddy/"))
		s_skip_vertical_shrink = true;
}

struct FrameDimsRec {
	void* menu;
	void* frame;
	int w;
	int h;
	bool skip_vertical_shrink; // sof_buddy/* pages keep the stock frame canvas
	bool height_shrunk;        // hk_frame_link applies at most once per ctor
	bool width_shrunk;         // ss_content pages shrink width once as well
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
	rec->skip_vertical_shrink = false;
	rec->height_shrunk = false;
	rec->width_shrunk = false;
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
		rec->skip_vertical_shrink = s_skip_vertical_shrink;
		rec->height_shrunk = false;
		rec->width_shrunk = false;
	}
	return ret;
}

using Frame_Link_t = void(__thiscall*)(void* child, void* parent);
static Frame_Link_t oFrame_Link = nullptr;

static bool frame_name_is(const void* frame, const char* want) {
	if (!frame) return false;
	const char* f = static_cast<const char*>(frame);
	const char* p = *reinterpret_cast<const char* const*>(f + kFrame_NamePtr);
	const size_t n = *reinterpret_cast<const size_t*>(f + kFrame_NameLen);
	const size_t wl = std::strlen(want);
	return p && n == wl && std::memcmp(p, want, wl) == 0;
}

static bool frame_name_starts_with(const void* frame, const char* prefix) {
	if (!frame) return false;
	const char* f = static_cast<const char*>(frame);
	const char* p = *reinterpret_cast<const char* const*>(f + kFrame_NamePtr);
	const size_t n = *reinterpret_cast<const size_t*>(f + kFrame_NameLen);
	const size_t pl = std::strlen(prefix);
	return p && n >= pl && std::memcmp(p, prefix, pl) == 0;
}

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
		const bool skip_vertical_shrink =
		    (cr && cr->skip_vertical_shrink) || (pr && pr->skip_vertical_shrink);
		if (cr) {
			cr->skip_vertical_shrink = skip_vertical_shrink;
			const float canvas =
			    menu ? *reinterpret_cast<float*>(static_cast<char*>(menu) + kMenuC_ScreenScale) : 0.f;
			const float ratio = vertical_shrink_ratio(canvas);
			// Resize pages carry their own absolute pixel canvas (`<stm resize W H>`), which
			// is authoritative over the frame box; never apply the wide-screen frame ratio.
			// The check is per-menu: a resize popup opened earlier must not disable the
			// correction for unrelated menus parsed later in the same session.
			const bool can_shrink = std::isfinite(ratio) && ratio > 0.0f &&
			                        std::fabs(ratio - 1.0f) > 0.001f &&
			                        !menu_c_is_resize_popup(menu) &&
			                        !scaled_menu_skip_for_intro_disclaimer();
			if (!cr->height_shrunk && !skip_vertical_shrink && can_shrink && menu && cr->h != 0) {
				int& fh = *reinterpret_cast<int*>(c + kFrame_H);
				fh = static_cast<int>(std::lround(static_cast<float>(cr->h) * ratio));
				cr->height_shrunk = true;
			}
			// The ss_content page is the 4:3 page scaled into the narrower 16:9
			// centre, so its frame widths use the same vertical ratio.
			if (!cr->width_shrunk && !skip_vertical_shrink && can_shrink && menu && cr->w > 0 &&
			    scaled_menu_ss_content_page_active() && !frame_name_is(c, "ss_content") &&
			    !frame_name_starts_with(c, "16_9_")) {
				int& fw = *reinterpret_cast<int*>(c + kFrame_W);
				fw = static_cast<int>(std::lround(static_cast<float>(cr->w) * ratio));
				cr->width_shrunk = true;
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
	char redirected[128];
	const char* use = scaled_menu_variant_redirect(name, redirected, sizeof(redirected));
	note_menu_source(use);
	return oToke_Ctor ? oToke_Ctor(toke, use) : toke;
}

using Toke_Insert_t = void(__thiscall*)(void* toke, const char* name);
static Toke_Insert_t oToke_Insert = nullptr;

#if defined(__GNUC__)
__attribute__((thiscall))
#endif
static void hk_toke_insert(void* toke, const char* name) {
	char redirected[128];
	const char* use = scaled_menu_variant_redirect(name, redirected, sizeof(redirected));
	note_menu_source(use);
	if (oToke_Insert) oToke_Insert(toke, use);
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
	char* t = ticker ? static_cast<char*>(ticker) : nullptr;
	bool bolted = false;
	float wide_ratio = 1.f;
	if (t) {
		bolted = *reinterpret_cast<uint8_t*>(t + kRect_Bolted) ||
		         *reinterpret_cast<void**>(t + kRect_BoltAnchor) != nullptr;
		char* menu = static_cast<char*>(menu_outer_from_widget(ticker));
		const float canvas = menu ? *reinterpret_cast<float*>(menu + kMenuC_ScreenScale) : 1.f;
		const float target = screen_y_scale;
		if (std::isfinite(canvas) && canvas > 1e-6f && std::isfinite(target) &&
		    target > 0.f)
			wide_ratio = target / canvas;
	}
	if (t && scaled_menu_uniform_actual_active() && !scaled_menu_skip_for_intro_disclaimer()) {
		if (!*reinterpret_cast<uint8_t*>(t + kRect_NoScale)) {
			// Tickers live in the canvas/art space (they are usually bolted to a ghoul part
			// drawn at canvas scale), while their text width comes from R_StrLen at
			// menuScale. The engine only multiplies W by the canvas for bolted tickers;
			// apply the same canvas scale ourselves when it does not.
			char* menu = static_cast<char*>(menu_outer_from_widget(ticker));
			const float canvas = menu ? *reinterpret_cast<float*>(menu + kMenuC_ScreenScale) : 1.f;
			const bool engine_scales = bolted;
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
	if (t && bolted && std::fabs(wide_ratio - 1.f) > 0.001f) {
		if (!*reinterpret_cast<uint8_t*>(t + kRect_NoScale)) {
			int& w = *reinterpret_cast<int*>(t + kRect_W);
			if (w > 0) w = static_cast<int>(std::lround(static_cast<float>(w) * wide_ratio));
			int& ow = *reinterpret_cast<int*>(t + kRect_OuterW);
			if (ow > 0) ow = static_cast<int>(std::lround(static_cast<float>(ow) * wide_ratio));
			int& xo = *reinterpret_cast<int*>(t + kRect_Xoff);
			if (xo != 0) xo = static_cast<int>(std::lround(static_cast<float>(xo) * wide_ratio));
		}
	}
}

using Ticker_Draw_t = char(__thiscall*)(void* ticker);
static Ticker_Draw_t oTicker_Draw = nullptr;

#if defined(__GNUC__)
__attribute__((thiscall))
#endif
static char hk_ticker_draw(void* ticker) {
	const bool previous = s_ticker_draw_bolted;
	char* t = static_cast<char*>(ticker);
	s_ticker_draw_bolted =
	    t && (*reinterpret_cast<uint8_t*>(t + kRect_Bolted) ||
	          *reinterpret_cast<void**>(t + kRect_BoltAnchor) != nullptr);
	const int ret = thiscall_noargs_int(ticker, reinterpret_cast<void*>(oTicker_Draw));
	s_ticker_draw_bolted = previous;
	return static_cast<char>(ret);
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

struct HoverState {
	void* stm;
	void* rect;
	unsigned generation;
};
static HoverState s_hover_states[128];
static int s_hover_state_next = 0;
static unsigned s_hover_generation = 0;

static void hover_state_record(void* stm, void* rect) {
	if (!stm) return;
	++s_hover_generation;
	const int count = static_cast<int>(sizeof(s_hover_states) / sizeof(s_hover_states[0]));
	for (int i = 0; i < count; ++i) {
		if (s_hover_states[i].stm == stm) {
			s_hover_states[i].rect = rect;
			s_hover_states[i].generation = s_hover_generation;
			return;
		}
	}
	HoverState& entry = s_hover_states[s_hover_state_next++ % count];
	entry.stm = stm;
	entry.rect = rect;
	entry.generation = s_hover_generation;
}

static HoverState* hover_state_find(void* stm) {
	if (!stm) return nullptr;
	const int count = static_cast<int>(sizeof(s_hover_states) / sizeof(s_hover_states[0]));
	for (int i = 0; i < count; ++i) {
		if (s_hover_states[i].stm == stm) return &s_hover_states[i];
	}
	return nullptr;
}

struct TipSource {
	void* menu;
	void* tip;
	void* rect;
	void* stm;
	unsigned generation;
	bool from_stm_hover;
};
static TipSource s_tip_sources[64];
static int s_tip_source_next = 0;

static void tip_source_store(void* menu, void* tip, void* rect, void* stm,
                             bool from_stm_hover, unsigned generation) {
	if (!menu || !tip || !rect) return;
	const int count = static_cast<int>(sizeof(s_tip_sources) / sizeof(s_tip_sources[0]));
	for (int i = 0; i < count; ++i) {
		if (s_tip_sources[i].menu == menu && s_tip_sources[i].tip == tip) {
			s_tip_sources[i].rect = rect;
			s_tip_sources[i].stm = stm;
			s_tip_sources[i].generation = generation;
			s_tip_sources[i].from_stm_hover = from_stm_hover;
			return;
		}
	}
	TipSource& entry = s_tip_sources[s_tip_source_next++ % count];
	entry.menu = menu;
	entry.tip = tip;
	entry.rect = rect;
	entry.stm = stm;
	entry.generation = generation;
	entry.from_stm_hover = from_stm_hover;
}

static void tip_source_reset(void) {
	for (TipSource& entry : s_tip_sources) {
		entry.menu = nullptr;
		entry.tip = nullptr;
		entry.rect = nullptr;
		entry.stm = nullptr;
		entry.generation = 0;
		entry.from_stm_hover = false;
	}
	s_tip_source_next = 0;
	for (HoverState& entry : s_hover_states) {
		entry.stm = nullptr;
		entry.rect = nullptr;
		entry.generation = 0;
	}
	s_hover_state_next = 0;
	++s_hover_generation;
}

static TipSource* tip_source_find(void* menu, void* tip) {
	if (!menu || !tip) return nullptr;
	const int count = static_cast<int>(sizeof(s_tip_sources) / sizeof(s_tip_sources[0]));
	for (int i = 0; i < count; ++i) {
		if (s_tip_sources[i].menu == menu && s_tip_sources[i].tip == tip)
			return &s_tip_sources[i];
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
	if (s_resize_page_active && widget_is_pic(rect) &&
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
	if (stm)
		hover_state_record(stm, *reinterpret_cast<void**>(static_cast<char*>(stm) + 0x34));
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
	void* stm = *reinterpret_cast<void**>(static_cast<char*>(rect) + kRect_MenuPtr);
	void* stm_hovered = stm ? *reinterpret_cast<void**>(static_cast<char*>(stm) + 0x34) : nullptr;
	tip_source_store(menu, tip, rect, stm, stm_hovered == rect, s_hover_generation);
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
	TipSource* source = tip_source_find(menu, tip);
	if (source) {
		if (source->from_stm_hover) {
			HoverState* hover = hover_state_find(source->stm);
			if (hover && hover->generation > source->generation &&
			    hover->rect != source->rect)
				return;
		} else {
			void* hovered = *reinterpret_cast<void**>(static_cast<char*>(menu) + kMenu_HoveredRect);
			if (hovered && hovered != source->rect) return;
		}
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

struct FrameScrollState {
	char name[48];
	int total;
	int offset;
};

static FrameScrollState s_frame_scroll[kFrameScrollMax];
static int s_frame_scroll_count = 0;

static void frame_name_copy(void* frame, char* out, size_t out_size) {
	if (!out || out_size == 0) return;
	out[0] = '\0';
	if (!frame) return;
	const char* text = *reinterpret_cast<const char**>(static_cast<char*>(frame) + kFrame_NamePtr);
	size_t length = *reinterpret_cast<size_t*>(static_cast<char*>(frame) + kFrame_NameLen);
	if (!text) return;
	if (length >= out_size) length = out_size - 1;
	std::memcpy(out, text, length);
	out[length] = '\0';
}

template <typename Fn>
static void for_each_menu_frame(void* menu, Fn&& fn) {
	if (!menu) return;
	void* sentinel = *reinterpret_cast<void**>(static_cast<char*>(menu) + kMenuC_FrameList);
	if (!sentinel) return;
	for (void* node = *reinterpret_cast<void**>(static_cast<char*>(sentinel) + kListNode_Next);
	     node && node != sentinel;
	     node = *reinterpret_cast<void**>(static_cast<char*>(node) + kListNode_Next)) {
		void* frame = *reinterpret_cast<void**>(static_cast<char*>(node) + kListNode_Frame);
		if (frame) fn(frame);
	}
}

// A menu scale change re-parses the page, which replaces every widget and starts
// the content stm at scroll 0. Capture each frame page's scroll before the command
// and reapply it to the matching frame after the new menu is built.
static void save_open_menu_scroll(void* menu) {
	s_frame_scroll_count = 0;
	for_each_menu_frame(menu, [](void* frame) {
		if (s_frame_scroll_count >= kFrameScrollMax) return;
		void* stm = *reinterpret_cast<void**>(static_cast<char*>(frame) + kFrame_Page);
		if (!stm) return;
		const int total = *reinterpret_cast<int*>(static_cast<char*>(stm) + kStm_ScrollTotal);
		const int offset = *reinterpret_cast<int*>(static_cast<char*>(stm) + kStm_ScrollY);
		if (total <= 0 || offset <= 0) return;
		FrameScrollState& saved = s_frame_scroll[s_frame_scroll_count];
		frame_name_copy(frame, saved.name, sizeof(saved.name));
		if (!saved.name[0]) return;
		saved.total = total;
		saved.offset = offset;
		++s_frame_scroll_count;
	});
}

static void restore_open_menu_scroll(void* menu) {
	if (s_frame_scroll_count == 0) return;
	if (!menu) {
		s_frame_scroll_count = 0;
		return;
	}
	for_each_menu_frame(menu, [](void* frame) {
		char name[48];
		frame_name_copy(frame, name, sizeof(name));
		if (!name[0]) return;
		for (int i = 0; i < s_frame_scroll_count; ++i) {
			const FrameScrollState& saved = s_frame_scroll[i];
			if (std::strcmp(saved.name, name) != 0) continue;
			void* stm = *reinterpret_cast<void**>(static_cast<char*>(frame) + kFrame_Page);
			if (!stm) return;
			const int new_total =
			    *reinterpret_cast<int*>(static_cast<char*>(stm) + kStm_ScrollTotal);
			if (new_total <= 0) return;
			const int new_offset =
			    restored_scroll_offset(saved.offset, saved.total, new_total);
			*reinterpret_cast<int*>(static_cast<char*>(stm) + kStm_ScrollY) = new_offset;
			thiscall_noargs_void(stm, rvaToAbsExe(reinterpret_cast<void*>(kRvaStmClampScroll)));
			return;
		}
	});
	s_frame_scroll_count = 0;
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
	void* old_menu = scaled_menu_current_menu();
	if (!old_menu) return;
	if (!detour_Cvar_Get::oCvar_Get) return;
	cvar_t* cv = detour_Cvar_Get::oCvar_Get("_sb_internal_menu_last_page", "", 0, nullptr);
	if (!cv || !cv->string || !cv->string[0] ||
	    std::strncmp(cv->string, "sof_buddy/", 10) != 0)
		return;
	save_open_menu_scroll(old_menu);
	char cmd[160];
	std::snprintf(cmd, sizeof(cmd), "sofbuddy_menu %s", cv->string);
	detour_Cmd_ExecuteString::oCmd_ExecuteString(cmd);
	restore_open_menu_scroll(scaled_menu_current_menu());
}

void scaled_menu_on_vid_layout_changed(void) {
	// Rects size themselves at RMF parse time from R_Strlen/R_StrHeight, so a menu that is
	// already open keeps the scale it was parsed with. Menus are re-parsed on next open
	// (see scaled_menu_M_PushMenu_pre), so nothing to rebuild here.
}

void scaled_menu_sync_scale_from_engine(void) {
	// Cheap and idempotent: recompute screen ratios from the actual video size and
	// re-clamp menuScale to the new resolution max. Called before menu layout is measured
	// so the first open uses the same scale the second open does.
	static int s_lastW = -1, s_lastH = -1;
	static bool s_done = false;
	extern int* viddef_width;
	extern int* viddef_height;
	// SoFPlus can expose the 16:9 *virtual* menu height through viddef_height while a
	// menu is being parsed. current_vid_* is captured by VID_CheckChanges from the
	// actual video mode; prefer it so the frame vertical ratio stays based on the real
	// screen height instead of the virtual page height.
	int w = current_vid_w;
	int h = current_vid_h;
	if (w <= 0 || h <= 0) {
		if (!viddef_width) viddef_width = static_cast<int*>(rvaToAbsExe(reinterpret_cast<void*>(0x0040365C)));
		if (!viddef_height) viddef_height = static_cast<int*>(rvaToAbsExe(reinterpret_cast<void*>(0x00403660)));
		if (!viddef_width || !viddef_height) return;
		w = *viddef_width;
		h = *viddef_height;
	}
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
	else {
		scaled_menu_detail::patch_vbar_thumb_scale();
		scaled_menu_detail::patch_loadbox_layout_scale();
	}
}

void scaled_menu_refresh_open_canvases(void) {}

bool scaled_menu_install_hooks(void) {
	auto** rr = reinterpret_cast<void**>(rvaToAbsExe(reinterpret_cast<void*>(kRvaDrawRenderRectIat)));
	if (rr && *rr) {
		oDraw_RenderRect = reinterpret_cast<Draw_RenderRect_t>(*rr);
		Draw_RenderRect = oDraw_RenderRect;
		writeUnsignedIntegerAt(rr, reinterpret_cast<unsigned int>(&hk_draw_render_rect));
	}
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
		reinterpret_cast<void*>(kRvaTextDraw),
		reinterpret_cast<void*>(&hk_text_draw),
		reinterpret_cast<void**>(&oText_Draw),
		"text_draw") && ok;
	ok = install_one(
		reinterpret_cast<void*>(kRvaFrameResize),
		reinterpret_cast<void*>(&hk_frame_resize),
		reinterpret_cast<void**>(&oFrame_Resize),
		"frame_resize") && ok;
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
		reinterpret_cast<void*>(kRvaTickerDraw),
		reinterpret_cast<void*>(&hk_ticker_draw),
		reinterpret_cast<void**>(&oTicker_Draw),
		"ticker_draw") && ok;
	ok = install_one(
		reinterpret_cast<void*>(kRvaPicSetup),
		reinterpret_cast<void*>(&hk_pic_setup),
		reinterpret_cast<void**>(&oPic_Setup),
		"pic_setup") && ok;
	ok = install_one(
		reinterpret_cast<void*>(kRvaHrParse),
		reinterpret_cast<void*>(&hk_hr_parse),
		reinterpret_cast<void**>(&oHr_Parse),
		"hr_parse") && ok;
	ok = install_one(
		reinterpret_cast<void*>(kRvaVbarCtor),
		reinterpret_cast<void*>(&hk_vbar_ctor),
		reinterpret_cast<void**>(&oVbar_Ctor),
		"vbar_ctor") && ok;
	ok = install_one(
		reinterpret_cast<void*>(kRvaVbarCleanup),
		reinterpret_cast<void*>(&hk_vbar_cleanup),
		reinterpret_cast<void**>(&oVbar_Cleanup),
		"vbar_cleanup") && ok;
	ok = install_one(
		reinterpret_cast<void*>(kRvaLoadboxDraw),
		reinterpret_cast<void*>(&hk_loadbox_draw),
		reinterpret_cast<void**>(&oLoadbox_Draw),
		"loadbox_draw") && ok;
	ok = install_one(
		reinterpret_cast<void*>(kRvaHbarParse),
		reinterpret_cast<void*>(&hk_hbar_parse),
		reinterpret_cast<void**>(&oHbar_Parse),
		"hbar_parse") && ok;
	ok = install_one(
		reinterpret_cast<void*>(kRvaHbarDraw),
		reinterpret_cast<void*>(&hk_hbar_draw),
		reinterpret_cast<void**>(&oHbar_Draw),
		"hbar_draw") && ok;
	ok = install_one(
		reinterpret_cast<void*>(kRvaBackdropDraw),
		reinterpret_cast<void*>(&hk_backdrop_draw),
		reinterpret_cast<void**>(&oBackdrop_Draw),
		"backdrop_draw") && ok;
	ok = install_one(
		reinterpret_cast<void*>(kRvaBackdropParse),
		reinterpret_cast<void*>(&hk_backdrop_parse),
		reinterpret_cast<void**>(&oBackdrop_Parse),
		"backdrop_parse") && ok;
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
	patch_menu_cursor_scale();
	return ok && Draw_RenderRect && oMenu_BorderDimsInit && oRect_Parse &&
	       oText_Draw && oFrame_Ctor && oFrame_Link && oToke_Ctor &&
	       oToke_Insert && oTicker_Init && oPic_Setup && oHr_Parse &&
	       oHbar_Parse && oHbar_Draw && oBackdrop_Draw && oBackdrop_Parse &&
	       oStm_HoverPass && oRect_HoverPrep && oMenu_CreateTip && oTip_Render;
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

float scaled_menu_screen_scale(void) {
	const float s = menuScale > 0.0f ? menuScale : 1.0f;
	return menu_effective_scale(s);
}

bool scaled_menu_text_align_available(void) {
	return oRect_Parse != nullptr && oText_Draw != nullptr;
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
