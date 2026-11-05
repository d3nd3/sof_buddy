#include "feature_config.h"

#if FEATURE_SCALED_HUD || FEATURE_SCALED_MENU || FEATURE_INTRO_SCALER

#include "sof_compat.h"
#include "util.h"
#include "generated_detours.h"
#include "../../scaled_ui_base/shared.h"
#include "debug/hook_callsite.h"
#include "runtime_features.h"
#if FEATURE_INTRO_SCALER
#include "../../intro_scaler/shared.h"
#endif
#include <math.h>
#include <string.h>

static bool s_dmRankingScorePhase = true;

static bool forceVanillaFont(void)
{
	cvar_t* pack = findCvar(const_cast<char*>("_sofbuddy_font_pack"));
	return pack && static_cast<int>(pack->value) == 2;
}

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
	if (fc == FontCaller::Unknown) {
		// Menu widget text outside the named-caller table (ticker such as input_c,
		// popup, loadbox, filebox, text_c, vbar and interface helpers): classify by the
		// menu IRVA region nexus so those runs get the uniform text scale too.
		if (is_menu_system_caller(info.module, (uint32_t)info.functionStartRva)) {
			*static_cast<FontCallerResolveCtx*>(raw)->out = FontCaller::MenuSystemText;
			return true;
		}
		return false;
	}
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
    if (!RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_SCALED_HUD) &&
        !RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_SCALED_MENU) &&
        !RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_SCALED_CON) &&
        !RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_INTRO_SCALER)) {
        original(screenX, screenY, text, colorPalette, font, rememberLastColor);
        return;
    }
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

	if (RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_SCALED_CON) &&
	    g_currentFontCaller == FontCaller::MissionStatus) {
		const float s = snapped_text_scale_active(fontScale);
		if (s != 1.0f) {
			const float vid_w = (viddef_width && *viddef_width > 0)
				? static_cast<float>(*viddef_width)
				: (current_vid_w > 0 ? static_cast<float>(current_vid_w) : 640.0f);
			const float textW = vid_w - 2.0f * static_cast<float>(screenX);
			screenX -= static_cast<int>(textW * (s - 1.0f) * 0.5f);
		}
	} else if (RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_SCALED_CON) &&
	           g_currentFontCaller == FontCaller::SCRDrawPause) {
		const float s = snapped_text_scale_active(fontScale);
		if (s != 1.0f) {
			const float textW = (float)(current_vid_w - 2 * screenX);
			const float textH = (float)(current_vid_h - 2 * screenY);
			screenX -= (int)(textW * (s - 1.0f) * 0.5f);
			screenY -= (int)(textH * (s - 1.0f) * 0.5f);
		}
	} else if (RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_SCALED_HUD) &&
	           g_activeRenderType == uiRenderType::HudDmRanking) {
		const float hs = snapped_text_scale_active(hudScale);
		using r_strlen_t = int (__cdecl *)(char*, char*);
		const r_strlen_t r_strlen = reinterpret_cast<r_strlen_t>(rvaToAbsRef((void*)0x000042F0));
		const int textWidth = (r_strlen && text && font)
			? r_strlen(text, font)
			: font_glyph_px(realFont) * (text ? strlen(text) : 0);
		const float halfTextWidth = hs * textWidth * 0.5f;
		int offsetEdge = 40;

		float x_scale = static_cast<float>(current_vid_w) / 640.0f;

		if (hudDmRanking_wasImage) {
			static int* s_dm_ranking_state = (int*)rvaToAbsExe((void*)0x001E7E94);
			if (s_dm_ranking_state && *s_dm_ranking_state == 7) {
				int half_text_len = screenX - (current_vid_w - (offsetEdge * x_scale));
				screenX = current_vid_w - offsetEdge * x_scale - 16 * hs + half_text_len * hs;
				screenY = 20 * screen_y_scale + 64 * hs + 6 * screen_y_scale;
			} else {
				screenX = current_vid_w - 16 * hs - offsetEdge * x_scale - halfTextWidth;
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
			screenX = current_vid_w - offsetEdge * x_scale - 16 * hs - halfTextWidth;
		}

	} else if (RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_SCALED_HUD) &&
	           g_activeRenderType == uiRenderType::HudInventory) {
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
		if (forceVanillaFont() && text && font) {
			using r_strlen_t = int (__cdecl *)(char*, char*);
			const r_strlen_t r_strlen = reinterpret_cast<r_strlen_t>(rvaToAbsRef((void*)0x000042F0));
			if (r_strlen) {
				const int textWidth = r_strlen(text, font);
				const int fixedWidth = font_glyph_px(realFont) * static_cast<int>(strlen(text));
				screenX -= static_cast<int>(roundf((textWidth - fixedWidth) * hs * 0.5f));
			}
		}
	} else if (RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_SCALED_HUD) &&
	           g_currentFontCaller == FontCaller::PlayerInfoName) {
		// Playername drawn by ref_gl Draw_PlayerInfo above a player. The caller centers the
		// string with the unscaled width, and the glyph vertices below grow it rightward from
		// the first char — so pre-shift left by half the expected growth to stay centered.
		extern bool g_iconsAutoscale;
		if (g_iconsAutoscale && text) {
			const float s = snapped_text_scale_active(hudScale);
			if (s != 1.0f) {
				const float w = (float)strlen(text) * (float)realFontSizes[realFont];
				screenX -= (int)(w * (s - 1.0f) * 0.5f);
			}
		}
	}
		// Menu text: SoF layout (R_Strlen / R_StrHeight, scaled by font_measure_scale) picks
		// the position; glyphs grow from the run start, so nothing shifts here. Small
		// black stamped text (the version string on the panel art) must stay stock-sized:
		// scaling it pushes it off its panel and makes it invisible.
		const bool stockStamp = scaled_menu_version_stamp(text, static_cast<int>(realFont));
		if (stockStamp) scaled_menu_text_scale_bypass(true);
		if (stockStamp) {
			// The version ctext sets a right-clip around its (scaled) rect; the stock-size
			// stamp then falls outside it. Give the stamp a full-screen scissor.
			auto** rr = reinterpret_cast<void**>(
			    rvaToAbsExe(reinterpret_cast<void*>(0x004035FC)));
			using DrawRenderRect_t = void(__cdecl*)(int, int, int, int);
			if (rr && *rr)
				reinterpret_cast<DrawRenderRect_t>(*rr)(0, 0, current_vid_w, current_vid_h);
		}
		original(screenX, screenY, text, colorPalette, font, rememberLastColor);
		if (stockStamp) scaled_menu_text_scale_bypass(false);

	if (g_currentFontCaller != FontCaller::MissionStatus &&
	    g_currentFontCaller != FontCaller::SCR_DrawCenterPrint)
		g_currentFontCaller = FontCaller::Unknown;
	characterIndex = 0;
	g_activeDrawCall = DrawRoutineType::None;
	resetGlVertexQuadState();
}

#endif // FEATURE_SCALED_HUD || FEATURE_SCALED_MENU || FEATURE_INTRO_SCALER
