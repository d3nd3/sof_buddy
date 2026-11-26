#pragma once

#include "feature_config.h"

#if FEATURE_LIGHTING_BLEND

#include "sof_compat.h"
#include "lighting_curve.h"

extern int lightblend_src;
extern int lightblend_dst;

extern int *lightblend_target_src;
extern int *lightblend_target_dst;
extern float *lighting_cutoff_target;
extern double *water_size_double_target;
extern float *water_size_float_target;
extern unsigned char *shiny_spherical_target1;
extern unsigned char *shiny_spherical_target3;
extern unsigned char *shiny_pass_target;
extern float *shiny_flat_threshold_target;

extern void (__stdcall *real_glBlendFunc)(unsigned int, unsigned int);
extern void (__cdecl *oAdjustTexCoords)(float*, float*, float*, float*, float*, float*);
extern void (__stdcall *glTexCoord2f)(float, float);
extern void (__stdcall *glVertex3f)(float, float, float);
extern void (__stdcall *glBegin)(unsigned int);
extern void (__stdcall *glEnd)(void);
extern void (__stdcall *glColor3f)(float, float, float);

extern cvar_t * _sofbuddy_lighting_overbright;
extern cvar_t * _sofbuddy_lighting_overbright_profile;
extern cvar_t * _sofbuddy_lighting_preset;
extern cvar_t * _sofbuddy_lighting_cutoff;
extern cvar_t * _sofbuddy_water_size;
extern cvar_t * _sofbuddy_lightblend_src;
extern cvar_t * _sofbuddy_lightblend_dst;
extern cvar_t * _sofbuddy_shiny_spherical;
extern cvar_t * gl_ext_multitexture;

// Custom lightmap curve: 11 control points at 0,10,...,100% input.
// Outputs are also 0..100. 100 = white; lower inputs mapped to 100 are
// overbright (e.g. 0,20,40,60,80,100,100,100,100,100,100 is 2x).
extern cvar_t * _sofbuddy_lighting_curve[lighting_curve_detail::kCurvePointCount];
// Separate 11-point curve applied before the Overbright x2.  Identity by
// default, so enabling Overbright alone still produces the stock 2*T*L product.
extern cvar_t * _sofbuddy_lighting_overbright_curve[lighting_curve_detail::kCurvePointCount];
// UI-only mirror of the overbright curve.  The stored curve stays 0..100 but
// the editor shows the final brightness after the x2 product (0..200%).
extern cvar_t * _sb_internal_lighting_overbright_display[lighting_curve_detail::kCurvePointCount];
extern cvar_t * _sofbuddy_lighting_preserve_color;
extern cvar_t * _sofbuddy_lighting_normalize;
extern cvar_t * _sofbuddy_lighting_normalize_up;
extern cvar_t * _sofbuddy_lighting_normalize_down;
extern cvar_t * _sofbuddy_lighting_ghoul_min_light;

extern cvar_t * _sb_internal_lighting_multitexture_restore;
extern cvar_t * _sb_internal_lighting_normalize_status;
extern cvar_t * _sb_internal_lighting_style0_status;
extern float** lighting_lightstyles_ptr;
extern unsigned char (__cdecl *lighting_set_cache_state)(int surf);
extern cvar_t * _sb_internal_lighting_gl_modulate;
extern cvar_t * _sb_internal_lighting_gl_modulate_current;
extern cvar_t * _sb_internal_lighting_gl_modulate_pending;

extern bool is_blending;

