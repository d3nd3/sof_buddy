/*
	Lighting Blend - CVars

	This file contains all cvar declarations and registration for the lighting_blend feature.
*/

#include "feature_config.h"

#if FEATURE_LIGHTING_BLEND

#include "shared.h"
#include "generated_detours.h"

#include <cstdio>

#ifndef NULL
#define NULL 0
#endif

// CVar declarations
cvar_t * _sofbuddy_lighting_overbright = NULL;
cvar_t * _sofbuddy_lighting_overbright_profile = NULL;
cvar_t * _sofbuddy_lighting_preset = NULL;
cvar_t * _sofbuddy_lighting_cutoff = NULL;
cvar_t * _sofbuddy_water_size = NULL;
cvar_t * _sofbuddy_lightblend_src = NULL;
cvar_t * _sofbuddy_lightblend_dst = NULL;
cvar_t * _sofbuddy_shiny_spherical = NULL;
cvar_t * gl_ext_multitexture = NULL;

cvar_t * _sofbuddy_lighting_curve[lighting_curve_detail::kCurvePointCount] = {};
cvar_t * _sofbuddy_lighting_overbright_curve[lighting_curve_detail::kCurvePointCount] = {};
cvar_t * _sofbuddy_lighting_preserve_color = NULL;
cvar_t * _sofbuddy_lighting_normalize = NULL;
cvar_t * _sofbuddy_lighting_normalize_up = NULL;
cvar_t * _sofbuddy_lighting_normalize_down = NULL;
cvar_t * _sofbuddy_lighting_ghoul_min_light = NULL;
cvar_t * _sb_internal_lighting_gl_modulate = NULL;
cvar_t * _sb_internal_lighting_gl_modulate_current = NULL;
cvar_t * _sb_internal_lighting_gl_modulate_pending = NULL;

static const char* kLightingCurveCvarNames[lighting_curve_detail::kCurvePointCount] = {
	"_sofbuddy_lighting_curve_0",
	"_sofbuddy_lighting_curve_10",
	"_sofbuddy_lighting_curve_20",
	"_sofbuddy_lighting_curve_30",
	"_sofbuddy_lighting_curve_40",
	"_sofbuddy_lighting_curve_50",
	"_sofbuddy_lighting_curve_60",
	"_sofbuddy_lighting_curve_70",
	"_sofbuddy_lighting_curve_80",
	"_sofbuddy_lighting_curve_90",
	"_sofbuddy_lighting_curve_100",
};

static const char* kLightingCurveDefaults[lighting_curve_detail::kCurvePointCount] = {
	"0", "10", "20", "30", "40", "50", "60", "70", "80", "90", "100"
};

static const char* kOverbrightCurveCvarNames[lighting_curve_detail::kCurvePointCount] = {
	"_sofbuddy_lighting_overbright_curve_0",
	"_sofbuddy_lighting_overbright_curve_10",
	"_sofbuddy_lighting_overbright_curve_20",
	"_sofbuddy_lighting_overbright_curve_30",
	"_sofbuddy_lighting_overbright_curve_40",
	"_sofbuddy_lighting_overbright_curve_50",
	"_sofbuddy_lighting_overbright_curve_60",
	"_sofbuddy_lighting_overbright_curve_70",
	"_sofbuddy_lighting_overbright_curve_80",
	"_sofbuddy_lighting_overbright_curve_90",
	"_sofbuddy_lighting_overbright_curve_100",
};
cvar_t * _sb_internal_lighting_multitexture_restore = NULL;
cvar_t * _sb_internal_lighting_normalize_status = NULL;
cvar_t * _sb_internal_lighting_style0_status = NULL;

void lighting_reset_curve_cvars(void) {
	if (!detour_Cvar_Set2::oCvar_Set2) {
		return;
	}
	for (int i = 0; i < lighting_curve_detail::kCurvePointCount; ++i) {
		detour_Cvar_Set2::oCvar_Set2(
			const_cast<char*>(kLightingCurveCvarNames[i]),
			const_cast<char*>(kLightingCurveDefaults[i]), true);
	}
}

void lighting_reset_overbright_curve_cvars(void) {
	if (!detour_Cvar_Set2::oCvar_Set2) {
		return;
	}
	const lighting_curve_detail::CurvePoints defaults =
		lighting_curve_detail::overbright_profile_points(
			lighting_curve_detail::kOverbrightProfileLateHighlights);
	for (int i = 0; i < lighting_curve_detail::kCurvePointCount; ++i) {
		char buffer[8];
		std::snprintf(buffer, sizeof(buffer), "%d",
		              static_cast<int>(defaults.p[i] + 0.5f));
		detour_Cvar_Set2::oCvar_Set2(
			const_cast<char*>(kOverbrightCurveCvarNames[i]), buffer, true);
	}
}

