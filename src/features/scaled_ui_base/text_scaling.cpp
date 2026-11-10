/*
	Scaled UI - Text Scaling Functions

	This file handles non-console text scaling:
	- Generic HUD/menu text vertex scaling (ref.dll R_DrawFont)
*/

#include "feature_config.h"

#if FEATURE_SCALED_HUD || FEATURE_SCALED_MENU || FEATURE_INTRO_SCALER

#include "features.h"
#include "generated_detours.h"
#include "runtime_features.h"
using detour_R_DrawFont::oR_DrawFont;
#include "shared.h"
#include "util.h"
#if FEATURE_SCALED_MENU
#include "../scaled_menu/shared.h"
#include "../scaled_menu/border_frame_dims.h"
#endif
#if FEATURE_INTRO_SCALER
#include "../intro_scaler/shared.h"
#endif
#include "debug/hook_callsite.h"
#include <math.h>
#include <stdint.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#endif

static float pivotx;
static float pivoty;
static float fontStartX;
int characterIndex = 0;

// Menu text scaling: SoF lays text out with R_Strlen / R_StrHeight (centring, caret
// position, clipping, row pitch, column offsets), so those scale by the same factor as
// the glyphs. Each run then grows from its own start, leaving SoF's position intact.
static float g_menuFontPivotX = 0.0f;
static float g_menuFontPivotY = 0.0f;

bool scaled_menu_version_stamp(const char* text, int font) {
	if (!text || !*text || font != static_cast<int>(REALFONT_SMALL)) return false;
	if (text[0] < '0' || text[0] > '9') return false;
	bool dot = false;
	size_t n = 0;
	for (const char* p = text; *p; ++p) {
		if (++n > 31 || *p == ' ') return false;
		if (*p == '.') dot = true;
	}
	return dot;
}

static bool forceVanillaFont(void)
{
	cvar_t* pack = findCvar(const_cast<char*>("_sofbuddy_font_pack"));
	return pack && static_cast<int>(pack->value) == 2;
}

FontCaller g_currentFontCaller = FontCaller::Unknown;

void computeTextBottomAnchor(float topLineY, int lineCount, float lineHeight,
    float& bottomY, float& targetBottomY)
{
    const int lines = (lineCount > 0) ? lineCount : 1;
    bottomY = topLineY + (lines - 1) * lineHeight;
    const float virtualH = 480.0f;
    const float vidH = (current_vid_h > 0) ? (float)current_vid_h : virtualH;
    const float bottomY480 = bottomY * virtualH / vidH;
    float marginPct = (virtualH - bottomY480) / virtualH;
    if (marginPct < 0.0f) marginPct = 0.0f;
    if (marginPct > 1.0f) marginPct = 1.0f;
    targetBottomY = vidH * (1.0f - marginPct);
}

void applyBottomAnchoredScale(float& x, float& y, float bottomY, float targetBottomY,
    float scale, bool centerX)
{
    if (centerX) {
        const float cx = (current_vid_w > 0) ? current_vid_w * 0.5f : 320.0f;
        x = cx + (x - cx) * scale;
    } else {
        x *= scale;
    }
	y = targetBottomY + (y - bottomY) * scale;
}

