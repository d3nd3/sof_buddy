/*
	Scaled UI - CVars
	
	This file contains all cvar declarations and registration for the scaled_ui feature.
	
	CVars:
	- _sofbuddy_font_scale: Manual font scale; values above the resolution max use max (default: 1.0)
	- _sofbuddy_console_size: Console size as fraction of screen (default: 0.5)
	- _sofbuddy_hud_scale: Manual HUD scale; values above the resolution max use max (default: 1.0)
	- _sofbuddy_menu_scale: Manual menu scale; values above the resolution max use max (default: 1.0)
	- _sofbuddy_crossh_scale: Scale factor for crosshair textures (default: 1.0)
- _sofbuddy_icons_autoscale: Scale team icons + playernames with HUD scale (default: 0)
	- _sofbuddy_scale_round_auto: Snap cinematic image scale to round steps (default: 0)
	- _sofbuddy_scale_round_ratio: Round step for scale values (default: 0.25)
	- _sofbuddy_menu_tip_appear: milliseconds before a menu tooltip appears (default: 750)
	- _sofbuddy_menu_font_size_style: F12 menu text preset 0=Small, 1=Normal, 2=Large, 3=Extra Large (default: 0)
*/

#include "feature_config.h"

#if FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_MENU

#include "sof_buddy.h"
#include "sof_compat.h"
#include "generated_detours.h"
#include "runtime_features.h"
#include "sofbuddy_cfg.h"
#include "shared.h"
#include <cmath>
#include <cstdio>
#include <cstring>

cvar_t * _sofbuddy_scale_mode = NULL;

#if FEATURE_SCALED_CON
cvar_t * _sofbuddy_font_scale_pct = NULL;
cvar_t * _sofbuddy_font_scale = NULL;
cvar_t * _sofbuddy_console_size = NULL;
#endif
#if FEATURE_SCALED_MENU
cvar_t* _sofbuddy_menu_scale_pct = nullptr;
cvar_t* _sofbuddy_menu_scale = nullptr;
cvar_t* _sofbuddy_menu_tip_appear = nullptr;
cvar_t* _sofbuddy_menu_font_size_style = nullptr;
#endif
#if FEATURE_SCALED_HUD
cvar_t * _sofbuddy_hud_scale_pct = NULL;
cvar_t * _sofbuddy_hud_scale = NULL;
cvar_t * _sofbuddy_crossh_scale = NULL;
cvar_t * _sofbuddy_crossh_scale_auto = NULL;
cvar_t * _sofbuddy_scale_cinematic_pics = NULL;
cvar_t * _sofbuddy_icons_autoscale = NULL;
#endif
#if FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_MENU
cvar_t * _sofbuddy_font_pack = NULL;
#endif

bool g_applyingScalePercent = false;
constexpr int kCvarInteger = 0x20;

static void refresh_scaled_cvar_ptrs(void) {
    if (cvar_t* cv = findCvar(const_cast<char*>("_sofbuddy_scale_mode"))) _sofbuddy_scale_mode = cv;
#if FEATURE_SCALED_CON
    if (cvar_t* cv = findCvar(const_cast<char*>("_sofbuddy_font_scale_pct"))) _sofbuddy_font_scale_pct = cv;
    if (cvar_t* cv = findCvar(const_cast<char*>("_sofbuddy_font_scale"))) _sofbuddy_font_scale = cv;
#endif
#if FEATURE_SCALED_HUD
    if (cvar_t* cv = findCvar(const_cast<char*>("_sofbuddy_hud_scale_pct"))) _sofbuddy_hud_scale_pct = cv;
    if (cvar_t* cv = findCvar(const_cast<char*>("_sofbuddy_hud_scale"))) _sofbuddy_hud_scale = cv;
#endif
#if FEATURE_SCALED_MENU
    if (cvar_t* cv = findCvar(const_cast<char*>("_sofbuddy_menu_scale_pct"))) _sofbuddy_menu_scale_pct = cv;
    if (cvar_t* cv = findCvar(const_cast<char*>("_sofbuddy_menu_scale"))) _sofbuddy_menu_scale = cv;
#endif
}

