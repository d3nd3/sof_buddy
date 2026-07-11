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
	
	if (isDrawingTeamicons) {
		orig_glVertex2f(x, y);
		return;
	}

	FontCaller caller = g_currentFontCaller;
	switch (caller) {
		case FontCaller::DMRankingCalcXY: {
			const float s = snapped_text_scale_active(hudScale);
			SOFBUDDY_ASSERT(s > 0.0f);
			if (scaleX) x = pivotx + (x - pivotx) * s;
			if (scaleY) y = pivoty + (y - pivoty) * s;
			orig_glVertex2f(x + (characterIndex * realFontSizes[realFont])*(s-1), y);
			break;
		}
		case FontCaller::Inventory2: {
			// Same scale as cropped inventory/ammo pics (no glyph snap).
			const float s = hudScale;
			SOFBUDDY_ASSERT(s > 0.0f);
			if (scaleX) x = pivotx + (x - pivotx) * s;
			if (scaleY) y = pivoty + (y - pivoty) * s;
			orig_glVertex2f(x + (characterIndex * realFontSizes[realFont])*(s-1), y);
			break;
		}
			
		case FontCaller::ScopeCalcXY:
			orig_glVertex2f(x, y);
			break;
			
		case FontCaller::SCRDrawPause: {
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
			{
			const float s = snapped_text_scale_active(screen_y_scale);
			if (scaleX) x = pivotx + (x - pivotx) * s;
			if (scaleY) y = pivoty + (y - pivoty) * s;
			orig_glVertex2f(x + (characterIndex * realFontSizes[realFont])*(s-1), y);
			}
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
