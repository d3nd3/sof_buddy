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
	- _sofbuddy_font_scale_auto_axis: Font max-scale axis 0=vid_w/640, 1=vid_h/480 (default: 1)
	- _sofbuddy_hud_scale_auto_axis: HUD max-scale axis 0=vid_w/640, 1=vid_h/480 (default: 1)
	- _sofbuddy_menu_tip_appear: milliseconds before a menu tooltip appears (default: 250)
	- _sofbuddy_menu_font_size_style: F12 menu text preset 0=Small, 1=Normal, 2=Large, 3=Extra Large (default: 0)
*/

#include "feature_config.h"

#if FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_MENU

#include "sof_buddy.h"
#include "sof_compat.h"
#include "generated_detours.h"
#include "runtime_features.h"
#include <cstdio>
#include <cstring>

#if FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_MENU
cvar_t* _sofbuddy_scale_auto_axis = nullptr;
#endif
#if FEATURE_SCALED_CON
cvar_t* _sofbuddy_font_scale_auto_axis = nullptr;
#endif
#if FEATURE_SCALED_HUD
cvar_t* _sofbuddy_hud_scale_auto_axis = nullptr;
#endif
#if FEATURE_SCALED_CON
cvar_t * _sofbuddy_font_scale = NULL;
cvar_t * _sofbuddy_console_size = NULL;
#endif
#if FEATURE_SCALED_MENU
cvar_t* _sofbuddy_menu_scale = nullptr;
cvar_t* _sofbuddy_menu_tip_appear = nullptr;
cvar_t* _sofbuddy_menu_font_size_style = nullptr;
#endif
#if FEATURE_SCALED_HUD
cvar_t * _sofbuddy_hud_scale = NULL;
cvar_t * _sofbuddy_crossh_scale = NULL;
cvar_t * _sofbuddy_crossh_scale_auto = NULL;
cvar_t * _sofbuddy_crossh_scale_auto_axis = NULL;
cvar_t * _sofbuddy_scale_cinematic_pics = NULL;
cvar_t * _sofbuddy_icons_autoscale = NULL;
#endif
#if FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_MENU
cvar_t * _sofbuddy_font_pack = NULL;
#endif

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
extern void crosshair_scale_auto_axis_change(cvar_t * cvar);
extern bool g_scaleCinematicPics;
static void scalecinematicpics_change(cvar_t * cvar) {
    g_scaleCinematicPics = cvar && cvar->value != 0.0f;
}
extern bool g_iconsAutoscale;
static void iconsautoscale_change(cvar_t * cvar) {
    g_iconsAutoscale = cvar && cvar->value != 0.0f;
}
#endif
extern void scale_round_auto_change(cvar_t * cvar);
extern void scale_round_ratio_change(cvar_t * cvar);
extern void scale_auto_axis_change(cvar_t* cvar);
extern void font_scale_auto_axis_change(cvar_t* cvar);
extern void hud_scale_auto_axis_change(cvar_t* cvar);
extern float auto_scale_resolution_ratio(void);
extern float auto_scale_resolution_ratio_font(void);
extern float auto_scale_resolution_ratio_hud(void);
extern float round_scale_value(float v);

static void apply_scale_preset(cvar_t* scale, const char* preset, float maxScale) {
	if (!scale || !preset || !detour_Cvar_Set2::oCvar_Set2) return;
	float value;
	if (std::strcmp(preset, "auto") == 0) {
		value = round_scale_value(maxScale * 0.75f);
		if (value > maxScale) value = maxScale;
	} else if (std::strcmp(preset, "max") == 0)
		value = maxScale;
	else if (std::strcmp(preset, "min") == 0)
		value = 1.0f;
	else
		return;
	char buf[32];
	std::snprintf(buf, sizeof(buf), "%.6g", value);
	detour_Cvar_Set2::oCvar_Set2(scale->name, buf, true);
}

static void Cmd_SoFBuddy_ScalePreset_f(void) {
	if (!detour_Cmd_Argc::oCmd_Argc || !detour_Cmd_Argv::oCmd_Argv) return;
	if (detour_Cmd_Argc::oCmd_Argc() < 3) return;
	const char* target = detour_Cmd_Argv::oCmd_Argv(1);
	const char* preset = detour_Cmd_Argv::oCmd_Argv(2);
	if (!target || !preset) return;
#if FEATURE_SCALED_CON
	if (std::strcmp(target, "font") == 0) {
		apply_scale_preset(_sofbuddy_font_scale, preset, auto_scale_resolution_ratio_font());
		return;
	}
#endif
#if FEATURE_SCALED_HUD
	if (std::strcmp(target, "hud") == 0) {
		apply_scale_preset(_sofbuddy_hud_scale, preset, auto_scale_resolution_ratio_hud());
		return;
	}
#endif
#if FEATURE_SCALED_MENU
	if (std::strcmp(target, "menu") == 0) {
		apply_scale_preset(_sofbuddy_menu_scale, preset, auto_scale_resolution_ratio());
		return;
	}
#endif
}