#if FEATURE_SCALED_CON
extern void fontscale_change(cvar_t * cvar);
extern void consolesize_change(cvar_t * cvar);
#endif
#if FEATURE_SCALED_MENU
extern void menuscale_change(cvar_t* cvar);
#endif
#if FEATURE_SCALED_HUD
extern void hudscale_change(cvar_t * cvar);
extern void crosshairscale_change(cvar_t * cvar);
extern void crosshair_scale_auto_change(cvar_t * cvar);
extern bool g_scaleCinematicPics;
static void scalecinematicpics_change(cvar_t * cvar) {
    g_scaleCinematicPics = cvar && cvar->value != 0.0f;
}
extern void apply_auto_crosshair_scale(void);
extern bool g_iconsAutoscale;
static void iconsautoscale_change(cvar_t * cvar) {
    g_iconsAutoscale = cvar && cvar->value != 0.0f;
}
#endif
extern void scale_round_auto_change(cvar_t * cvar);
extern void scale_round_ratio_change(cvar_t * cvar);
extern float auto_scale_resolution_ratio(void);
extern float auto_scale_resolution_ratio_font(void);
extern float auto_scale_resolution_ratio_hud(void);
extern float round_scale_value(float v);

static bool scale_mode_is_smooth_cvar(const cvar_t* cvar) {
	if (!cvar) return true;
	const char* text = cvar->string;
	if (text && text[0]) {
		if (!std::strcmp(text, "Smooth")) return true;
		if (!std::strcmp(text, "Integer")) return false;
		return std::atof(text) >= 0.5;
	}
	return cvar->value >= 0.5f;
}

bool scale_mode_is_smooth(void) {
	cvar_t* mode = findCvar(const_cast<char*>("_sofbuddy_scale_mode"));
	return scale_mode_is_smooth_cvar(mode ? mode : _sofbuddy_scale_mode);
}

float scale_mode_apply(float scale) {
	if (g_applyingScalePercent)
		return scale;
	if (scale_mode_is_smooth())
		return scale;
	const float floored = std::floor(scale);
	return (floored < 1.0f) ? 1.0f : floored;
}

static float scale_smooth_value(float pct, float max_scale) {
	if (max_scale <= 1.0f || pct <= 0.0f)
		return 1.0f;
	if (max_scale < 2.0f)
		return 1.0f;
	if (pct >= 100.0f)
		return max_scale;
	const float raw = 1.0f + (max_scale - 1.0f) * (pct / 100.0f);
	if (raw < 2.0f)
		return std::round(raw);
	return raw;
}

static float scale_from_percent(float pct, float max_scale) {
	if (max_scale < 1.0f) max_scale = 1.0f;
	if (pct < 0.0f) pct = 0.0f;
	if (pct > 100.0f) pct = 100.0f;
	return scale_mode_apply(scale_smooth_value(pct, max_scale));
}

static void apply_percent_to_scale(cvar_t* pct, cvar_t* scale, float max_scale) {
	if (!pct || !scale || !detour_Cvar_Set2::oCvar_Set2) return;
	if (cvar_t* cv = findCvar(pct->name)) pct = cv;
	if (cvar_t* cv = findCvar(scale->name)) scale = cv;
	scale->flags &= ~kCvarInteger;
	char buf[32];
	std::snprintf(buf, sizeof(buf), "%.6g", scale_from_percent(pct->value, max_scale));
	g_applyingScalePercent = true;
	detour_Cvar_Set2::oCvar_Set2(scale->name, buf, true);
	g_applyingScalePercent = false;
}

static void font_scale_pct_change(cvar_t* cvar) {
	apply_percent_to_scale(cvar, _sofbuddy_font_scale, auto_scale_resolution_ratio_font());
}