enum lighting_preset_e {
	LIGHTING_PRESET_VANILLA = 0,
	LIGHTING_PRESET_OVERBRIGHT_2X = 1,
	LIGHTING_PRESET_OVERBRIGHT_4X = 2,
	LIGHTING_PRESET_SOFT_GLOW = 3,
	LIGHTING_PRESET_RAVEN_2X = 4,
	LIGHTING_PRESET_CUSTOM = 5,
	LIGHTING_PRESET_HIGH_CONTRAST = 6,
	LIGHTING_PRESET_DEEP_SHADOWS = 7,
	LIGHTING_PRESET_FILM_SLOPE = 8,
	LIGHTING_PRESET_CLIFF_EDGE = 9,
	LIGHTING_PRESET_BRIGHT_PUNCH = 10,
	LIGHTING_PRESET_MIDNIGHT = 11,
	LIGHTING_PRESET_COUNT = 12
};

enum lighting_color_mode_e {
	LIGHTING_COLOR_PER_CHANNEL = 0,
	LIGHTING_COLOR_PRESERVE_BRIGHTNESS = 1,
	LIGHTING_COLOR_PRESERVE_COLOUR = 2,
	LIGHTING_COLOR_COUNT = 3
};

void create_lightingblend_cvars(void);
void lighting_reset_overbright_curve_cvars(void);
void lightblend_change(cvar_t * cvar);
void lighting_overbright_change(cvar_t * cvar);
void lighting_preset_change(cvar_t * cvar);
void lighting_curve_change(cvar_t * cvar);
void lighting_overbright_curve_change(cvar_t * cvar);
void lighting_overbright_display_change(cvar_t * cvar);
void lighting_overbright_profile_change(cvar_t * cvar);
void lighting_preserve_color_change(cvar_t * cvar);
void lighting_normalize_change(cvar_t * cvar);
void lighting_normalize_amount_change(cvar_t * cvar);
void lighting_ghoul_min_light_change(cvar_t * cvar);
void lighting_gl_modulate_change(cvar_t * cvar);
void lighting_gl_modulate_note_menu(const char * menu_file);
void lighting_gl_modulate_menu_closed(void);
void lighting_cutoff_change(cvar_t * cvar);
void water_size_change(cvar_t * cvar);
void shiny_spherical_change(cvar_t * cvar);
void shiny_spherical_apply(int mode);
void __stdcall glBlendFunc_R_BlendLightmaps(unsigned int sfactor, unsigned int dfactor);
void __cdecl hkAdjustTexCoords(float* player_pos, float* vertex, float* in_normal, float* in_tangent, float* in_bitangent, float* out_new_s_t);

// Lighting engine state.
extern unsigned char lightmap_curve_lut[256];
extern unsigned char preserve_color_curve_lut[256];
extern unsigned char *lighting_gamma_table;
extern int *gl_state_lightmap_textures_ptr;
extern int *lightmap_chains_ptr;
extern void **r_worldmodel_ptr;
extern unsigned char *rdflags_ptr;
extern void **gl_mtex_coord_ptr;
extern int lightblend_preset;

void lighting_apply_effective_settings(bool user_change);
bool lighting_refresh_lut_from_gamma(void);
void lighting_request_lightmap_rebuild(void);
void lighting_rebuild_world_lightmaps(void);
void lighting_frame_tick(void);
void lighting_reset_runtime_state(void);
bool lighting_patch_lut_reads(void);
void lighting_register_commands(void);
void lighting_reset_curve_cvars(void);
void lighting_restore_multitexture_at_startup(void);
void lighting_sync_multitexture(bool allow_restart);
void lighting_apply_preserve_color_to_buffer(int surf, void* dest, int pitch);
void lighting_apply_entity_light_lut(float* color);
void lighting_install_ghoul_dlight_hook(void);
void lighting_set_lightmap_combine(bool enable);
void lighting_check_complex_states_for_lightmap(void);
void lighting_glend_for_lightmap(void);
void lighting_on_map_load_begin(void);
void lighting_cancel_pending_rebuild(void);
void lighting_on_map_loaded(void* model);

extern void (__cdecl *oCheckComplexStates)(void);
extern void (__stdcall **gl_select_texture_sgis)(unsigned int);
extern void (__cdecl *engine_active_texture)(unsigned int);

#endif