static void migrate_split_auto_axis(void) {
	if (!detour_Cvar_Get::oCvar_Get || !detour_Cvar_Set2::oCvar_Set2) return;
	cvar_t* flag = detour_Cvar_Get::oCvar_Get("_sb_internal_axis_split_migrated", "0", CVAR_SOFBUDDY_ARCHIVE, nullptr);
	if (flag && flag->value != 0.0f) return;
	cvar_t* legacy = detour_Cvar_Get::oCvar_Get("_sofbuddy_scale_auto_axis", "1", 0, nullptr);
	if (legacy && legacy->modified) {
		char buf[16];
		std::snprintf(buf, sizeof(buf), "%.0f", legacy->value);
#if FEATURE_SCALED_CON
		if (_sofbuddy_font_scale_auto_axis && !_sofbuddy_font_scale_auto_axis->modified)
			detour_Cvar_Set2::oCvar_Set2(const_cast<char*>("_sofbuddy_font_scale_auto_axis"), buf, true);
#endif
#if FEATURE_SCALED_HUD
		if (_sofbuddy_hud_scale_auto_axis && !_sofbuddy_hud_scale_auto_axis->modified)
			detour_Cvar_Set2::oCvar_Set2(const_cast<char*>("_sofbuddy_hud_scale_auto_axis"), buf, true);
#endif
	}
	detour_Cvar_Set2::oCvar_Set2(const_cast<char*>("_sb_internal_axis_split_migrated"), const_cast<char*>("1"), true);
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

static bool scale_preset_command_registered = false;

static void register_scale_preset_command(void) {
	if (scale_preset_command_registered || !detour_Cmd_AddCommand::oCmd_AddCommand) return;
	detour_Cmd_AddCommand::oCmd_AddCommand(const_cast<char*>("sofbuddy_scale_preset"),
	                                       Cmd_SoFBuddy_ScalePreset_f);
	scale_preset_command_registered = true;
}

void create_scaled_ui_cvars(void) {
#if FEATURE_SCALED_CON
    if (RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_SCALED_CON)) {
    _sofbuddy_font_scale_auto_axis = detour_Cvar_Get::oCvar_Get("_sofbuddy_font_scale_auto_axis", "1", CVAR_SOFBUDDY_ARCHIVE, font_scale_auto_axis_change);
    font_scale_auto_axis_change(_sofbuddy_font_scale_auto_axis);
    _sofbuddy_font_scale = detour_Cvar_Get::oCvar_Get("_sofbuddy_font_scale", "1", CVAR_SOFBUDDY_ARCHIVE, fontscale_change);
    migrate_legacy_negative_scale(_sofbuddy_font_scale);
    fontscale_change(_sofbuddy_font_scale);
    _sofbuddy_console_size = detour_Cvar_Get::oCvar_Get("_sofbuddy_console_size", "0.5", CVAR_SOFBUDDY_ARCHIVE, consolesize_change);
    detour_Cvar_Get::oCvar_Get("_sb_internal_font_scale_rounded", "1", 0, nullptr);
    }
#endif
#if FEATURE_SCALED_HUD
    if (RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_SCALED_HUD)) {
    _sofbuddy_hud_scale_auto_axis = detour_Cvar_Get::oCvar_Get("_sofbuddy_hud_scale_auto_axis", "1", CVAR_SOFBUDDY_ARCHIVE, hud_scale_auto_axis_change);
    hud_scale_auto_axis_change(_sofbuddy_hud_scale_auto_axis);
    _sofbuddy_hud_scale = detour_Cvar_Get::oCvar_Get("_sofbuddy_hud_scale", "1", CVAR_SOFBUDDY_ARCHIVE, hudscale_change);
    migrate_legacy_negative_scale(_sofbuddy_hud_scale);
    hudscale_change(_sofbuddy_hud_scale);
    _sofbuddy_crossh_scale_auto_axis = detour_Cvar_Get::oCvar_Get("_sofbuddy_crossh_scale_auto_axis", "1", CVAR_SOFBUDDY_ARCHIVE, crosshair_scale_auto_axis_change);
    crosshair_scale_auto_axis_change(_sofbuddy_crossh_scale_auto_axis);
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
    _sofbuddy_scale_auto_axis = detour_Cvar_Get::oCvar_Get("_sofbuddy_scale_auto_axis", "1", 0, scale_auto_axis_change);
    scale_auto_axis_change(_sofbuddy_scale_auto_axis);
    _sofbuddy_menu_scale = detour_Cvar_Get::oCvar_Get("_sofbuddy_menu_scale", "1", CVAR_SOFBUDDY_ARCHIVE, menuscale_change);
    _sofbuddy_menu_tip_appear = detour_Cvar_Get::oCvar_Get("_sofbuddy_menu_tip_appear", "250", CVAR_SOFBUDDY_ARCHIVE, nullptr);
    _sofbuddy_menu_font_size_style = detour_Cvar_Get::oCvar_Get("_sofbuddy_menu_font_size_style", "0", CVAR_SOFBUDDY_ARCHIVE, nullptr);
    migrate_legacy_negative_scale(_sofbuddy_menu_scale);
    menuscale_change(_sofbuddy_menu_scale);
    detour_Cvar_Get::oCvar_Get("_sb_internal_menu_scale_rounded", "1", 0, nullptr);
    }
#endif
#if FEATURE_SCALED_CON || FEATURE_SCALED_HUD
    migrate_split_auto_axis();
#if FEATURE_SCALED_CON
    if (_sofbuddy_font_scale_auto_axis) font_scale_auto_axis_change(_sofbuddy_font_scale_auto_axis);
#endif
#if FEATURE_SCALED_HUD
    if (_sofbuddy_hud_scale_auto_axis) hud_scale_auto_axis_change(_sofbuddy_hud_scale_auto_axis);
#endif
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
    register_scale_preset_command();
}

#endif // FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_MENU