static void hud_scale_pct_change(cvar_t* cvar) {
	apply_percent_to_scale(cvar, _sofbuddy_hud_scale, auto_scale_resolution_ratio_hud());
}

static void menu_scale_pct_change(cvar_t* cvar) {
	apply_percent_to_scale(cvar, _sofbuddy_menu_scale, auto_scale_resolution_ratio());
}

static void apply_integer_scale(cvar_t* scale, float max_scale) {
	if (!scale || !detour_Cvar_Set2::oCvar_Set2) return;
	if (cvar_t* cv = findCvar(scale->name)) scale = cv;
	float value = std::floor(scale->value);
	if (value < 1.0f) value = 1.0f;
	if (max_scale >= 1.0f && value > max_scale) value = max_scale;
	char buf[32];
	std::snprintf(buf, sizeof(buf), "%.0f", value);
	if (std::strcmp(scale->string ? scale->string : "", buf) != 0)
		detour_Cvar_Set2::oCvar_Set2(scale->name, buf, true);
}

static void reapply_scale_percentages_for_mode(bool integer_mode) {
	refresh_scaled_cvar_ptrs();
#if FEATURE_SCALED_CON
	if (RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_SCALED_CON)) {
		if (integer_mode)
			apply_integer_scale(_sofbuddy_font_scale, auto_scale_resolution_ratio_font());
		else if (_sofbuddy_font_scale_pct)
			font_scale_pct_change(_sofbuddy_font_scale_pct);
	}
#endif
#if FEATURE_SCALED_HUD
	if (RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_SCALED_HUD)) {
		if (integer_mode)
			apply_integer_scale(_sofbuddy_hud_scale, auto_scale_resolution_ratio_hud());
		else if (_sofbuddy_hud_scale_pct)
			hud_scale_pct_change(_sofbuddy_hud_scale_pct);
		apply_auto_crosshair_scale();
	}
#endif
#if FEATURE_SCALED_MENU
	if (RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_SCALED_MENU)) {
		if (integer_mode)
			apply_integer_scale(_sofbuddy_menu_scale, auto_scale_resolution_ratio());
		else if (_sofbuddy_menu_scale_pct)
			menu_scale_pct_change(_sofbuddy_menu_scale_pct);
	}
#endif
}

void reapply_scale_percentages(void) {
	reapply_scale_percentages_for_mode(!scale_mode_is_smooth());
}

static void scale_reset_default(cvar_t* pct, cvar_t* scale, float max_scale, bool integer_mode) {
	if (integer_mode) {
		float value = std::round(1.0f + (max_scale - 1.0f) * 0.75f);
		if (value < 1.0f) value = 1.0f;
		if (max_scale >= 1.0f && value > max_scale) value = max_scale;
		char buf[32];
		std::snprintf(buf, sizeof(buf), "%.0f", value);
		if (scale && detour_Cvar_Set2::oCvar_Set2) detour_Cvar_Set2::oCvar_Set2(scale->name, buf, true);
	} else if (pct && detour_Cvar_Set2::oCvar_Set2) {
		detour_Cvar_Set2::oCvar_Set2(pct->name, const_cast<char*>("75"), true);
	}
}