inline void handleFontVertex(float x, float y, bool scaleX, bool scaleY, bool incrementChar) {
	SOFBUDDY_ASSERT(orig_glVertex2f != nullptr);

	// Playername/icon text drawn inside ref_gl Draw_PlayerInfo. Excluded from scaling unless
	// the player asked for icon autoscale.
	extern bool g_iconsAutoscale;
	if (isDrawingTeamicons && !g_iconsAutoscale) {
		orig_glVertex2f(x, y);
		return;
	}

	FontCaller caller = g_currentFontCaller;
	switch (caller) {
		case FontCaller::DMRankingCalcXY: {
			if (!RuntimeFeatures::Enabled(
			        RuntimeFeatures::Feature::RUNTIME_FEATURE_SCALED_HUD)) {
				orig_glVertex2f(x, y);
				break;
			}
			const float s = snapped_text_scale_active(hudScale);
			SOFBUDDY_ASSERT(s > 0.0f);
			if (scaleX) x = pivotx + (x - pivotx) * s;
			if (scaleY) y = pivoty + (y - pivoty) * s;
			orig_glVertex2f(x + (characterIndex * realFontSizes[realFont])*(s-1), y);
			break;
		}
		case FontCaller::Inventory2: {
			if (!RuntimeFeatures::Enabled(
			        RuntimeFeatures::Feature::RUNTIME_FEATURE_SCALED_HUD)) {
				orig_glVertex2f(x, y);
				break;
			}
			// Same scale as cropped inventory/ammo pics (no glyph snap).
			const float s = hudScale;
			SOFBUDDY_ASSERT(s > 0.0f);
			if (scaleX) x = pivotx + (x - pivotx) * s;
			if (scaleY) y = pivoty + (y - pivoty) * s;
			const float advance = forceVanillaFont()
				? x - fontStartX
				: characterIndex * realFontSizes[realFont];
			orig_glVertex2f(x + advance * (s - 1), y);
			break;
		}
			
		case FontCaller::ScopeCalcXY:
			orig_glVertex2f(x, y);
			break;
			
		case FontCaller::SCRDrawPause: {
			if (!RuntimeFeatures::Enabled(
			        RuntimeFeatures::Feature::RUNTIME_FEATURE_SCALED_CON)) {
				orig_glVertex2f(x, y);
				break;
			}
			const float s = snapped_text_scale_active(fontScale);
			if (s != 1.0f) {
				if (scaleX) x = pivotx + (x - pivotx) * s;
				if (scaleY) y = pivoty + (y - pivoty) * s;
				orig_glVertex2f(x + (characterIndex * realFontSizes[realFont])*(s-1), y);
			} else {
				orig_glVertex2f(x, y);
			}
			break;
		}

		case FontCaller::SCR_DrawCenterPrint: {
			if (!RuntimeFeatures::Enabled(
			        RuntimeFeatures::Feature::RUNTIME_FEATURE_SCALED_CON)) {
				orig_glVertex2f(x, y);
				break;
			}
			const float s = snapped_text_scale_active(fontScale);
			if (s != 1.0f && g_centerPrintAnchorSeq == g_lastCenterPrintSeq &&
			    g_centerPrintAnchorY > 0.0f && g_lastCenterPrintLineCount > 0) {
				float font_h = g_centerPrintLineStep;
				if (font_h <= 0.0f) font_h = static_cast<float>(realFontSizes[realFont]);
				if (font_h <= 0.0f) font_h = 8.0f;
				computeTextBottomAnchor(g_centerPrintAnchorY, g_lastCenterPrintLineCount,
				    font_h, g_centerPrintBottomY, g_centerPrintTargetBottomY);
				applyBottomAnchoredScale(x, y, g_centerPrintBottomY,
				    g_centerPrintTargetBottomY, s, true);
			}
			orig_glVertex2f(x, y);
			break;
		}

		case FontCaller::MissionStatus: {
			if (!RuntimeFeatures::Enabled(
			        RuntimeFeatures::Feature::RUNTIME_FEATURE_SCALED_CON)) {
				orig_glVertex2f(x, y);
				break;
			}
			const float s = snapped_text_scale_active(fontScale);
			if (s != 1.0f) {
				if (scaleX) x = pivotx + (x - pivotx) * s;
				if (scaleY) y = pivoty + (y - pivoty) * s;
				orig_glVertex2f(x + (characterIndex * realFontSizes[realFont]) * (s - 1.0f), y);
			} else {
				orig_glVertex2f(x, y);
			}
			break;
		}
			
		case FontCaller::DrawLine:
			orig_glVertex2f(x, y);
			break;

		case FontCaller::PlayerInfoName: {
			// Playername above a player (ref_gl Draw_PlayerInfo). Same pivot-growth scheme as
			// DMRanking; hkR_DrawFont pre-shifted screenX so the scaled string stays centered.
			if (!RuntimeFeatures::Enabled(
			        RuntimeFeatures::Feature::RUNTIME_FEATURE_SCALED_HUD) || !g_iconsAutoscale) {
				orig_glVertex2f(x, y);
				break;
			}
			const float s = snapped_text_scale_active(hudScale);
			SOFBUDDY_ASSERT(s > 0.0f);
			if (scaleX) x = pivotx + (x - pivotx) * s;
			if (scaleY) y = pivoty + (y - pivoty) * s;
			orig_glVertex2f(x + (characterIndex * realFontSizes[realFont])*(s-1), y);
			break;
		}
			
#ifdef UI_MENU
		case FontCaller::RectDrawTextItem:
		case FontCaller::RectDrawTextLine:
		case FontCaller::TickerDraw:
		case FontCaller::InputHandle:
		case FontCaller::FileboxHandle:
		case FontCaller::LoadboxGetIndices:
		case FontCaller::TipRender:
		case FontCaller::MenuSystemText:
#if FEATURE_INTRO_SCALER
			if (intro_scaler_disclaimer_menu_text_active()) {
				const float s = intro_scaler_page_width_scale();
				if (std::fabs(s - 1.f) < 0.001f) {
					orig_glVertex2f(x, y);
					break;
				}
				// Scale the whole run around its first glyph, like menu text below.
				// The engine's x positions already contain the space advance (spaces
				// emit no glyph quad), so scaling them keeps word gaps proportional at
				// any resolution. The old per-quad pivot + characterIndex advance
				// ignored spaces and collapsed them at 4K.
				orig_glVertex2f(
				    g_menuFontPivotX + (x - g_menuFontPivotX) * s,
				    g_menuFontPivotY + (y - g_menuFontPivotY) * s);
				break;
			}
			if (intro_scaler_intro_cinematic_active()) {
				orig_glVertex2f(x, y);
				break;
			}
#endif
#if FEATURE_SCALED_MENU
			if (scaled_menu_uniform_actual_active()) {
				const float s = scaled_menu_snapped_text_scale(realFont);
				if (s == 1.0f) {
					orig_glVertex2f(x, y);
					break;
				}
				orig_glVertex2f(
				    scaled_menu_detail::scale_axis_coordinate(x, g_menuFontPivotX, s),
				    scaled_menu_detail::scale_axis_coordinate(y, g_menuFontPivotY, s));
			} else
#endif
				orig_glVertex2f(x, y);
			break;
#endif
			
		default:
			orig_glVertex2f(x, y);
			break;
	}
	
	if (incrementChar) characterIndex++;
}

