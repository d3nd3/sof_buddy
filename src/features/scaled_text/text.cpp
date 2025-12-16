/*
	Scaled UI - Text Scaling Functions

	This file handles non-console text scaling:
	- Generic HUD/menu text vertex scaling (ref.dll R_DrawFont)
	- Menu text measurement scaling (R_Strlen/R_StrHeight)
*/

#include "feature_config.h"

#if FEATURE_SCALED_TEXT

#include "features.h"
#include "generated_detours.h"
using detour_R_DrawFont::oR_DrawFont;
#include "../scaled_ui_base/shared.h"
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

// FontCaller enum provided by caller_from.h

FontCaller g_currentFontCaller = FontCaller::Unknown;

inline void handleFontVertex(float x, float y, bool scaleX, bool scaleY, bool incrementChar) {
	SOFBUDDY_ASSERT(orig_glVertex2f != nullptr);
	
	if (isDrawingTeamicons) {
		// Don't scale these for now.
		orig_glVertex2f(x, y);
		return;
	}

	FontCaller caller = g_currentFontCaller;
	switch (caller) {
		case FontCaller::DMRankingCalcXY:
		case FontCaller::Inventory2:
			// PrintOut(PRINT_LOG, "DMRankingCalcXY or Inventory2\n");
			SOFBUDDY_ASSERT(hudScale > 0.0f);
			if (scaleX) x = pivotx + (x - pivotx) * hudScale;
			if (scaleY) y = pivoty + (y - pivoty) * hudScale;
			orig_glVertex2f(x + (characterIndex * realFontSizes[realFont])*(hudScale-1), y);
			break;
			
		case FontCaller::ScopeCalcXY:
			orig_glVertex2f(x, y);
			break;
			
		case FontCaller::SCRDrawPause:
		case FontCaller::SCRUpdateScreen:
			SOFBUDDY_ASSERT(screen_y_scale > 0.0f);
			if (scaleX) x = pivotx + (x - pivotx) * screen_y_scale;
			if (scaleY) y = pivoty + (y - pivoty) * screen_y_scale;
			orig_glVertex2f(x + (characterIndex * realFontSizes[realFont])*(screen_y_scale-1), y);
			break;
			
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

#include <stddef.h>

/*
    This function is called internally by:
    - SCR_DrawPlayerInfo() - teamicons (WE DONT SCALE)
    - cScope::Draw() (SCALE IN glVertex2f_DrawFont_1)
    - cCountdown::Draw() (SCALE IN glVertex2f_DrawFont_1)
    - cDMRanking::Draw() (WE SCALE BELOW)
    - cInventory2::Draw() (WE SCALE BELOW)
    - cInfoTicker::Draw() (SCALE IN glVertex2f_DrawFont_1)
    - cMissionStatus::Draw() (SCALE IN glVertex2f_DrawFont_1)
    - SCR_DrawPause() (SCALE IN glVertex2f_DrawFont_1)
    - rect_c::DrawTextItem() (SCALE IN glVertex2f_DrawFont_1)
    - loadbox_c::Draw() (SCALE IN glVertex2f_DrawFont_1)
    - various other menu items (SCALE IN glVertex2f_DrawFont_1)

    Anything text/font that is scaled must be re-aligned.
    This function is centering the dmRanking text.
    And left aligning the item/gun ui/hud text.

	0x20006dc0 - cScope::CalcXY
	0x20007af0 - cDMRanking::CalcXY
	0x20008260 - cInventory2()
	0x20013710 - SCR_DrawPause()
	0x200163c0 - ? Inside SCR_UpdateScreen()
	0x200cecc0 - rect_c::DrawTextItem()
	0x200cf0e0 - rect_c::DrawTextLine()
	0x200d19f0 - ticker_c::Draw()
	0x200d4060 - input_c::Handle() ?
	0x200d9f80 - filebox_c::Handle()
	0x200dbf10 - loadbox_c::GetIndices()
	0x200de430 - serverbox_c::Draw()
	0x200ea8a0 - tip_c::Render()
	0x30002a40 - Draw_Line
*/

#endif // FEATURE_SCALED_TEXT