static void Cmd_SoFBuddy_ScaleReset_f(void) {
	if (!detour_Cmd_Argc::oCmd_Argc || !detour_Cmd_Argv::oCmd_Argv) return;
	if (detour_Cmd_Argc::oCmd_Argc() < 2) return;
	const char* target = detour_Cmd_Argv::oCmd_Argv(1);
	if (!target) return;
	const bool integer_mode = !scale_mode_is_smooth();
#if FEATURE_SCALED_CON
	if (std::strcmp(target, "font") == 0) {
		scale_reset_default(_sofbuddy_font_scale_pct, _sofbuddy_font_scale, auto_scale_resolution_ratio_font(), integer_mode);
		sofbuddy_cfg_save_now();
		return;
	}
#endif
#if FEATURE_SCALED_HUD
	if (std::strcmp(target, "hud") == 0) {
		scale_reset_default(_sofbuddy_hud_scale_pct, _sofbuddy_hud_scale, auto_scale_resolution_ratio_hud(), integer_mode);
		sofbuddy_cfg_save_now();
		return;
	}
#endif
#if FEATURE_SCALED_MENU
	if (std::strcmp(target, "menu") == 0) {
		scale_reset_default(_sofbuddy_menu_scale_pct, _sofbuddy_menu_scale, auto_scale_resolution_ratio(), integer_mode);
		sofbuddy_cfg_save_now();
		return;
	}
#endif
}

static void scale_mode_change(cvar_t* cvar) {
	if (cvar) _sofbuddy_scale_mode = cvar;
	refresh_scaled_cvar_ptrs();
	reapply_scale_percentages_for_mode(!scale_mode_is_smooth_cvar(cvar));
	scaled_ui_request_scale_reapply();
#if FEATURE_SCALED_MENU
	scaled_ui_request_menu_reload();
#endif
#if FEATURE_SCALED_HUD
	if (RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_SCALED_HUD))
		apply_auto_crosshair_scale();
#endif
}

static void font_pack_change(cvar_t*) {
    if (detour_Cmd_ExecuteString::oCmd_ExecuteString)
        detour_Cmd_ExecuteString::oCmd_ExecuteString("vid_restart");
}

static void migrate_legacy_negative_scale(cvar_t* scale) {
	if (!scale || !detour_Cvar_Set2::oCvar_Set2) return;
	if (scale->value < 0.0f)
		detour_Cvar_Set2::oCvar_Set2(scale->name, const_cast<char*>("1"), true);
}

void create_scaled_ui_cvars(void) {
    _sofbuddy_scale_mode = detour_Cvar_Get::oCvar_Get("_sofbuddy_scale_mode", "1", CVAR_SOFBUDDY_ARCHIVE, scale_mode_change);
    if (detour_Cmd_AddCommand::oCmd_AddCommand)
        detour_Cmd_AddCommand::oCmd_AddCommand(const_cast<char*>("sofbuddy_scale_reset"), Cmd_SoFBuddy_ScaleReset_f);
#if FEATURE_SCALED_CON
    if (RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_SCALED_CON)) {
    _sofbuddy_font_scale_pct = detour_Cvar_Get::oCvar_Get("_sofbuddy_font_scale_pct", "75", CVAR_SOFBUDDY_ARCHIVE, font_scale_pct_change);
    _sofbuddy_font_scale = detour_Cvar_Get::oCvar_Get("_sofbuddy_font_scale", "1", CVAR_SOFBUDDY_ARCHIVE, fontscale_change);
    migrate_legacy_negative_scale(_sofbuddy_font_scale);
    if (_sofbuddy_font_scale) _sofbuddy_font_scale->flags &= ~kCvarInteger;
    fontscale_change(_sofbuddy_font_scale);
    _sofbuddy_console_size = detour_Cvar_Get::oCvar_Get("_sofbuddy_console_size", "0.5", CVAR_SOFBUDDY_ARCHIVE, consolesize_change);
    detour_Cvar_Get::oCvar_Get("_sb_internal_font_scale_rounded", "1", 0, nullptr);
    }