void __stdcall my_glVertex2f_DrawFont_1(float x, float y) {
	if (characterIndex == 0) {
		fontStartX = x;
		// Run start: pivot so the run grows right/down from where SoF's layout put it.
		g_menuFontPivotX = x;
		g_menuFontPivotY = y;
	}
	pivotx = x;
	pivoty = y;
	handleFontVertex(x, y, false, false, false);
}

void __stdcall my_glVertex2f_DrawFont_2(float x, float y) {
	handleFontVertex(x, y, true, false, false);
}

void __stdcall my_glVertex2f_DrawFont_3(float x, float y) {
	handleFontVertex(x, y, true, true, false);
}

void __stdcall my_glVertex2f_DrawFont_4(float x, float y) {
	handleFontVertex(x, y, false, true, true);
}

#ifdef UI_MENU

// Menu-module callers measure text so their layout (centring, caret at x + width,
// clipping, row pitch, column offsets) matches the scaled glyphs we draw.
struct MenuMeasureCaller {
	bool is_menu = false;
};

static bool pickMenuMeasureCaller(const CallerInfo& info, void* raw) {
	if (is_menu_system_caller(info.module, (uint32_t)info.functionStartRva)) {
		static_cast<MenuMeasureCaller*>(raw)->is_menu = true;
		return true;
	}
	return false;
}

float scaled_menu_font_measure_scale(const char* fontStd) {
#if FEATURE_SCALED_MENU
	if (!RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_SCALED_MENU) ||
	    !scaled_menu_uniform_actual_active()) {
		return 1.0f;
	}
	scaled_menu_sync_scale_from_engine();
	const char* fontName = fontStd
	                           ? *reinterpret_cast<char* const*>(fontStd + 4)
	                           : nullptr;
	const realFontEnum_t font = fontName ? getRealFontEnum(fontName) : REALFONT_UNKNOWN;
	return snapped_text_scale(scaled_menu_screen_scale(), font_glyph_px(font));
#else
	(void)fontStd;
	return 1.0f;
#endif
}

int scaled_menu_measure_text_width(const char* text, const char* fontStd) {
#if FEATURE_SCALED_MENU
	if (!text || !fontStd || !detour_R_Strlen::oR_Strlen) return 0;
	const int width =
	    detour_R_Strlen::oR_Strlen(const_cast<char*>(text), const_cast<char*>(fontStd));
	const float scale = scaled_menu_font_measure_scale(fontStd);
	return (scale == 1.0f)
	           ? width
	           : static_cast<int>(lroundf(scale * static_cast<float>(width)));
#else
	(void)text;
	(void)fontStd;
	return 0;
#endif
}

static float font_measure_scale(const char* fontStd) {
#if FEATURE_INTRO_SCALER
	// Startup violence disclaimer: its glyphs scale by the page-width ratio (never by
	// menuScale), so its measurement must follow the same ratio — unconditionally, so it
	// is correct regardless of the scaled-menu cvars.
	if (intro_scaler_disclaimer_menu_text_active()) return intro_scaler_page_width_scale();
	if (intro_scaler_intro_cinematic_active()) return 1.0f;
#endif
#if FEATURE_SCALED_MENU
	// Menu layout (centring, caret at x + width, clipping, row pitch, columns) is driven
	// by these measurements, so they must scale with the glyphs we draw — otherwise the
	// clip rects cut the scaled runs into slivers.
	if (RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_SCALED_MENU) &&
	    scaled_menu_uniform_actual_active()) {
		MenuMeasureCaller caller;
		HookCallsite::visitExternalCallers(pickMenuMeasureCaller, &caller);
		if (caller.is_menu) {
			return scaled_menu_font_measure_scale(fontStd);
		}
	}
#else
	(void)pickMenuMeasureCaller;
#endif
	return 1.0f;
}

int hkR_Strlen(char* str, char* fontStd, detour_R_Strlen::tR_Strlen original) {
	if (!original || !str || !fontStd) return 0;
	const float s = font_measure_scale(fontStd);
	const int w = original(str, fontStd);
	return s == 1.0f ? w : static_cast<int>(lroundf(s * static_cast<float>(w)));
}

int hkR_StrHeight(char* fontStd, detour_R_StrHeight::tR_StrHeight original) {
	if (!original || !fontStd) return 0;
	const float s = font_measure_scale(fontStd);
	const int h = original(fontStd);
	const int scaled_h =
	    s == 1.0f ? h : static_cast<int>(lroundf(s * static_cast<float>(h)));
	return scaled_h;
}

#endif // UI_MENU

#endif // FEATURE_SCALED_HUD || FEATURE_SCALED_MENU || FEATURE_INTRO_SCALER
