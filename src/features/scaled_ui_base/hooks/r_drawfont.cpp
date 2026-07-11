#include "feature_config.h"

#if FEATURE_SCALED_HUD || FEATURE_SCALED_MENU

#include "sof_compat.h"
#include "util.h"
#include "generated_detours.h"
#include "../../scaled_ui_base/shared.h"
#include "debug/hook_callsite.h"
#include <string.h>

static bool s_dmRankingScorePhase = true;

void resetDmRankingFontPhase(void)
{
    s_dmRankingScorePhase = true;
}

struct FontCallerResolveCtx {
    FontCaller* out;
};

static bool pickKnownFontCaller(const CallerInfo& info, void* raw)
{
    FontCaller fc = getFontCallerFrom(info.module, (uint32_t)info.functionStartRva);
    if (fc == FontCaller::Unknown) return false;
    *static_cast<FontCallerResolveCtx*>(raw)->out = fc;
    return true;
}

static FontCaller fontCallerFromRenderType(uiRenderType rt)
{
    switch (rt) {
    case uiRenderType::HudInventory: return FontCaller::Inventory2;
    case uiRenderType::HudDmRanking: return FontCaller::DMRankingCalcXY;
    default: return FontCaller::Unknown;
    }
}

void hkR_DrawFont(int screenX, int screenY, char * text, int colorPalette, char * font, bool rememberLastColor, detour_R_DrawFont::tR_DrawFont original) {
    resetGlVertexQuadState();
    g_activeDrawCall = DrawRoutineType::Font;
    if (g_currentFontCaller == FontCaller::Unknown) {
        g_currentFontCaller = fontCallerFromRenderType(g_activeRenderType);
        if (g_currentFontCaller == FontCaller::Unknown) {
            FontCallerResolveCtx ctx{&g_currentFontCaller};
            HookCallsite::visitExternalCallers(pickKnownFontCaller, &ctx);
        }
    }
    if (g_currentFontCaller == FontCaller::SCR_DrawCenterPrint) {
        static float s_cpLastY = 0.0f;
        if (g_centerPrintAnchorSeq != g_lastCenterPrintSeq) {
            g_centerPrintAnchorSeq = g_lastCenterPrintSeq;
            g_centerPrintAnchorY = static_cast<float>(screenY);
            s_cpLastY = static_cast<float>(screenY);
            g_centerPrintLineStep = 0.0f;
            g_centerPrintScaleSeq = 0;
            g_centerPrintBottomY = -1.0f;
        } else {
            if (g_centerPrintLineStep <= 0.0f && static_cast<float>(screenY) > s_cpLastY)
                g_centerPrintLineStep = static_cast<float>(screenY) - s_cpLastY;
            if (static_cast<float>(screenY) < g_centerPrintAnchorY)
                g_centerPrintAnchorY = static_cast<float>(screenY);
            s_cpLastY = static_cast<float>(screenY);
        }
    }

	SOFBUDDY_ASSERT(font != nullptr);
	if (font) {
		realFont = getRealFontEnum((char*)(* (int * )(font + 4)));
	} else {
		realFont = REALFONT_UNKNOWN;
	}

	if (g_currentFontCaller == FontCaller::MissionStatus) {
		const float s = snapped_text_scale_active(fontScale);
		if (s != 1.0f) {
			const float vid_w = (viddef_width && *viddef_width > 0)
				? static_cast<float>(*viddef_width)
				: (current_vid_w > 0 ? static_cast<float>(current_vid_w) : 640.0f);
			const float textW = vid_w - 2.0f * static_cast<float>(screenX);
			screenX -= static_cast<int>(textW * (s - 1.0f) * 0.5f);
		}
	} else if (g_currentFontCaller == FontCaller::SCRDrawPause) {
		const float s = snapped_text_scale_active(fontScale);
		if (s != 1.0f) {
			const float textW = (float)(current_vid_w - 2 * screenX);
			const float textH = (float)(current_vid_h - 2 * screenY);
			screenX -= (int)(textW * (s - 1.0f) * 0.5f);
			screenY -= (int)(textH * (s - 1.0f) * 0.5f);
		}
	} else if (g_activeRenderType == uiRenderType::HudDmRanking) {
		const float hs = snapped_text_scale_active(hudScale);
		int fontWidth = 12;
		int offsetEdge = 40;

		float x_scale = static_cast<float>(current_vid_w) / 640.0f;

		if (hudDmRanking_wasImage) {
			static int* s_dm_ranking_state = (int*)rvaToAbsExe((void*)0x001E7E94);
			if (s_dm_ranking_state && *s_dm_ranking_state == 7) {
				int half_text_len = screenX - (current_vid_w - (offsetEdge * x_scale));
				screenX = current_vid_w - offsetEdge * x_scale - 16 * hs + half_text_len * hs;
				screenY = 20 * screen_y_scale + 64 * hs + 6 * screen_y_scale;
			} else {
				screenX = current_vid_w - 16 * hs - offsetEdge * x_scale - hs * fontWidth * strlen(text) / 2;
				screenY = screenY + (hs - 1) * (32 + 3);

				if (s_dmRankingScorePhase) {
					s_dmRankingScorePhase = false;
				} else {
					screenY = screenY + (hs - 1) * (realFontSizes[realFont] + 3);
					s_dmRankingScorePhase = true;
				}
			}

		} else {
			if (s_dmRankingScorePhase) {
				s_dmRankingScorePhase = false;
			} else {
				screenY = screenY + (hs - 1) * (realFontSizes[realFont] + 3);
				s_dmRankingScorePhase = true;
			}
			screenX = current_vid_w - offsetEdge * x_scale - 16 * hs - hs * fontWidth * strlen(text) / 2;
		}

	} else if (g_activeRenderType == uiRenderType::HudInventory) {
		// Match cropped pics (raw hudScale); glyph snap drifts text off the panels.
		const float hs = hudScale;
		if ( hudInventory_wasItem ) {
			if (text && ( (text[0] >= '0' && text[0] <= '8') || (text[0] =='9' && text[1] != 'm') ) ) {
				float set_x,set_y;
				enumCroppedDrawMode before = hudCroppedEnum;
				hudCroppedEnum = ITEM_INVEN_BOTTOM;
				drawCroppedPicVertex(false,false,set_x,set_y);
				hudCroppedEnum = before;

				screenX = set_x + -52.0f * hs;
				screenY = set_y + -29.0f * hs;
			} else {
				float set_x,set_y;
				enumCroppedDrawMode before = hudCroppedEnum;
				hudCroppedEnum = ITEM_INVEN_SWITCH;
				set_x = screenX;
				set_y = screenY;
				drawCroppedPicVertex(true,false,set_x,set_y);
				hudCroppedEnum = before;

				screenX = set_x + -111.0f * hs;
				screenY = set_y;
			}
		} else {
			if (text && ( (text[0] >= '0' && text[0] <= '8') || (text[0] =='9' && text[1] != 'M') ) ) {
				float set_x,set_y;
				enumCroppedDrawMode before = hudCroppedEnum;
				hudCroppedEnum = GUN_AMMO_BOTTOM;

				drawCroppedPicVertex(false,false,set_x,set_y);
				hudCroppedEnum = before;

				screenX = set_x + -97.0f * hs;
				screenY = set_y + -29.0f * hs;
			}
			else {
				float set_x,set_y;
				enumCroppedDrawMode before = hudCroppedEnum;
				hudCroppedEnum = GUN_AMMO_SWITCH;
				set_x = screenX;
				set_y = screenY;
				drawCroppedPicVertex(true,false,set_x,set_y);
				hudCroppedEnum = before;

				screenX = set_x + -79.0f * hs;
				screenY = set_y;
			}
		}
	}
	original(screenX, screenY, text, colorPalette, font, rememberLastColor);

	if (g_currentFontCaller != FontCaller::MissionStatus &&
	    g_currentFontCaller != FontCaller::SCR_DrawCenterPrint)
		g_currentFontCaller = FontCaller::Unknown;
	characterIndex = 0;
	g_activeDrawCall = DrawRoutineType::None;
	resetGlVertexQuadState();
}

#endif // FEATURE_SCALED_HUD || FEATURE_SCALED_MENU