#endif
#if FEATURE_SCALED_HUD
    if (RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_SCALED_HUD)) {
    _sofbuddy_hud_scale_pct = detour_Cvar_Get::oCvar_Get("_sofbuddy_hud_scale_pct", "75", CVAR_SOFBUDDY_ARCHIVE, hud_scale_pct_change);
    _sofbuddy_hud_scale = detour_Cvar_Get::oCvar_Get("_sofbuddy_hud_scale", "1", CVAR_SOFBUDDY_ARCHIVE, hudscale_change);
    migrate_legacy_negative_scale(_sofbuddy_hud_scale);
    if (_sofbuddy_hud_scale) _sofbuddy_hud_scale->flags &= ~kCvarInteger;
    hudscale_change(_sofbuddy_hud_scale);
    _sofbuddy_crossh_scale_auto = detour_Cvar_Get::oCvar_Get("_sofbuddy_crossh_scale_auto", "0", CVAR_SOFBUDDY_ARCHIVE, crosshair_scale_auto_change);
    crosshair_scale_auto_change(_sofbuddy_crossh_scale_auto);
    _sofbuddy_crossh_scale = detour_Cvar_Get::oCvar_Get("_sofbuddy_crossh_scale", "1", CVAR_SOFBUDDY_ARCHIVE, crosshairscale_change);
    crosshairscale_change(_sofbuddy_crossh_scale);
    _sofbuddy_scale_cinematic_pics = detour_Cvar_Get::oCvar_Get("_sofbuddy_scale_cinematic_pics", "1", CVAR_SOFBUDDY_ARCHIVE, scalecinematicpics_change);
    scalecinematicpics_change(_sofbuddy_scale_cinematic_pics);
    _sofbuddy_icons_autoscale = detour_Cvar_Get::oCvar_Get("_sofbuddy_icons_autoscale", "1", CVAR_SOFBUDDY_ARCHIVE, iconsautoscale_change);
    iconsautoscale_change(_sofbuddy_icons_autoscale);
    detour_Cvar_Get::oCvar_Get("_sb_internal_hud_scale_rounded", "1", 0, nullptr);
    }
#endif
#if FEATURE_SCALED_MENU
    if (RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_SCALED_MENU)) {
    _sofbuddy_menu_scale_pct = detour_Cvar_Get::oCvar_Get("_sofbuddy_menu_scale_pct", "75", CVAR_SOFBUDDY_ARCHIVE, menu_scale_pct_change);
    _sofbuddy_menu_scale = detour_Cvar_Get::oCvar_Get("_sofbuddy_menu_scale", "1", CVAR_SOFBUDDY_ARCHIVE, menuscale_change);
    _sofbuddy_menu_tip_appear = detour_Cvar_Get::oCvar_Get("_sofbuddy_menu_tip_appear", "750", CVAR_SOFBUDDY_ARCHIVE, nullptr);
    _sofbuddy_menu_font_size_style = detour_Cvar_Get::oCvar_Get("_sofbuddy_menu_font_size_style", "0", CVAR_SOFBUDDY_ARCHIVE, nullptr);
    migrate_legacy_negative_scale(_sofbuddy_menu_scale);
    if (_sofbuddy_menu_scale) _sofbuddy_menu_scale->flags &= ~kCvarInteger;
    menuscale_change(_sofbuddy_menu_scale);
    detour_Cvar_Get::oCvar_Get("_sb_internal_menu_scale_rounded", "1", 0, nullptr);
    }
#endif
#if FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_MENU
    if (RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_SCALED_CON) ||
        RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_SCALED_HUD)) {
        _sofbuddy_font_pack = detour_Cvar_Get::oCvar_Get("_sofbuddy_font_pack", "0", CVAR_SOFBUDDY_ARCHIVE, nullptr);
        if (_sofbuddy_font_pack) _sofbuddy_font_pack->command = font_pack_change;
        detour_Cvar_Get::oCvar_Get("_sofbuddy_scale_round_ratio", "0.25", CVAR_SOFBUDDY_ARCHIVE, scale_round_ratio_change);
        detour_Cvar_Get::oCvar_Get("_sofbuddy_scale_round_auto", "0", CVAR_SOFBUDDY_ARCHIVE, scale_round_auto_change);
    }
#endif
    refresh_scaled_cvar_ptrs();
    reapply_scale_percentages();
}

#endif // FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_MENU