/*
	Create and register all lighting_blend cvars
*/
void create_lightingblend_cvars(void) {
	// Legacy switch kept so existing configs keep working. The F12 menu now
	// drives the preset below; setting this to 1 selects Raven 2x.
	_sofbuddy_lighting_overbright = detour_Cvar_Get::oCvar_Get("_sofbuddy_lighting_overbright","0",CVAR_SOFBUDDY_ARCHIVE,&lighting_overbright_change);
	_sofbuddy_lighting_overbright_profile = detour_Cvar_Get::oCvar_Get("_sofbuddy_lighting_overbright_profile","5",CVAR_SOFBUDDY_ARCHIVE,&lighting_overbright_profile_change);
	_sofbuddy_lighting_preset = detour_Cvar_Get::oCvar_Get("_sofbuddy_lighting_preset","0",CVAR_SOFBUDDY_ARCHIVE,&lighting_preset_change);
	_sofbuddy_lighting_cutoff = detour_Cvar_Get::oCvar_Get("_sofbuddy_lighting_cutoff","64",CVAR_SOFBUDDY_ARCHIVE,&lighting_cutoff_change);
	_sofbuddy_water_size = detour_Cvar_Get::oCvar_Get("_sofbuddy_water_size","64",CVAR_SOFBUDDY_ARCHIVE,&water_size_change);

	_sofbuddy_lightblend_src = detour_Cvar_Get::oCvar_Get("_sofbuddy_lightblend_src","GL_ZERO",CVAR_SOFBUDDY_ARCHIVE,&lightblend_change);
	_sofbuddy_lightblend_dst = detour_Cvar_Get::oCvar_Get("_sofbuddy_lightblend_dst","GL_SRC_COLOR",CVAR_SOFBUDDY_ARCHIVE,&lightblend_change);
	_sofbuddy_shiny_spherical = detour_Cvar_Get::oCvar_Get("_sofbuddy_shiny_spherical","1",CVAR_SOFBUDDY_ARCHIVE,&shiny_spherical_change);
	for (int i = 0; i < lighting_curve_detail::kCurvePointCount; ++i) {
		_sofbuddy_lighting_curve[i] = detour_Cvar_Get::oCvar_Get(
			kLightingCurveCvarNames[i], kLightingCurveDefaults[i],
			CVAR_SOFBUDDY_ARCHIVE, &lighting_curve_change);
	}
	// Separate curve for the Overbright toggle.  The default is the Late
	// highlights profile: half through 70% input, then a top-end lift.
	const lighting_curve_detail::CurvePoints overbright_defaults =
		lighting_curve_detail::overbright_profile_points(
			lighting_curve_detail::kOverbrightProfileLateHighlights);
	for (int i = 0; i < lighting_curve_detail::kCurvePointCount; ++i) {
		char default_value[8];
		std::snprintf(default_value, sizeof(default_value), "%d",
		              static_cast<int>(overbright_defaults.p[i] + 0.5f));
		_sofbuddy_lighting_overbright_curve[i] = detour_Cvar_Get::oCvar_Get(
			kOverbrightCurveCvarNames[i], default_value,
			CVAR_SOFBUDDY_ARCHIVE, &lighting_overbright_curve_change);
	}
	_sofbuddy_lighting_preserve_color = detour_Cvar_Get::oCvar_Get("_sofbuddy_lighting_preserve_color","1",CVAR_SOFBUDDY_ARCHIVE,&lighting_preserve_color_change);
	_sofbuddy_lighting_normalize = detour_Cvar_Get::oCvar_Get("_sofbuddy_lighting_normalize","0",CVAR_SOFBUDDY_ARCHIVE,&lighting_normalize_change);
	_sofbuddy_lighting_normalize_up = detour_Cvar_Get::oCvar_Get("_sofbuddy_lighting_normalize_up","100",CVAR_SOFBUDDY_ARCHIVE,&lighting_normalize_amount_change);
	_sofbuddy_lighting_normalize_down = detour_Cvar_Get::oCvar_Get("_sofbuddy_lighting_normalize_down","100",CVAR_SOFBUDDY_ARCHIVE,&lighting_normalize_amount_change);
	_sofbuddy_lighting_ghoul_min_light = detour_Cvar_Get::oCvar_Get("_sofbuddy_lighting_ghoul_min_light","0",CVAR_SOFBUDDY_ARCHIVE,&lighting_ghoul_min_light_change);
	// -1 = not forced by the feature; >=0 = value to restore when leaving a legacy blend.
	_sb_internal_lighting_multitexture_restore = detour_Cvar_Get::oCvar_Get("_sb_internal_lighting_multitexture_restore","-1",CVAR_SOFBUDDY_ARCHIVE,NULL);
	_sb_internal_lighting_normalize_status = detour_Cvar_Get::oCvar_Get("_sb_internal_lighting_normalize_status","",0,NULL);
	_sb_internal_lighting_style0_status = detour_Cvar_Get::oCvar_Get("_sb_internal_lighting_style0_status","",0,NULL);
	// F12 Lighting page snippet: the Custom-only manual blend gate.
	detour_Cvar_Get::oCvar_Get("_sb_internal_lighting_custom_section_rmf","",0,NULL);
	// Live normalization sliders: shown only when the active mode uses them.
	detour_Cvar_Get::oCvar_Get("_sb_internal_lighting_normalize_show_up","0",0,NULL);
	detour_Cvar_Get::oCvar_Get("_sb_internal_lighting_normalize_show_down","0",0,NULL);
	// Mirror for the F12 Lightmap Multiplier slider. The real gl_modulate only
	// changes after the warning popup is confirmed.
	_sb_internal_lighting_gl_modulate = detour_Cvar_Get::oCvar_Get(
		"_sb_internal_lighting_gl_modulate", "1", 0, &lighting_gl_modulate_change);
	_sb_internal_lighting_gl_modulate_current = detour_Cvar_Get::oCvar_Get(
		"_sb_internal_lighting_gl_modulate_current", "1", 0, NULL);
	_sb_internal_lighting_gl_modulate_pending = detour_Cvar_Get::oCvar_Get(
		"_sb_internal_lighting_gl_modulate_pending", "1", 0, NULL);
	gl_ext_multitexture = detour_Cvar_Get::oCvar_Get("gl_ext_multitexture","1",CVAR_SOFBUDDY_ARCHIVE,NULL);
}

#endif // FEATURE_LIGHTING_BLEND
