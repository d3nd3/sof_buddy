#include "feature_config.h"

#if FEATURE_SCALED_CON || FEATURE_SCALED_HUD

#include <math.h>
#include "sof_compat.h"
#include "generated_detours.h"
#include "shared.h"

#if FEATURE_SCALED_CON
extern cvar_t* _sofbuddy_font_scale;
extern void fontscale_change(cvar_t* cvar);
#endif
#if FEATURE_SCALED_HUD
extern cvar_t* _sofbuddy_hud_scale;
extern void hudscale_change(cvar_t* cvar);
#endif

float scaleRoundRatio = 0.25f;
bool scaleRoundAuto = false;

float round_scale_value(float v) {
	if (scaleRoundRatio <= 0.0f) return v;
	return roundf(v / scaleRoundRatio) * scaleRoundRatio;
}

float effective_auto_scale(float raw) {
	if (raw <= 0.0f) raw = 1.0f;
	return scaleRoundAuto ? round_scale_value(raw) : raw;
}

// Snap scale so 8px reference glyphs land on whole pixels (768p: 1.6 -> 1.625).
float snap_font_scale_to_glyph_grid(float s) {
	if (s <= 1.0f) return s;
	return roundf(s * 8.0f) / 8.0f;
}

float snap_ui_pixel(float v) {
	return floorf(v + 0.5f);
}

static void refresh_scale_rounding(void) {
#if FEATURE_SCALED_CON
	if (_sofbuddy_font_scale) fontscale_change(_sofbuddy_font_scale);
#endif
#if FEATURE_SCALED_HUD
	if (_sofbuddy_hud_scale) hudscale_change(_sofbuddy_hud_scale);
#endif
}

void scale_round_auto_change(cvar_t* cvar) {
	scaleRoundAuto = cvar && cvar->value != 0.0f;
	if (cvar) refresh_scale_rounding();
}

void scale_round_ratio_change(cvar_t* cvar) {
	float r = cvar ? cvar->value : 0.25f;
	if (r < 0.05f) r = 0.05f;
	if (r > 1.0f) r = 1.0f;
	scaleRoundRatio = r;
	if (cvar) refresh_scale_rounding();
}

#endif
