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
#include <fstream>
#include <sstream>
#include <ctime>
#if defined(_WIN32)
#include <windows.h>
#endif

static float pivotx;
static float pivoty;
int characterIndex = 0;

FontCaller g_currentFontCaller = FontCaller::Unknown;

float scrCenterPrint_lineStartX = 0.0f;
float scrCenterPrint_lineY = 0.0f;
float scrCenterPrint_charWidth = 0.0f;
float scrCenterPrint_initialX = 0.0f;
float scrCenterPrint_originalPivotX = 0.0f;
float scrCenterPrint_originalPivotY = 0.0f;
float scrCenterPrint_currentCharVertex1X = 0.0f;
float scrCenterPrint_currentCharVertex1Y = 0.0f;
float scrCenterPrint_currentCharPivotX = 0.0f;
float scrCenterPrint_currentCharPivotY = 0.0f;
float scrCenterPrint_currentCharVertex1FinalX = 0.0f;
float scrCenterPrint_currentCharVertex1FinalY = 0.0f;
int scrCenterPrint_currentCharIndex = 0; // Store characterIndex for current character (used after wrap)

inline void handleFontVertex(float x, float y, bool scaleX, bool scaleY, bool incrementChar) {
	SOFBUDDY_ASSERT(orig_glVertex2f != nullptr);
	
	if (isDrawingTeamicons) {
		orig_glVertex2f(x, y);
		return;
	}

	FontCaller caller = g_currentFontCaller;
	// #region agent log
	{
		std::ofstream log("/home/dinda/git-projects/d3nd3/public/sof/sof_buddy/.cursor/debug.log", std::ios::app);
		if (log.is_open() && caller == FontCaller::SCR_DrawCenterPrint) {
			std::stringstream ss;
			ss << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"A\",\"location\":\"text_scaling.cpp:46\",\"message\":\"handleFontVertex SCR_DrawCenterPrint\",\"data\":{\"caller\":" << (int)caller << ",\"x\":" << x << ",\"y\":" << y << ",\"scaleX\":" << scaleX << ",\"scaleY\":" << scaleY << ",\"incrementChar\":" << incrementChar << ",\"charWidth\":" << scrCenterPrint_charWidth << ",\"charIndex\":" << characterIndex << "},\"timestamp\":" << (long long)(time(nullptr)*1000) << "}\n";
			log << ss.str();
		}
	}
	// #endregion
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
				float cx = current_vid_w * 0.5f;
				x = cx + (x - cx) * s;
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

