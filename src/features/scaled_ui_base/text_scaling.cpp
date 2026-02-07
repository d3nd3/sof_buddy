/*
	Scaled UI - Text Scaling Functions

	This file handles non-console text scaling:
	- Generic HUD/menu text vertex scaling (ref.dll R_DrawFont)
	- Menu text measurement scaling (R_Strlen/R_StrHeight)
*/

#include "feature_config.h"

#if FEATURE_SCALED_HUD || FEATURE_SCALED_MENU

#include "features.h"
#include "generated_detours.h"
using detour_R_DrawFont::oR_DrawFont;
#include "shared.h"
#include "util.h"
#include "debug/hook_callsite.h"

#include <math.h>
#include <stdint.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#endif

static float pivotx;
static float pivoty;
int characterIndex = 0;

FontCaller g_currentFontCaller = FontCaller::Unknown;

inline void handleFontVertex(float x, float y, bool scaleX, bool scaleY, bool incrementChar) {
	SOFBUDDY_ASSERT(orig_glVertex2f != nullptr);
	
	if (isDrawingTeamicons) {
		orig_glVertex2f(x, y);
		return;
	}

	FontCaller caller = g_currentFontCaller;
	switch (caller) {
		case FontCaller::DMRankingCalcXY:
		case FontCaller::Inventory2:
			SOFBUDDY_ASSERT(hudScale > 0.0f);
			if (scaleX) x = pivotx + (x - pivotx) * hudScale;
			if (scaleY) y = pivoty + (y - pivoty) * hudScale;
			orig_glVertex2f(x + (characterIndex * realFontSizes[realFont])*(hudScale-1), y);
			break;
			
		case FontCaller::ScopeCalcXY:
			orig_glVertex2f(x, y);
			break;
			
		case FontCaller::SCRDrawPause:
			SOFBUDDY_ASSERT(screen_y_scale > 0.0f);
			if (scaleX) x = pivotx + (x - pivotx) * screen_y_scale;
			if (scaleY) y = pivoty + (y - pivoty) * screen_y_scale;
			orig_glVertex2f(x + (characterIndex * realFontSizes[realFont])*(screen_y_scale-1), y);
			break;

		case FontCaller::SCR_DrawCenterPrint: {
			float s = fontScale;
			if (s <= 0.0f) s = 1.0f;
			if (s != 1.0f) {
				// Centerprint can arrive in two coordinate domains depending on caller path:
				// - virtual 640x480 space (common in legacy UI paths)
				// - real render resolution space
				// Using real-res anchors for virtual-space vertices can push text off-screen.
				const bool hi_res = (current_vid_w > 640 || current_vid_h > 480);
				const bool looks_virtual = hi_res && x <= 700.0f && y <= 520.0f;

				const float cx = looks_virtual ? 320.0f : (current_vid_w * 0.5f);
				const float domain_h = looks_virtual ? 480.0f : (current_vid_h > 0 ? static_cast<float>(current_vid_h) : 480.0f);

				// Top line anchor captured from R_DrawFont for the active centerprint payload.
				float top = 0.0f;
				if (g_centerPrintAnchorSeq == g_lastCenterPrintSeq && g_centerPrintAnchorY > 0.0f) {
					top = g_centerPrintAnchorY;
				} else {
					// Fallback if anchor capture was unavailable.
					top = (y < 96.0f) ? 48.0f : (looks_virtual ? 168.0f : (current_vid_h * 0.35f));
				}

				int lines = g_lastCenterPrintLineCount;
				if (lines < 1) lines = 1;
				float font_h = static_cast<float>(realFontSizes[realFont]);
				if (font_h <= 0.0f) font_h = 8.0f;

				const float block_h = (lines > 0) ? (font_h * lines) : font_h;
				float s_eff = s;
				// Guarantee vertical fit when possible.
				if (block_h > 1.0f) {
					const float fit_scale = (domain_h - 2.0f) / block_h;
					if (fit_scale > 0.0f && s_eff > fit_scale) {
						s_eff = fit_scale;
					}
				}
				if (s_eff < 0.1f) s_eff = 0.1f;

				const float cy = top + block_h * 0.5f;
				const float scaled_h = block_h * s_eff;
				float top_scaled = cy - scaled_h * 0.5f;
				float bottom_scaled = cy + scaled_h * 0.5f;

				float y_shift = 0.0f;
				const float margin = 1.0f;
				if (top_scaled < margin) {
					y_shift = margin - top_scaled;
				}
				if (bottom_scaled + y_shift > domain_h - margin) {
					y_shift -= (bottom_scaled + y_shift) - (domain_h - margin);
				}

				x = cx + (x - cx) * s_eff;
				y = cy + (y - cy) * s_eff + y_shift;
			}
			orig_glVertex2f(x, y);
			break;
		}
			
		case FontCaller::DrawLine:
			orig_glVertex2f(x, y);
			break;
			
#ifdef UI_MENU
		case FontCaller::RectDrawTextItem:
		case FontCaller::RectDrawTextLine:
		case FontCaller::TickerDraw:
		case FontCaller::InputHandle:
		case FontCaller::FileboxHandle:
		case FontCaller::LoadboxGetIndices:
		case FontCaller::ServerboxDraw:
		case FontCaller::TipRender:
			SOFBUDDY_ASSERT(screen_y_scale > 0.0f);
			if (scaleX) x = pivotx + (x - pivotx) * screen_y_scale;
			if (scaleY) y = pivoty + (y - pivoty) * screen_y_scale;
			orig_glVertex2f(x + (characterIndex * realFontSizes[realFont])*(screen_y_scale-1), y);
			break;
#endif
			
		default:
			orig_glVertex2f(x, y);
			break;
	}
	
	if (incrementChar) characterIndex++;
}

void __stdcall my_glVertex2f_DrawFont_1(float x, float y) {
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

int hkR_Strlen(char * str, char * fontStd)
{
	SOFBUDDY_ASSERT(str != nullptr);
	SOFBUDDY_ASSERT(fontStd != nullptr);
	SOFBUDDY_ASSERT(screen_y_scale > 0.0f);
	
	int space_count = 0;
	for (char *p = str; *p != '\0'; ++p) {
		if (*p == ' ') {
			space_count++;
		}
	}

	extern int (__cdecl *oR_Strlen)(char*, char*);
	SOFBUDDY_ASSERT(oR_Strlen != nullptr);
	return screen_y_scale * ( oR_Strlen(str, fontStd) - 5 * space_count);
}

int hkR_StrHeight(char * fontStd)
{
	SOFBUDDY_ASSERT(fontStd != nullptr);
	SOFBUDDY_ASSERT(screen_y_scale > 0.0f);
	
	extern int (__cdecl *oR_StrHeight)(char*);
	SOFBUDDY_ASSERT(oR_StrHeight != nullptr);
	return screen_y_scale * oR_StrHeight(fontStd);
}

#endif // UI_MENU

#endif // FEATURE_SCALED_HUD || FEATURE_SCALED_MENU
