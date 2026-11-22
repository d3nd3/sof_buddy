/*
	Lighting Blend Tweak

	Original idea: allow the OpenGL lightmap blend function to be swapped.
	The extended version keeps that for the legacy "Raven 2x" style and adds an
	11-point lightmap tone curve.  The curve is applied where ref_gl turns the
	built lightmap into 8-bit texels, so it works with gl_ext_multitexture 1
	and 0, and it implements real overbright (lightmap clips before the texture
	is multiplied, instead of clipping the texture * lightmap product).
*/

#include "feature_config.h"

#if FEATURE_LIGHTING_BLEND
#include "sof_compat.h"
#include "generated_detours.h"
#include "runtime_features.h"
#include "util.h"
#include "shared.h"
#include "DetourXS/detourxs.h"
#include <windows.h>
#include <string.h>
#include <cmath>
#include <stdlib.h>
#include <stdint.h>
#include <cstdio>

typedef enum {
	it_skin,
	it_sprite,
	it_wall,
	it_pic,
	it_sky,
	it_detail,
	it_temp,
	it_font,
	it_shiny
} imagetype_t;

static const char* imagetype_to_string(imagetype_t type) {
	switch (type) {
		case it_skin: return "it_skin";
		case it_sprite: return "it_sprite";
		case it_wall: return "it_wall";
		case it_pic: return "it_pic";
		case it_sky: return "it_sky";
		case it_detail: return "it_detail";
		case it_temp: return "it_temp";
		case it_font: return "it_font";
		case it_shiny: return "it_shiny";
		default: return "unknown";
	}
}

// OpenGL blend function constants
#define GL_ZERO                           0x0000
#define GL_ONE                            0x0001
#define GL_SRC_COLOR                      0x0300
#define GL_ONE_MINUS_SRC_COLOR            0x0301
#define GL_DST_COLOR                      0x0306
#define GL_ONE_MINUS_DST_COLOR            0x0307
#define GL_SRC_ALPHA                      0x0302
#define GL_ONE_MINUS_SRC_ALPHA            0x0303
#define GL_DST_ALPHA                      0x0304
#define GL_ONE_MINUS_DST_ALPHA            0x0305
#define GL_CONSTANT_COLOR                 0x8001
#define GL_ONE_MINUS_CONSTANT_COLOR       0x8002
#define GL_CONSTANT_ALPHA                 0x8003
#define GL_ONE_MINUS_CONSTANT_ALPHA       0x8004
#define GL_SRC_ALPHA_SATURATE             0x0308

#define GL_TEXTURE_2D                     0x0DE1
#define GL_TEXTURE_BINDING_2D             0x8069
#define GL_RGBA                           0x1908
#define GL_UNSIGNED_BYTE                  0x1401
#define GL_BLEND                          0x0BE2
#define GL_BLEND_SRC                      0x0BE1
#define GL_BLEND_DST                      0x0BE0
#define GL_DEPTH_WRITEMASK                0x0B72
#define GL_DEPTH_TEST                     0x0B71
#define GL_DEPTH_FUNC                     0x0B73
#define GL_LEQUAL                         0x0203
#define GL_CULL_FACE                      0x0B44
#define GL_TEXTURE_ENV                    0x2300
#define GL_TEXTURE_ENV_MODE               0x2200
#define GL_MODULATE                       0x2100
#define GL_COMBINE_ARB                    0x8570
#define GL_COMBINE_RGB_ARB                0x8571
#define GL_COMBINE_ALPHA_ARB              0x8572
#define GL_SOURCE0_RGB_ARB                0x8580
#define GL_SOURCE1_RGB_ARB                0x8581
#define GL_SOURCE0_ALPHA_ARB              0x8588
#define GL_SOURCE1_ALPHA_ARB              0x8589
#define GL_PREVIOUS_ARB                   0x8578
#define GL_TEXTURE                        0x1702
#define GL_RGB_SCALE_ARB                  0x8573
#define GL_ALPHA_SCALE                    0x0D1C
#define GL_TEXTURE0_ARB                   0x84C0
#define GL_TEXTURE1_ARB                   0x84C1
#define GL_TRIANGLE_FAN                   0x0006

// ref_gl.dll surface/model layout (verified against the retail 1.06a renderer).
static const unsigned int kSurfaceStride = 0x6C;
static const unsigned int kModelNumSurfacesOffset = 0xB4;
static const unsigned int kModelSurfacesOffset = 0xB8;
static const unsigned int kSurfTexinfoOffset = 0x34;
static const unsigned int kSurfDlightFrameOffset = 0x44;
static const unsigned int kSurfLightmapIndexOffset = 0x4C;
static const unsigned int kSurfSamplesOffset = 0x64;
static const unsigned int kSurfStylesOffset = 0x50;
static const unsigned int kSurfLightmapWidthOffset = 0x68;
static const unsigned int kSurfLightmapHeightOffset = 0x69;
static const unsigned int kSurfLightmapXOffset = 0x18;
static const unsigned int kSurfLightmapYOffset = 0x1C;
static const unsigned int kTexinfoFlagsOffset = 0x20;
static const unsigned int kNonLitSurfaceFlags = 0x80C;

typedef void (__stdcall *glBindTexture_t)(unsigned int, unsigned int);
typedef void (__stdcall *glTexSubImage2D_t)(unsigned int, int, int, int, int, int, unsigned int, unsigned int, const void*);
typedef void (__stdcall *glGetIntegerv_t)(unsigned int, int*);
typedef void (__stdcall *glTexEnvi_t)(unsigned int, unsigned int, int);

// SRC = Incoming Data(Light)
// DST = Existing Data(Texture)
// Defaults.
int lightblend_src = GL_ZERO;
int lightblend_dst = GL_SRC_COLOR;
int lightblend_preset = LIGHTING_PRESET_VANILLA;

unsigned char lightmap_curve_lut[256];
unsigned char preserve_color_curve_lut[256];
unsigned char *lighting_gamma_table = nullptr;
int *gl_state_lightmap_textures_ptr = nullptr;
int *lightmap_chains_ptr = nullptr;
void **r_worldmodel_ptr = nullptr;
unsigned char *rdflags_ptr = nullptr;
void **gl_mtex_coord_ptr = nullptr;
// ref_gl's active lightstyle array (float[4] per style: RGB + white).  Used
// read-only to show the current map's static style 0 value.
float** lighting_lightstyles_ptr = nullptr;
unsigned char (__cdecl *lighting_set_cache_state)(int surf) = nullptr;
void (__cdecl *oCheckComplexStates)(void) = nullptr;
void (__stdcall **gl_select_texture_sgis)(unsigned int) = nullptr;
void (__cdecl *engine_active_texture)(unsigned int) = nullptr;
static bool s_combine_active = false;
// Cached once per settings/renderer change so the per-polygon wrappers only do
// a boolean check while Overbright Lighting is off.
static bool s_overbright_combiner_enabled = false;
// True only while map normalization is actually changing the world LUT.  The
// R_LightPoint post-hook then applies the same LUT to entity/particle light;
// otherwise it returns after one boolean check.
static bool s_entity_light_lut_active = false;
static bool s_gl_modulate_sync = false;
static bool s_gl_modulate_warning_pending = false;
static bool s_gl_modulate_warning_open = false;
static float s_gl_modulate_requested = 1.0f;
static unsigned s_gl_modulate_pending_since = 0;

static int custom_lightblend_src = GL_ZERO;
static int custom_lightblend_dst = GL_SRC_COLOR;
static bool s_syncing_cvars = false;
static bool s_have_last_gamma = false;
static unsigned char s_last_gamma[256];
static int s_map_max_light = 0;
static int s_map_max_surfaces = 0;
static bool s_map_max_scanned = false;
static int s_map_min_light = 255;
static long long s_map_light_sum = 0;
static long long s_map_light_sq_sum = 0;
static long long s_map_light_count = 0;
static int s_map_light_median = 0;
static int s_map_light_p75 = 0;
static int s_map_light_p90 = 0;
static int s_map_light_mean = 0;
static int s_map_light_std = 0;
static int s_map_light_histogram[256] = {};
// Shared normalization LUT (indexed by a gamma-corrected byte).  It is built
// once per settings/map change and reused by the per-channel LUT, the preserve
// post-hook, and R_LightPoint so every light path uses the same curve.
static unsigned char s_normalization_lut[256];
static bool s_normalization_lut_active = false;
static bool s_rebuild_pending = false;
static int s_rebuild_delay = 0;

static glBindTexture_t s_glBindTexture = nullptr;
static glTexSubImage2D_t s_glTexSubImage2D = nullptr;
static glGetIntegerv_t s_glGetIntegerv = nullptr;
static glTexEnvi_t s_glTexEnvi = nullptr;

/*
	Built-in curve presets.  Every curve preset writes these values into the
	11 public curve cvars, so the sliders always show the active preset and
	Custom can start from any preset.  Vanilla, Raven 2x, and Custom are not
	in this table.
*/
struct LightingCurvePreset {
	int preset;
	const char* name;
	const char* alias;
	const char* summary;
	float values[lighting_curve_detail::kCurvePointCount];
};

static const LightingCurvePreset s_curve_presets[] = {
	{ LIGHTING_PRESET_OVERBRIGHT_2X, "2x Linear", "linear2x",
	  "Linear 2x overbright.",
	  { 0, 20, 40, 60, 80, 100, 100, 100, 100, 100, 100 } },
	{ LIGHTING_PRESET_OVERBRIGHT_4X, "4x Linear", "linear4x",
	  "Linear 4x overbright.",
	  { 0, 40, 80, 100, 100, 100, 100, 100, 100, 100, 100 } },
	{ LIGHTING_PRESET_SOFT_GLOW, "Soft Glow", "softglow",
	  "Lifted shadows with a gentle shoulder.",
	  { 5, 17, 29, 42, 56, 70, 82, 94, 100, 100, 100 } },
	{ LIGHTING_PRESET_HIGH_CONTRAST, "High Contrast", "contrast",
	  "Steep midtones, dark shadows, flat highlights.",
	  { 0, 1, 4, 10, 25, 50, 74, 89, 96, 99, 100 } },
	{ LIGHTING_PRESET_DEEP_SHADOWS, "Deep Shadows", "deep",
	  "Crushed low end with a bright upper slope.",
	  { 0, 0, 2, 6, 18, 44, 70, 86, 94, 98, 100 } },
	{ LIGHTING_PRESET_FILM_SLOPE, "Film Slope", "film",
	  "Gentle toe, long shoulder, film-like midtones.",
	  { 3, 8, 16, 28, 44, 60, 74, 85, 92, 97, 100 } },
	{ LIGHTING_PRESET_CLIFF_EDGE, "Cliff Edge", "cliff",
	  "Near-black shadows, sudden bright midtones.",
	  { 0, 2, 5, 8, 72, 82, 89, 94, 97, 99, 100 } },
	{ LIGHTING_PRESET_BRIGHT_PUNCH, "Bright Punch", "punch",
	  "Brighter midtones with a soft highlight roll-off.",
	  { 0, 10, 22, 38, 58, 75, 87, 94, 98, 99, 100 } },
	{ LIGHTING_PRESET_MIDNIGHT, "Midnight", "midnight",
	  "Very dark shadows with a late, steep climb.",
	  { 0, 0, 1, 3, 8, 22, 50, 78, 92, 98, 100 } },
};

static const LightingCurvePreset* find_curve_preset(int preset) {
	for (const LightingCurvePreset& candidate : s_curve_presets) {
		if (candidate.preset == preset) {
			return &candidate;
		}
	}
	return nullptr;
}

static bool is_curve_preset(int preset) {
	return find_curve_preset(preset) != nullptr;
}

static bool equals_ignore_case(const char* a, const char* b) {
	if (!a || !b) {
		return false;
	}
	while (*a && *b) {
		char ca = *a++;
		char cb = *b++;
		if (ca >= 'A' && ca <= 'Z') ca = static_cast<char>(ca - 'A' + 'a');
		if (cb >= 'A' && cb <= 'Z') cb = static_cast<char>(cb - 'A' + 'a');
		if (ca != cb) {
			return false;
		}
	}
	return *a == '\0' && *b == '\0';
}

static const LightingCurvePreset* find_curve_preset_by_token(const char* token) {
	for (const LightingCurvePreset& candidate : s_curve_presets) {
		if (equals_ignore_case(candidate.alias, token) ||
		    equals_ignore_case(candidate.name, token)) {
			return &candidate;
		}
	}
	return nullptr;
}

static void *shiny_nop_site = nullptr;
static unsigned char shiny_nop_saved[10];
static bool shiny_nop_saved_init = false;
static void *shiny_nop_site3 = nullptr;
static unsigned char shiny_nop3_saved[21];
static bool shiny_nop3_saved_init = false;

// Function pointer declarations
void (__stdcall *real_glBlendFunc)(unsigned int, unsigned int) = nullptr;
void (__cdecl *oAdjustTexCoords)(float* player_pos, float* vertex, float* in_normal, float* in_tangent, float* in_bitangent, float* out_new_s_t) = nullptr;
void (__stdcall *glTexCoord2f)(float s, float t) = nullptr;
void (__stdcall *glVertex3f)(float x, float y, float z) = nullptr;
void (__stdcall *glBegin)(unsigned int mode) = nullptr;
void (__stdcall *glEnd)(void) = nullptr;
void (__stdcall *glColor3f)(float r, float g, float b) = nullptr;


// Function declarations
void __stdcall glBlendFunc_R_BlendLightmaps(unsigned int sfactor, unsigned int dfactor);

void __cdecl hkAdjustTexCoords(float* player_pos, float* vertex, float* in_normal, float* in_tangent, float* in_bitangent, float* out_new_s_t);
void lightblend_change(cvar_t * cvar);
void lighting_overbright_change(cvar_t * cvar);
void lighting_preset_change(cvar_t * cvar);
void lighting_curve_change(cvar_t * cvar);
void lighting_preserve_color_change(cvar_t * cvar);
void lighting_normalize_change(cvar_t * cvar);
void lighting_cutoff_change(cvar_t * cvar);
void water_size_change(cvar_t * cvar);
void shiny_spherical_change(cvar_t * cvar);

static void set_cvar_string(cvar_t* cvar, const char* value);

// Callbacks for R_BlendLightmaps
void r_blendlightmaps_pre_callback(void);
void r_blendlightmaps_post_callback(void);


/*
	CVar change callback for _sofbuddy_lighting_cutoff

	Called when _sofbuddy_lighting_cutoff is changed.
	Writes to ref_gl address 0x2C368 as float*.
	This function is externally referenced in cvars.cpp.
*/
void lighting_cutoff_change(cvar_t * cvar) {
	if (lighting_cutoff_target) {
		writeFloatAt(lighting_cutoff_target, cvar->value);
		PrintOut(PRINT_LOG, "lighting_blend: Set lighting cutoff to %f\n", cvar->value);
	} else {
		PrintOut(PRINT_BAD, "lighting_blend: lighting_cutoff_target not initialized (ref_gl not loaded)\n");
	}
}

/*
	CVar change callback for _sofbuddy_water_size

	Called when _sofbuddy_water_size is changed.
	Writes double to ref_gl address 0x2C390 and float to 0x2C398.
	This function is externally referenced in cvars.cpp.
*/
void water_size_change(cvar_t * cvar) {
	if (water_size_double_target && water_size_float_target) {
		if (cvar->value == 0.0f) {
			PrintOut(PRINT_BAD, "lighting_blend: water_size cannot be 0 (division by zero)\n");
			detour_Cvar_Set2::oCvar_Set2(const_cast<char*>("_sofbuddy_water_size"), const_cast<char*>("16"), true);
			return;
		}
		if (cvar->value < 16.0f) {
			PrintOut(PRINT_BAD, "lighting_blend: water_size must be at least 16 (got %f)\n", cvar->value);
			detour_Cvar_Set2::oCvar_Set2(const_cast<char*>("_sofbuddy_water_size"), const_cast<char*>("16"), true);
			return;
		}
		writeDoubleAt(water_size_double_target, (double)cvar->value);
		writeFloatAt(water_size_float_target, 1.0f / cvar->value);
		PrintOut(PRINT_LOG, "lighting_blend: Set water_size to %f (double=0x2C390, float=0x2C398)\n", cvar->value);
	} else {
		PrintOut(PRINT_BAD, "lighting_blend: water_size targets not initialized (ref_gl not loaded)\n");
	}
}

/*
	CVar change callback for _sofbuddy_shiny_spherical

	Called when _sofbuddy_shiny_spherical is changed.
	Writes byte to ref_gl address 0x14814.
	This function is externally referenced in cvars.cpp.
*/
void shiny_spherical_change(cvar_t * cvar) {
	if (shiny_spherical_target1) {
		unsigned char value = cvar->value == 0.0f ? 0xEB : 0x74;
		WriteByte(shiny_spherical_target1, value);
		PrintOut(PRINT_LOG, "lighting_blend: Set shiny_spherical to %d (wrote 0x%02X to 0x14814)\n", (int)cvar->value, value);
	}

	if (shiny_spherical_target3) {
		unsigned char value = cvar->value == 0.0f ? 0xEB : 0x74;
		WriteByte(shiny_spherical_target3, value);
		PrintOut(PRINT_LOG, "lighting_blend: Set shiny_spherical to %d (wrote 0x%02X to 0x14A66)\n", (int)cvar->value, value);
	}
}

static int parse_blend_value(const char* string, bool is_src) {
	int value = 0;

	if (!string) {
		return is_src ? GL_ZERO : GL_SRC_COLOR;
	}

	if (!strcmp(string, "GL_ZERO")) {
		value = GL_ZERO;
	} else if (!strcmp(string, "GL_ONE")) {
		value = GL_ONE;
	} else if (!strcmp(string, "GL_SRC_COLOR")) {
		value = GL_SRC_COLOR;
	} else if (!strcmp(string, "GL_ONE_MINUS_SRC_COLOR")) {
		value = GL_ONE_MINUS_SRC_COLOR;
	} else if (!strcmp(string, "GL_DST_COLOR")) {
		value = GL_DST_COLOR;
	} else if (!strcmp(string, "GL_ONE_MINUS_DST_COLOR")) {
		value = GL_ONE_MINUS_DST_COLOR;
	} else if (!strcmp(string, "GL_SRC_ALPHA")) {
		value = GL_SRC_ALPHA;
	} else if (!strcmp(string, "GL_ONE_MINUS_SRC_ALPHA")) {
		value = GL_ONE_MINUS_SRC_ALPHA;
	} else if (!strcmp(string, "GL_DST_ALPHA")) {
		value = GL_DST_ALPHA;
	} else if (!strcmp(string, "GL_ONE_MINUS_DST_ALPHA")) {
		value = GL_ONE_MINUS_DST_ALPHA;
	} else if (!strcmp(string, "GL_CONSTANT_COLOR")) {
		value = GL_CONSTANT_COLOR;
	} else if (!strcmp(string, "GL_ONE_MINUS_CONSTANT_COLOR")) {
		value = GL_ONE_MINUS_CONSTANT_COLOR;
	} else if (!strcmp(string, "GL_CONSTANT_ALPHA")) {
		value = GL_CONSTANT_ALPHA;
	} else if (!strcmp(string, "GL_ONE_MINUS_CONSTANT_ALPHA")) {
		value = GL_ONE_MINUS_CONSTANT_ALPHA;
	} else if (!strcmp(string, "GL_SRC_ALPHA_SATURATE")) {
		value = GL_SRC_ALPHA_SATURATE;
	} else {
		PrintOut(PRINT_BAD, "Bad lightblend value: %s. Valid values:\n"
		                     "GL_ZERO, GL_ONE, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR, GL_DST_COLOR, "
		                     "GL_ONE_MINUS_DST_COLOR, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA, "
		                     "GL_ONE_MINUS_DST_ALPHA, GL_CONSTANT_COLOR, GL_ONE_MINUS_CONSTANT_COLOR, "
		                     "GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA, GL_SRC_ALPHA_SATURATE\n",
		                     string);

		if (is_src) {
			PrintOut(PRINT_DEV, "Defaulting to src:GL_ZERO\n");
			value = GL_ZERO;
		} else {
			PrintOut(PRINT_DEV, "Defaulting to dst:GL_SRC_COLOR\n");
			value = GL_SRC_COLOR;
		}
	}

	return value;
}

static float cvar_float_or(cvar_t* cvar, float fallback) {
	return cvar ? cvar->value : fallback;
}

/*
	0 = Off, 1 = Up (max -> 255), 2 = Down (min -> 0, max kept),
	3 = Both (min -> 0 and max -> 255).
*/
static int lighting_normalize_mode(void) {
	if (!RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_LIGHTING_BLEND) ||
	    !_sofbuddy_lighting_normalize) {
		return lighting_curve_detail::kNormalizationModeOff;
	}
	return lighting_curve_detail::sanitize_normalization_mode(
		static_cast<int>(_sofbuddy_lighting_normalize->value + 0.5f));
}

static bool lighting_normalization_enabled(void) {
	return lighting_normalize_mode() != lighting_curve_detail::kNormalizationModeOff;
}

static int lighting_normalize_up_percent(void) {
	if (!_sofbuddy_lighting_normalize_up) {
		return 100;
	}
	return lighting_curve_detail::sanitize_normalize_percent(
		static_cast<int>(_sofbuddy_lighting_normalize_up->value + 0.5f));
}

static int lighting_normalize_down_percent(void) {
	if (!_sofbuddy_lighting_normalize_down) {
		return 100;
	}
	return lighting_curve_detail::sanitize_normalize_percent(
		static_cast<int>(_sofbuddy_lighting_normalize_down->value + 0.5f));
}

/*
	The curve operates on gamma-corrected lightmap values, so the normalization
	references must be gamma-table values of the map's raw samples.  The live
	percentage settings choose how far the black and white points move from the
	observed min/max; there is no fixed 4x cap in this path.
*/
static int lighting_normalize_gamma_value(int raw);

static int lighting_normalize_max_reference(void) {
	if (!lighting_normalization_enabled() || s_map_max_light <= 0) {
		return 0;
	}
	return lighting_normalize_gamma_value(s_map_max_light);
}

static int lighting_normalize_min_reference(void) {
	if (!lighting_normalization_enabled() || s_map_max_light <= 0 ||
	    !lighting_curve_detail::normalization_mode_uses_min(lighting_normalize_mode())) {
		return 0;
	}
	return lighting_normalize_gamma_value(s_map_min_light);
}

static float lighting_normalize_gain(void) {
	const int mode = lighting_normalize_mode();
	const int max_reference = lighting_normalize_max_reference();
	if (mode == lighting_curve_detail::kNormalizationModeOff || max_reference <= 0) {
		return 1.0f;
	}
	const int min_reference = lighting_normalize_min_reference();
	return lighting_curve_detail::normalize_gain_amount(
		mode, min_reference, max_reference,
		lighting_normalize_up_percent(), lighting_normalize_down_percent());
}

/*
	R_LightPoint supplies entity/particle/Ghoul light from raw lightmap data.
	Only transform it when the world LUT differs from the stock gamma table or
	the preserve LUT differs from identity, so Vanilla stays byte-for-byte stock.
*/
static bool lighting_entity_light_lut_needed(void) {
	if (!RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_LIGHTING_BLEND)) {
		return false;
	}
	for (int i = 0; i < 256; ++i) {
		const unsigned char stock = lighting_gamma_table
			? lighting_gamma_table[i] : static_cast<unsigned char>(i);
		if (lightmap_curve_lut[i] != stock) {
			return true;
		}
	}
	for (int i = 0; i < 256; ++i) {
		if (preserve_color_curve_lut[i] != static_cast<unsigned char>(i)) {
			return true;
		}
	}
	return false;
}

static int lighting_normalize_gamma_value(int raw) {
	if (raw < 0) {
		raw = 0;
	} else if (raw > 255) {
		raw = 255;
	}
	return lighting_gamma_table ? lighting_gamma_table[raw] : raw;
}

static const char* lighting_normalize_mode_label(int mode) {
	switch (lighting_curve_detail::sanitize_normalization_mode(mode)) {
		case lighting_curve_detail::kNormalizationModeUp: return "Up";
		case lighting_curve_detail::kNormalizationModeDown: return "Down";
		case lighting_curve_detail::kNormalizationModeBoth: return "Both";
		default: return "Off";
	}
}

static int lighting_color_mode(void) {
	if (!_sofbuddy_lighting_preserve_color) {
		return LIGHTING_COLOR_PER_CHANNEL;
	}
	int mode = static_cast<int>(_sofbuddy_lighting_preserve_color->value + 0.5f);
	if (mode < 0) {
		mode = LIGHTING_COLOR_PER_CHANNEL;
	}
	if (mode >= LIGHTING_COLOR_COUNT) {
		mode = LIGHTING_COLOR_COUNT - 1;
	}
	return mode;
}

static const char* lighting_color_mode_label(int mode) {
	switch (mode) {
		case LIGHTING_COLOR_PRESERVE_BRIGHTNESS: return "Preserve brightness";
		case LIGHTING_COLOR_PRESERVE_COLOUR: return "Preserve colour";
		default: return "Per-channel";
	}
}

static bool lighting_raven_product_requested(void) {
	return RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_LIGHTING_BLEND) &&
	       lightblend_preset == LIGHTING_PRESET_RAVEN_2X;
}

/*
	Raven 2x works with either renderer path:
	- multitexture on: unit 1 modulates TEXTURE by PREVIOUS and scales by 2,
	  so the fragment is 2*T*L without touching the frame blend state.
	- multitexture off: the original DST_C / SRC_C framebuffer blend is used.

	ref_gl nulls glSelectTextureSGIS/glMTexCoord2fSGIS in R_Init when
	gl_ext_multitexture is 0, so the function pointer is the actual active
	renderer path. Using it instead of the cvar also keeps a console cvar
	change without vid_restart consistent with the renderer that is still
	running; the next vid_restart switches both together.
*/
static bool lighting_raven_combiner_active(void) {
	return lighting_raven_product_requested() &&
	       gl_select_texture_sgis && *gl_select_texture_sgis;
}

static lighting_curve_detail::CurvePoints effective_curve_points(void) {
	lighting_curve_detail::CurvePoints points = lighting_curve_detail::identity();

	if (lighting_raven_product_requested()) {
		// Overbright has its own pre-curve and separate profiles.  The default
		// is Late highlights; 2x everything is the old linear identity, so the
		// stock Overbright product remains 2*T*L.  A darker curve gives
		// 2*T*C(L) and leaves the stored Lighting Style curve untouched.
		const lighting_curve_detail::CurvePoints defaults =
			lighting_curve_detail::overbright_profile_points(
				lighting_curve_detail::kOverbrightProfileLateHighlights);
		for (int i = 0; i < lighting_curve_detail::kCurvePointCount; ++i) {
			points.p[i] = cvar_float_or(_sofbuddy_lighting_overbright_curve[i],
			                            defaults.p[i]);
		}
		return lighting_curve_detail::sanitize(points);
	}

	if (lightblend_preset == LIGHTING_PRESET_VANILLA) {
		return points;
	}

	// Built-in presets and Custom are all driven by the 11 public curve cvars.
	for (int i = 0; i < lighting_curve_detail::kCurvePointCount; ++i) {
		points.p[i] = cvar_float_or(_sofbuddy_lighting_curve[i],
		                            static_cast<float>(i * 10));
	}

	return lighting_curve_detail::sanitize(points);
}

static bool s_ghoul_min_light_warned = false;

/*
	Map one gamma-corrected light value through the same LUT used by
	R_LightPoint for entities, particles, and Ghoul models.
*/
static unsigned char lighting_effective_light_byte(int raw) {
	if (raw < 0) raw = 0;
	if (raw > 255) raw = 255;
	if (lighting_color_mode() == LIGHTING_COLOR_PER_CHANNEL) {
		return lightmap_curve_lut[raw];
	}
	const unsigned char gamma = lighting_gamma_table
		? lighting_gamma_table[raw] : static_cast<unsigned char>(raw);
	const unsigned char* normalize =
		s_normalization_lut_active ? s_normalization_lut : nullptr;
	const int curve_input = normalize ? normalize[gamma] : gamma;
	return preserve_color_curve_lut[curve_input];
}

/*
	DrawPlayer clamps Ghoul model lighting to a hard 0.1 floor for entities
	with RF_MINLIGHT, applied after R_LightPoint and ghl_light_multiply.
	Vanilla keeps that stock floor.  The F12 `Ghoul Minimum Light` bypass
	setting removes the floor so the model follows the actual light and can go
	fully dark.  The instruction shape is checked first so a different ref_gl
	build is left untouched.
*/
static void lighting_update_ghoul_min_light(void) {
	unsigned char* instruction = reinterpret_cast<unsigned char*>(
		rvaToAbsRef(reinterpret_cast<void*>(0x0001F456)));
	if (!instruction) {
		return;
	}
	const bool patchable =
		instruction[0] == 0xB8 &&
		instruction[5] == 0x89 && instruction[6] == 0x07 &&
		instruction[7] == 0x89 && instruction[8] == 0x46 && instruction[9] == 0x0C &&
		instruction[10] == 0x89 && instruction[11] == 0x46 && instruction[12] == 0x10;
	if (!patchable) {
		if (!s_ghoul_min_light_warned) {
			s_ghoul_min_light_warned = true;
			PrintOut(PRINT_BAD, "lighting_blend: Ghoul minimum-light patch skipped (unexpected opcodes)\n");
		}
		return;
	}

	const bool bypass =
		_sofbuddy_lighting_ghoul_min_light &&
		_sofbuddy_lighting_ghoul_min_light->value >= 0.5f;
	const float minimum = bypass ? 0.0f : 0.1f;

	unsigned int bits;
	memcpy(&bits, &minimum, sizeof(bits));
	writeUnsignedIntegerAt(instruction + 1, bits);
	FlushInstructionCache(GetCurrentProcess(), instruction + 1, sizeof(bits));
}

/*
	Ghoul receives engine dlights through Ghoul::AddImportLight(const
	QuakeLight&).  Its model lighting uses that copy directly, so the dlight
	contribution bypasses the R_LightPoint LUT.  Scale the copy's colour by the
	effective curve; the world dlight path keeps the original r_dlights and is
	still curred by R_BuildLightMap.  The vtable is the Ghoul vtable in SoF.exe
	(confirmed by intro_scaler's kRvaGhoulVtbl).
*/
typedef void (__thiscall *GhoulAddImportLight_t)(void* ghoul, const float* light);
static GhoulAddImportLight_t s_original_add_import_light = nullptr;

#if defined(__GNUC__) && defined(__i386__)
static void thiscall_add_import_light(void* ghoul, const float* light, void* fn) {
	if (!fn) return;
	__asm__ __volatile__(
		"pushl %2\n\t"
		"movl %0, %%ecx\n\t"
		"call *%1\n\t"
		: : "r"(ghoul), "r"(fn), "r"(light)
		: "eax", "ecx", "edx", "memory");
}
#endif

#if defined(__GNUC__)
__attribute__((thiscall))
#endif
static void hk_ghoul_add_import_light(void* ghoul, const float* light) {
	if (!s_original_add_import_light) {
		return;
	}
	if (!light || !s_entity_light_lut_active) {
#if defined(__GNUC__) && defined(__i386__)
		thiscall_add_import_light(ghoul, light, reinterpret_cast<void*>(s_original_add_import_light));
#else
		s_original_add_import_light(ghoul, light);
#endif
		return;
	}

	// QuakeLight: origin[3], colour[3], intensity.
	float scaled[7];
	memcpy(scaled, light, sizeof(scaled));
	float peak = scaled[3];
	if (scaled[4] > peak) peak = scaled[4];
	if (scaled[5] > peak) peak = scaled[5];
	if (peak > 0.0f) {
		if (peak > 1.0f) peak = 1.0f;
		const int raw = static_cast<int>(peak * 192.0f + 0.5f);
		const float mapped = static_cast<float>(lighting_effective_light_byte(raw)) * (1.0f / 255.0f);
		const float factor = mapped / peak;
		scaled[3] *= factor;
		scaled[4] *= factor;
		scaled[5] *= factor;
	}

#if defined(__GNUC__) && defined(__i386__)
	thiscall_add_import_light(ghoul, scaled, reinterpret_cast<void*>(s_original_add_import_light));
#else
	s_original_add_import_light(ghoul, scaled);
#endif
}

void lighting_install_ghoul_dlight_hook(void) {
	void** ghoul_slot = reinterpret_cast<void**>(
		rvaToAbsRef(reinterpret_cast<void*>(0x000AA4A0)));
	if (!ghoul_slot || !*ghoul_slot) {
		return;
	}
	void* ghoul = *ghoul_slot;
	void** vtable = *reinterpret_cast<void***>(ghoul);
	if (!vtable) {
		return;
	}
	void** entry = &vtable[31]; // AddImportLight at vtable+0x7C
	if (!*entry || *entry == reinterpret_cast<void*>(&hk_ghoul_add_import_light)) {
		return;
	}
	s_original_add_import_light = reinterpret_cast<GhoulAddImportLight_t>(*entry);
	PrintOut(PRINT_LOG, "lighting_blend: Ghoul import-light original %p\n",
	         reinterpret_cast<void*>(s_original_add_import_light));

	DWORD old = 0;
	if (!VirtualProtect(entry, sizeof(void*), PAGE_EXECUTE_READWRITE, &old)) {
		PrintOut(PRINT_BAD, "lighting_blend: Ghoul import-light hook skipped (VirtualProtect failed)\n");
		s_original_add_import_light = nullptr;
		return;
	}
	*entry = reinterpret_cast<void*>(&hk_ghoul_add_import_light);
	VirtualProtect(entry, sizeof(void*), old, &old);
	PrintOut(PRINT_LOG, "lighting_blend: Ghoul import-light curve hook installed\n");
}

/*
	Rebuild the lightmap-only LUT by composing the current game gamma table
	with the 11-point tone curve, including map lighting normalization when
	enabled.  Returns true when the LUT changed.
*/
bool lighting_refresh_lut_from_gamma(void) {
	unsigned char next_lightmap[256];
	unsigned char next_preserve[256];
	unsigned char next_normalization[256];
	const lighting_curve_detail::CurvePoints points = effective_curve_points();
	const bool preserve =
		RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_LIGHTING_BLEND) &&
		lighting_color_mode() != LIGHTING_COLOR_PER_CHANNEL;
	// Overbright is 2 * C(L); use unbiased rounding so a half-level curve does
	// not systematically lift the dark end by one 8-bit step.
	const bool overbright = lighting_raven_product_requested();

	lighting_curve_detail::build_normalization_lut_amount(
		next_normalization, lighting_normalize_mode(),
		lighting_normalize_min_reference(), lighting_normalize_max_reference(),
		lighting_normalize_up_percent(), lighting_normalize_down_percent());

	bool changed = false;
	if (memcmp(next_normalization, s_normalization_lut, sizeof(next_normalization))) {
		memcpy(s_normalization_lut, next_normalization, sizeof(next_normalization));
		changed = true;
	}
	s_normalization_lut_active = false;
	for (int i = 0; i < 256; ++i) {
		if (s_normalization_lut[i] != static_cast<unsigned char>(i)) {
			s_normalization_lut_active = true;
			break;
		}
	}

	for (int i = 0; i < 256; ++i) {
		const unsigned char base = lighting_gamma_table
			? lighting_gamma_table[i] : static_cast<unsigned char>(i);
		// Per-channel mode composes gamma, normalization and the curve in the
		// inline LUT. The preserve modes leave the inline LUT at the original
		// gamma table; they apply both normalization and the curve uniformly in
		// the R_BuildLightMap post-hook so channel ratios stay intact.
		const unsigned char curved =
			overbright
				? lighting_curve_detail::apply_byte_nearest_even(
					  s_normalization_lut[base], points)
				: lighting_curve_detail::apply_byte(s_normalization_lut[base], points);
		next_lightmap[i] = preserve ? base : curved;
		next_preserve[i] =
			overbright
				? lighting_curve_detail::apply_byte_nearest_even(
					  static_cast<unsigned char>(i), points)
				: lighting_curve_detail::apply_byte(static_cast<unsigned char>(i), points);
	}

	if (memcmp(next_lightmap, lightmap_curve_lut, sizeof(next_lightmap))) {
		memcpy(lightmap_curve_lut, next_lightmap, sizeof(next_lightmap));
		changed = true;
	}
	if (memcmp(next_preserve, preserve_color_curve_lut, sizeof(next_preserve))) {
		memcpy(preserve_color_curve_lut, next_preserve, sizeof(next_preserve));
		changed = true;
	}
	return changed;
}

static const char* preset_name(int preset) {
	if (const LightingCurvePreset* curve = find_curve_preset(preset)) {
		return curve->name;
	}
	switch (preset) {
		case LIGHTING_PRESET_VANILLA: return "Vanilla";
		case LIGHTING_PRESET_RAVEN_2X: return "Raven 2x";
		case LIGHTING_PRESET_CUSTOM: return "Custom";
		default: return "Unknown";
	}
}

static const char* preset_summary(int preset) {
	if (const LightingCurvePreset* curve = find_curve_preset(preset)) {
		return curve->summary;
	}
	switch (preset) {
		case LIGHTING_PRESET_VANILLA: return "Original SoF lighting.";
		case LIGHTING_PRESET_RAVEN_2X: return "Raven 2x product. Works with multitexture on or off.";
		case LIGHTING_PRESET_CUSTOM:
			if (custom_lightblend_src != GL_ZERO || custom_lightblend_dst != GL_SRC_COLOR)
				return "Custom curve + extra blend (stacks; can look neon).";
			return "Custom 11-point curve (neutral blend).";
		default: return "";
	}
}

static void set_cvar_string(cvar_t* cvar, const char* value) {
	if (!cvar || !cvar->string || !value || !detour_Cvar_Set2::oCvar_Set2) {
		return;
	}
	if (!strcmp(cvar->string, value)) {
		return;
	}
	const bool old_sync = s_syncing_cvars;
	s_syncing_cvars = true;
	detour_Cvar_Set2::oCvar_Set2(cvar->name, const_cast<char*>(value), true);
	s_syncing_cvars = old_sync;
}

/*
	Quake lightstyle letters map to (letter - 'a') / ('m' - 'a'): 'm' is 1.0,
	'l' is 0.92, and 'n'..'z' go above 1.0 for extra punch.  Some maps set
	CS_LIGHTS+0 to a letter below 'm' to leave more headroom for other styles;
	read the renderer's active style 0 so the F12 page can show what the
	current map actually uses.
*/
static bool lighting_read_style0(float& r, float& g, float& b) {
	if (!r_worldmodel_ptr || !*r_worldmodel_ptr ||
	    !lighting_lightstyles_ptr || !*lighting_lightstyles_ptr) {
		return false;
	}
	float* styles = *lighting_lightstyles_ptr;
	if (!styles || IsBadReadPtr(styles, 4 * sizeof(float))) {
		return false;
	}
	r = styles[0];
	g = styles[1];
	b = styles[2];
	return true;
}

static char lighting_style0_letter(float value) {
	const int index = static_cast<int>(value * 12.0f + 0.5f);
	if (index < 0 || index > 25) {
		return '\0';
	}
	return static_cast<char>('a' + index);
}

static void lighting_update_style0_status(void) {
	if (!_sb_internal_lighting_style0_status) {
		return;
	}

	char buffer[96];
	float r;
	float g;
	float b;
	if (!lighting_read_style0(r, g, b)) {
		buffer[0] = '\0';
	} else {
		const float value = (r + g + b) / 3.0f;
		const char letter = lighting_style0_letter(value);
		if (letter && std::fabs(r - g) < 0.01f && std::fabs(g - b) < 0.01f) {
			std::snprintf(buffer, sizeof(buffer), "Static style 0 : %.2f (%c)",
			              value, letter);
		} else {
			std::snprintf(buffer, sizeof(buffer),
			              "Static style 0 : RGB %.2f / %.2f / %.2f", r, g, b);
		}
	}
	set_cvar_string(_sb_internal_lighting_style0_status, buffer);
}

/*
	RMF snippets injected with <includecvar>.  Only the curve-specific rows are
	swapped out when Overbright Lighting is on; normalization is
	independent of the curve and stays visible.

	Keep theme tokens ({font_title}, ...) out of these strings: the include is
	expanded by the menu parser, after the page's theme tokens have already been
	resolved.  The surrounding page sets the font type before each include.
*/
static const char kLightingCustomSectionRmf[] = R"RMF(<cinclude _sb_internal_lighting_custom sof_buddy/lighting_custom>
)RMF";

/*
	The F12 Lighting tab only includes the manual blend section.  It is a
	Custom-only, Overbright-off control; the live hub owns the preview rows.
*/
static void lighting_refresh_ui_rmf(void) {
	if (!detour_Cvar_Get::oCvar_Get) {
		return;
	}

	const bool overbright_active =
		RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_LIGHTING_BLEND) &&
		_sofbuddy_lighting_overbright &&
		_sofbuddy_lighting_overbright->value != 0.0f;
	cvar_t* custom = detour_Cvar_Get::oCvar_Get(
		"_sb_internal_lighting_custom_section_rmf", "", 0, nullptr);
	set_cvar_string(custom, overbright_active ? "" : kLightingCustomSectionRmf);

	const int normalize_mode = lighting_normalize_mode();
	cvar_t* show_up = detour_Cvar_Get::oCvar_Get(
		"_sb_internal_lighting_normalize_show_up", "0", 0, nullptr);
	cvar_t* show_down = detour_Cvar_Get::oCvar_Get(
		"_sb_internal_lighting_normalize_show_down", "0", 0, nullptr);
	set_cvar_string(show_up,
	                (normalize_mode == lighting_curve_detail::kNormalizationModeUp ||
	                 normalize_mode == lighting_curve_detail::kNormalizationModeBoth)
	                    ? "1" : "0");
	set_cvar_string(show_down,
	                (normalize_mode == lighting_curve_detail::kNormalizationModeDown ||
	                 normalize_mode == lighting_curve_detail::kNormalizationModeBoth)
	                    ? "1" : "0");
}

/*
	F12 Lightmap Multiplier slider guard.

	The slider is bound to an internal mirror cvar, not to gl_modulate
	directly. Moving it away from 1 queues a warning page; the real gl_modulate
	only changes when the player confirms. Cancel (or closing the menu) resets
	the mirror to the real value.
*/
static cvar_t* lighting_gl_modulate_cvar(void) {
	return findCvar(const_cast<char*>("gl_modulate"));
}

static void lighting_set_gl_modulate_value(float value) {
	cvar_t* real = lighting_gl_modulate_cvar();
	if (!real || !detour_Cvar_Set2::oCvar_Set2) {
		return;
	}
	char buffer[32];
	std::snprintf(buffer, sizeof(buffer), "%g", value);
	const bool old_sync = s_gl_modulate_sync;
	s_gl_modulate_sync = true;
	detour_Cvar_Set2::oCvar_Set2(real->name, buffer, true);
	if (_sb_internal_lighting_gl_modulate) {
		detour_Cvar_Set2::oCvar_Set2(
			_sb_internal_lighting_gl_modulate->name, buffer, true);
	}
	s_gl_modulate_sync = old_sync;
}

static void lighting_sync_gl_modulate_mirror(void) {
	if (!_sb_internal_lighting_gl_modulate || !detour_Cvar_Set2::oCvar_Set2) {
		return;
	}
	// Never clobber the value the player is about to confirm or cancel.
	if (s_gl_modulate_warning_pending || s_gl_modulate_warning_open) {
		return;
	}
	cvar_t* real = lighting_gl_modulate_cvar();
	if (!real) {
		return;
	}
	if (std::fabs(_sb_internal_lighting_gl_modulate->value - real->value) <= 0.0001f) {
		return;
	}
	char buffer[32];
	std::snprintf(buffer, sizeof(buffer), "%g", real->value);
	const bool old_sync = s_gl_modulate_sync;
	s_gl_modulate_sync = true;
	detour_Cvar_Set2::oCvar_Set2(
		_sb_internal_lighting_gl_modulate->name, buffer, true);
	s_gl_modulate_sync = old_sync;
}

static void lighting_set_gl_modulate_pending(float value) {
	s_gl_modulate_requested = value;
	if (!detour_Cvar_Set2::oCvar_Set2) {
		return;
	}
	cvar_t* real = lighting_gl_modulate_cvar();
	if (_sb_internal_lighting_gl_modulate_current) {
		char current[32];
		std::snprintf(current, sizeof(current), "%g", real ? real->value : 1.0f);
		detour_Cvar_Set2::oCvar_Set2(
			_sb_internal_lighting_gl_modulate_current->name, current, true);
	}
	if (!_sb_internal_lighting_gl_modulate_pending) {
		return;
	}
	char buffer[32];
	std::snprintf(buffer, sizeof(buffer), "%g", value);
	detour_Cvar_Set2::oCvar_Set2(
		_sb_internal_lighting_gl_modulate_pending->name, buffer, true);
}

void lighting_gl_modulate_change(cvar_t* cvar) {
	if (s_gl_modulate_sync || !cvar) {
		return;
	}
	cvar_t* real = lighting_gl_modulate_cvar();
	if (!real) {
		return;
	}

	const float requested = cvar->value;
	if (std::fabs(requested - real->value) <= 0.0001f) {
		return;
	}

	// Returning to the safe value never needs the warning.
	if (std::fabs(requested - 1.0f) <= 0.0001f) {
		lighting_set_gl_modulate_value(1.0f);
		return;
	}

	lighting_set_gl_modulate_pending(requested);
	// While the popup is already up, only update the requested value.  The
	// slider is not visible underneath, but a console write should not stack a
	// second warning page.
	if (!s_gl_modulate_warning_open) {
		s_gl_modulate_warning_pending = true;
		s_gl_modulate_pending_since = 0;
		if (detour_Sys_Milliseconds::oSys_Milliseconds) {
			s_gl_modulate_pending_since = static_cast<unsigned>(
				detour_Sys_Milliseconds::oSys_Milliseconds());
		}
	}
}

static void lighting_gl_modulate_tick(void) {
	if (s_gl_modulate_warning_pending) {
		// The player can drag through a value and back to 1 before this tick
		// runs.  In that case apply the safe value and never show the page.
		float requested = _sb_internal_lighting_gl_modulate
		                      ? _sb_internal_lighting_gl_modulate->value
		                      : 1.0f;
		cvar_t* real = lighting_gl_modulate_cvar();
		const bool safe = std::fabs(requested - 1.0f) <= 0.0001f;
		const bool unchanged = real && std::fabs(requested - real->value) <= 0.0001f;
		if (safe || unchanged) {
			s_gl_modulate_warning_pending = false;
			s_gl_modulate_pending_since = 0;
			if (safe) {
				lighting_set_gl_modulate_value(1.0f);
			} else {
				lighting_sync_gl_modulate_mirror();
			}
			return;
		}

		// Debounce slider drags: wait until the value has been stable for a
		// moment so the player can reach the intended value before the popup
		// interrupts them.  Returning to 1 is handled above and is immediate.
		if (s_gl_modulate_pending_since &&
		    detour_Sys_Milliseconds::oSys_Milliseconds) {
			const unsigned now = static_cast<unsigned>(
				detour_Sys_Milliseconds::oSys_Milliseconds());
			if (now - s_gl_modulate_pending_since < 300u) {
				return;
			}
		}

		PrintOut(PRINT_LOG,
		         "lighting_blend: gl_modulate warning: current %g requested %g\n",
		         real ? real->value : 1.0f, requested);
		s_gl_modulate_warning_pending = false;
		s_gl_modulate_pending_since = 0;
		s_gl_modulate_warning_open = true;
		if (detour_Cbuf_AddText::oCbuf_AddText) {
			detour_Cbuf_AddText::oCbuf_AddText(
				"sofbuddy_menu sof_buddy/gl_modulate_warning\n");
		}
		return;
	}
	if (!s_gl_modulate_warning_open) {
		// This is only a display mirror; syncing it every 16 frames keeps the
		// slider current without touching the cvar path every frame.
		static unsigned sync_tick = 0;
		if (++sync_tick >= 16) {
			sync_tick = 0;
			lighting_sync_gl_modulate_mirror();
		}
	}
}

static void lighting_revert_gl_modulate_warning(const char* reason) {
	if (!s_gl_modulate_warning_open) {
		return;
	}
	s_gl_modulate_warning_open = false;
	s_gl_modulate_warning_pending = false;
	s_gl_modulate_pending_since = 0;
	cvar_t* real = lighting_gl_modulate_cvar();
	PrintOut(PRINT_LOG,
	         "lighting_blend: gl_modulate warning %s, restoring %g\n",
	         reason, real ? real->value : 1.0f);
	lighting_set_gl_modulate_value(real ? real->value : 1.0f);
}

void lighting_gl_modulate_menu_closed(void) {
	lighting_revert_gl_modulate_warning("menu closed");
}

void lighting_gl_modulate_note_menu(const char* menu_file) {
	if (!s_gl_modulate_warning_open) {
		return;
	}
	if (menu_file && strstr(menu_file, "gl_modulate_warning")) {
		return;
	}
	// Any other page replaced the popup (F12, a tab click, a console menu
	// command).  That is an implicit cancel, so never leave gl_modulate
	// showing the requested value.
	lighting_revert_gl_modulate_warning("replaced by another page");
}

static void Cmd_Lighting_ModulateConfirm_f(void) {
	if (!s_gl_modulate_warning_open) {
		return;
	}
	PrintOut(PRINT_LOG, "lighting_blend: gl_modulate warning confirmed %g\n",
	         s_gl_modulate_requested);
	lighting_set_gl_modulate_value(s_gl_modulate_requested);
	s_gl_modulate_warning_open = false;
	s_gl_modulate_warning_pending = false;
	s_gl_modulate_pending_since = 0;
	if (detour_Cbuf_AddText::oCbuf_AddText) {
		detour_Cbuf_AddText::oCbuf_AddText(
			"killmenu\nsofbuddy_menu sof_buddy/lighting\n");
	}
}

static void Cmd_Lighting_ModulateCancel_f(void) {
	if (!s_gl_modulate_warning_open) {
		return;
	}
	cvar_t* real = lighting_gl_modulate_cvar();
	PrintOut(PRINT_LOG, "lighting_blend: gl_modulate warning cancelled, keeping %g\n",
	         real ? real->value : 1.0f);
	lighting_set_gl_modulate_value(real ? real->value : 1.0f);
	s_gl_modulate_warning_open = false;
	s_gl_modulate_warning_pending = false;
	s_gl_modulate_pending_since = 0;
	if (detour_Cbuf_AddText::oCbuf_AddText) {
		detour_Cbuf_AddText::oCbuf_AddText(
			"killmenu\nsofbuddy_menu sof_buddy/lighting\n");
	}
}

/*
	Write all 11 curve sliders without invoking the curve callbacks.  Used by
	built-in presets and the curve-template command so changing a preset never
	looks like a user editing a slider.
*/
static void lighting_write_curve_cvars(const float* values) {
	if (!values || !detour_Cvar_Set2::oCvar_Set2) {
		return;
	}
	const bool old_sync = s_syncing_cvars;
	s_syncing_cvars = true;
	for (int i = 0; i < lighting_curve_detail::kCurvePointCount; ++i) {
		cvar_t* cvar = _sofbuddy_lighting_curve[i];
		if (!cvar || !cvar->name) {
			continue;
		}
		char buffer[16];
		std::snprintf(buffer, sizeof(buffer), "%d",
		              static_cast<int>(values[i] + 0.5f));
		if (!cvar->string || strcmp(cvar->string, buffer) != 0) {
			detour_Cvar_Set2::oCvar_Set2(cvar->name, buffer, true);
		}
	}
	s_syncing_cvars = old_sync;
}

static void lighting_sync_preset_curve_cvars(void) {
	const LightingCurvePreset* preset = find_curve_preset(lightblend_preset);
	if (preset) {
		lighting_write_curve_cvars(preset->values);
	}
}

static int lighting_overbright_profile_selected(void) {
	if (!_sofbuddy_lighting_overbright_profile) {
		return lighting_curve_detail::kOverbrightProfileLateHighlights;
	}
	int profile = static_cast<int>(_sofbuddy_lighting_overbright_profile->value + 0.5f);
	if (profile < 0 || profile > lighting_curve_detail::kOverbrightProfileLateHighlights) {
		profile = lighting_curve_detail::kOverbrightProfileLateHighlights;
	}
	return profile;
}

static void lighting_read_overbright_curve_points(lighting_curve_detail::CurvePoints& points) {
	const lighting_curve_detail::CurvePoints defaults =
		lighting_curve_detail::overbright_profile_points(
			lighting_curve_detail::kOverbrightProfileLateHighlights);
	for (int i = 0; i < lighting_curve_detail::kCurvePointCount; ++i) {
		points.p[i] = cvar_float_or(_sofbuddy_lighting_overbright_curve[i],
		                            defaults.p[i]);
	}
	points = lighting_curve_detail::sanitize(points);
}

static void lighting_write_overbright_curve_cvars(const float* values) {
	if (!values || !detour_Cvar_Set2::oCvar_Set2) {
		return;
	}
	const bool old_sync = s_syncing_cvars;
	s_syncing_cvars = true;
	for (int i = 0; i < lighting_curve_detail::kCurvePointCount; ++i) {
		cvar_t* cvar = _sofbuddy_lighting_overbright_curve[i];
		if (!cvar || !cvar->name) {
			continue;
		}
		char buffer[16];
		std::snprintf(buffer, sizeof(buffer), "%d",
		              static_cast<int>(values[i] + 0.5f));
		if (!cvar->string || strcmp(cvar->string, buffer) != 0) {
			detour_Cvar_Set2::oCvar_Set2(cvar->name, buffer, true);
		}
	}
	s_syncing_cvars = old_sync;
}

static void lighting_sync_overbright_profile_curve_cvars(void) {
	const int profile = lighting_overbright_profile_selected();
	if (profile == lighting_curve_detail::kOverbrightProfileCustom) {
		return;
	}
	const lighting_curve_detail::CurvePoints points =
		lighting_curve_detail::overbright_profile_points(profile);
	lighting_write_overbright_curve_cvars(points.p);
}

// False until the archived overbright curve has been migrated/detected.  The
// curve cvars fire their change callback while the config loads, so profile
// detection must not run before the migration has seen the complete curve.
static bool s_overbright_profile_migrated = false;

/*
	Migration for configs created before the Overbright profiles existed:
	- the old linear identity curve becomes the new Late highlights default
	- a hand-tuned curve is kept and marked Custom
	- a curve that matches a profile selects that profile
*/
static void lighting_migrate_overbright_profile_once(void) {
	if (s_overbright_profile_migrated || !_sofbuddy_lighting_overbright_profile) {
		return;
	}
	s_overbright_profile_migrated = true;

	const int profile = lighting_overbright_profile_selected();
	lighting_curve_detail::CurvePoints current;
	lighting_read_overbright_curve_points(current);
	const int detected = lighting_curve_detail::overbright_profile_for_points(current);

	if ((profile == lighting_curve_detail::kOverbrightProfilePoppingHighlights ||
	     profile == lighting_curve_detail::kOverbrightProfileLateHighlights) &&
	    detected == lighting_curve_detail::kOverbrightProfile2xEverything) {
		const lighting_curve_detail::CurvePoints defaults =
			lighting_curve_detail::overbright_profile_points(
				lighting_curve_detail::kOverbrightProfileLateHighlights);
		lighting_write_overbright_curve_cvars(defaults.p);
		set_cvar_string(_sofbuddy_lighting_overbright_profile, "5");
		return;
	}

	if (profile == lighting_curve_detail::kOverbrightProfileCustom || detected == profile) {
		return;
	}

	char buffer[4];
	std::snprintf(buffer, sizeof(buffer), "%d", detected);
	set_cvar_string(_sofbuddy_lighting_overbright_profile, buffer);
}

/*
	Turn the scanned lightmap histogram into the summary values used by the
	normalization range and the F12 readout.  This runs once per map load (or
	when a lit feature is turned on mid-map), so the extra arithmetic is not in
	any frame path.
*/
static void lighting_finalize_map_light_stats(void) {
	// Start at white so the first populated histogram bucket becomes the
	// absolute minimum.  (Starting at 0 made the minimum always 0, which is
	// wrong for Down normalization on maps with a raised black point.)
	s_map_min_light = 255;
	s_map_light_sum = 0;
	s_map_light_sq_sum = 0;
	s_map_light_count = 0;
	s_map_light_median = 0;
	s_map_light_p75 = 0;
	s_map_light_p90 = 0;
	s_map_light_mean = 0;
	s_map_light_std = 0;

	int max_light = 0;
	for (int i = 0; i < 256; ++i) {
		const int count = s_map_light_histogram[i];
		if (count <= 0) {
			continue;
		}
		s_map_light_count += count;
		s_map_light_sum += static_cast<long long>(i) * count;
		s_map_light_sq_sum += static_cast<long long>(i) * i * count;
		if (i < s_map_min_light) {
			s_map_min_light = i;
		}
		max_light = i;
	}
	s_map_max_light = max_light;

	if (s_map_light_count <= 0) {
		// No lit static samples at all: the world is drawn fullbright.
		s_map_min_light = 0;
		return;
	}

	s_map_light_mean = static_cast<int>(s_map_light_sum / s_map_light_count);
	const long long mean_sq = s_map_light_sq_sum / s_map_light_count;
	const long long variance = mean_sq - static_cast<long long>(s_map_light_mean) * s_map_light_mean;
	s_map_light_std = variance > 0
		? static_cast<int>(std::sqrt(static_cast<double>(variance)) + 0.5)
		: 0;

	const long long half = (s_map_light_count + 1) / 2;
	const long long p75_target = (s_map_light_count * 75 + 99) / 100;
	const long long p90_target = (s_map_light_count * 90 + 99) / 100;
	long long cumulative = 0;
	for (int i = 0; i < 256; ++i) {
		cumulative += s_map_light_histogram[i];
		if (s_map_light_median == 0 && cumulative >= half) {
			s_map_light_median = i;
		}
		if (s_map_light_p75 == 0 && cumulative >= p75_target) {
			s_map_light_p75 = i;
		}
		if (s_map_light_p90 == 0 && cumulative >= p90_target) {
			s_map_light_p90 = i;
		}
	}
}

/*
	Scan the current world model's static lightmap samples and return the
	brightest channel value.  Samples are stored per surface as
	width * height * 3 bytes for each active light style.
*/
static int lighting_scan_model_max_light(void* model_void, int* out_surfaces) {
	memset(s_map_light_histogram, 0, sizeof(s_map_light_histogram));
	if (out_surfaces) {
		*out_surfaces = 0;
	}
	if (!model_void) {
		lighting_finalize_map_light_stats();
		return 0;
	}

	unsigned char* model = reinterpret_cast<unsigned char*>(model_void);
	const int num_surfaces = *reinterpret_cast<int*>(model + kModelNumSurfacesOffset);
	unsigned char* surfaces = *reinterpret_cast<unsigned char**>(model + kModelSurfacesOffset);
	if (!surfaces || num_surfaces <= 0 || num_surfaces > 65536) {
		lighting_finalize_map_light_stats();
		return 0;
	}

	int scanned = 0;
	for (int i = 0; i < num_surfaces; ++i) {
		unsigned char* surf = surfaces + static_cast<size_t>(i) * kSurfaceStride;

		unsigned char* texinfo = *reinterpret_cast<unsigned char**>(surf + kSurfTexinfoOffset);
		if (!texinfo) {
			continue;
		}
		const int flags = *reinterpret_cast<int*>(texinfo + kTexinfoFlagsOffset);
		if (flags & kNonLitSurfaceFlags) {
			continue;
		}

		const unsigned char* styles = surf + kSurfStylesOffset;
		int num_styles = 0;
		while (num_styles < 4 && styles[num_styles] != 0xFF) {
			++num_styles;
		}
		if (num_styles == 0) {
			continue;
		}

		const unsigned char* samples = *reinterpret_cast<unsigned char**>(surf + kSurfSamplesOffset);
		if (!samples) {
			continue;
		}

		const int width = *reinterpret_cast<unsigned char*>(surf + kSurfLightmapWidthOffset);
		const int height = *reinterpret_cast<unsigned char*>(surf + kSurfLightmapHeightOffset);
		if (width <= 0 || height <= 0 || width > 128 || height > 128) {
			continue;
		}

		const int count = width * height * 3 * num_styles;
		if (count <= 0 || count > 128 * 128 * 3 * 4) {
			continue;
		}

		for (int j = 0; j < count; ++j) {
			++s_map_light_histogram[samples[j]];
		}
		++scanned;
	}

	if (out_surfaces) {
		*out_surfaces = scanned;
	}
	lighting_finalize_map_light_stats();
	return s_map_max_light;
}

/*
	Scan once for the current map.  Called at map load and again if a lit
	feature is enabled after the map was already running.
*/
static void lighting_refresh_map_light_stats(void) {
	if (s_map_max_scanned || !r_worldmodel_ptr || !*r_worldmodel_ptr) {
		return;
	}
	s_map_max_light = lighting_scan_model_max_light(*r_worldmodel_ptr, &s_map_max_surfaces);
	s_map_max_scanned = true;
	PrintOut(PRINT_LOG,
	         "lighting_blend: map light stats samples=%lld min=%d max=%d mean=%d std=%d median=%d p75=%d p90=%d surfaces=%d\n",
	         s_map_light_count, s_map_min_light, s_map_max_light, s_map_light_mean,
	         s_map_light_std, s_map_light_median, s_map_light_p75, s_map_light_p90,
	         s_map_max_surfaces);
}

static void lighting_update_normalize_status(void) {
	if (!_sb_internal_lighting_normalize_status) {
		return;
	}

	char buffer[128];
	const int mode = lighting_normalize_mode();
	const int up = lighting_normalize_up_percent();
	const int down = lighting_normalize_down_percent();
	if (mode == lighting_curve_detail::kNormalizationModeOff) {
		std::snprintf(buffer, sizeof(buffer), "%s", "Off");
	} else if (s_map_max_light <= 0) {
		const char* reason = s_map_max_scanned ? "no static samples." : "waiting for map.";
		if (mode == lighting_curve_detail::kNormalizationModeUp) {
			std::snprintf(buffer, sizeof(buffer), "Up %d%% - %s", up, reason);
		} else if (mode == lighting_curve_detail::kNormalizationModeDown) {
			std::snprintf(buffer, sizeof(buffer), "Down %d%% - %s", down, reason);
		} else {
			std::snprintf(buffer, sizeof(buffer), "Both u%d%% d%d%% - %s",
			              up, down, reason);
		}
	} else {
		const int min_reference = lighting_normalize_min_reference();
		const int max_reference = lighting_normalize_max_reference();
		if (mode == lighting_curve_detail::kNormalizationModeUp) {
			const int target = lighting_curve_detail::normalize_target_max(max_reference, up);
			std::snprintf(buffer, sizeof(buffer), "Up %d%% -> white %d/255", up, target);
		} else if (mode == lighting_curve_detail::kNormalizationModeDown) {
			const int target = lighting_curve_detail::normalize_target_min(min_reference, down);
			std::snprintf(buffer, sizeof(buffer),
			              min_reference <= 0 ? "Down %d%% - min=0: no dark point."
			                                 : "Down %d%% -> black %d",
			              down, target);
		} else {
			const int black_down = lighting_curve_detail::normalize_target_min(
				min_reference, down);
			const int white = lighting_curve_detail::normalize_target_max(
				max_reference, up);
			const float up_gain = max_reference > 0
			                          ? static_cast<float>(white) / max_reference
			                          : 1.0f;
			const int final_black = static_cast<int>(std::lround(black_down * up_gain));
			std::snprintf(buffer, sizeof(buffer), "Both u%d%% d%d%% -> %d..%d",
			              up, down, final_black, white);
		}
	}
	set_cvar_string(_sb_internal_lighting_normalize_status, buffer);
}
void lighting_apply_effective_settings(bool user_change) {
	if (!detour_Cvar_Get::oCvar_Get) {
		return;
	}

	const bool feature_enabled =
		RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_LIGHTING_BLEND);
	const bool overbright_requested = feature_enabled &&
		_sofbuddy_lighting_overbright &&
		_sofbuddy_lighting_overbright->value != 0.0f;

	if (!feature_enabled) {
		lightblend_preset = LIGHTING_PRESET_VANILLA;
	} else if (overbright_requested) {
		// Overbright Lighting is independent of the stored Lighting Style. The
		// preset cvar is left untouched so turning it back off returns to the
		// curve style the player had selected.
		lightblend_preset = LIGHTING_PRESET_RAVEN_2X;
	} else if (_sofbuddy_lighting_preset) {
		int preset = static_cast<int>(_sofbuddy_lighting_preset->value + 0.5f);
		if (preset < 0 || preset >= LIGHTING_PRESET_COUNT) {
			preset = LIGHTING_PRESET_VANILLA;
		}
		lightblend_preset = preset;
	}

	s_overbright_combiner_enabled = lighting_raven_combiner_active();
	if (lighting_raven_product_requested()) {
		if (s_overbright_combiner_enabled) {
			// Multitexture path: base is T*L, the combiner scales unit 1 by 2.
			lightblend_src = GL_ZERO;
			lightblend_dst = GL_SRC_COLOR;
		} else {
			// Single-texture path: use the real Raven product blend.
			lightblend_src = GL_DST_COLOR;
			lightblend_dst = GL_SRC_COLOR;
		}
	} else {
		switch (lightblend_preset) {
			case LIGHTING_PRESET_RAVEN_2X:
				lightblend_src = GL_DST_COLOR;
				lightblend_dst = GL_SRC_COLOR;
				break;
			case LIGHTING_PRESET_CUSTOM:
				lightblend_src = custom_lightblend_src;
				lightblend_dst = custom_lightblend_dst;
				break;
			default:
				lightblend_src = GL_ZERO;
				lightblend_dst = GL_SRC_COLOR;
				break;
		}
	}

	if (feature_enabled && s_overbright_profile_migrated) {
		lighting_sync_overbright_profile_curve_cvars();
	}
	lighting_sync_preset_curve_cvars();

	const bool changed = lighting_refresh_lut_from_gamma();
	// Must be evaluated after the LUT refresh: enabling normalization or a
	// curve changes the LUT in this call, and R_LightPoint should follow it
	// immediately.  Preserve modes keep the inline LUT at gamma, so the
	// shared normalization LUT is checked explicitly.
	s_entity_light_lut_active =
		lighting_entity_light_lut_needed() || s_normalization_lut_active;
	// DrawPlayer's hard minimum-light floor is applied after the entity LUT,
	// so update it whenever the effective LUT changes.
	lighting_update_ghoul_min_light();
	// GhoulMain is created during R_Init, which runs after RefDllLoaded.  This
	// is a no-op until the first map load / settings change.
	lighting_install_ghoul_dlight_hook();
	if (user_change && changed) {
		lighting_request_lightmap_rebuild();
	}

	// Keep the old config switch meaningful for existing configs and scripts.
	// Do not write while the feature is disabled: the Cvar_Set2 feature prompt
	// would interpret this as a user edit.
	cvar_t* custom_flag = detour_Cvar_Get::oCvar_Get("_sb_internal_lighting_custom", "0", 0, nullptr);
	if (custom_flag) {
		set_cvar_string(custom_flag, lightblend_preset == LIGHTING_PRESET_CUSTOM ? "1" : "0");
	}
	lighting_update_normalize_status();
	lighting_refresh_ui_rmf();
	if (!s_gl_modulate_warning_pending && !s_gl_modulate_warning_open) {
		lighting_sync_gl_modulate_mirror();
	}

	if (user_change) {
		lighting_sync_multitexture(true);
	}

	PrintOut(PRINT_LOG, "lighting_blend: preset=%s src=0x%X dst=0x%X color_mode=%s lut_changed=%d\n",
	         preset_name(lightblend_preset), lightblend_src, lightblend_dst,
	         lighting_color_mode_label(lighting_color_mode()),
	         changed ? 1 : 0);
}

static bool lighting_needs_legacy_blend(void) {
	if (lighting_raven_product_requested()) {
		// Raven 2x now works with either renderer path: the combiner supplies
		// the x2 when multitexture is on, and the legacy blend when it is off.
		// Never write the user's gl_ext_multitexture value for Raven.
		return false;
	}
	return lightblend_preset == LIGHTING_PRESET_CUSTOM &&
	       (custom_lightblend_src != GL_ZERO || custom_lightblend_dst != GL_SRC_COLOR);
}

static int lighting_multitexture_restore_value(void) {
	if (!_sb_internal_lighting_multitexture_restore ||
	    !_sb_internal_lighting_multitexture_restore->string) {
		return -1;
	}
	return atoi(_sb_internal_lighting_multitexture_restore->string);
}

static void lighting_set_multitexture_restore_value(int value) {
	char buffer[16];
	std::snprintf(buffer, sizeof(buffer), "%d", value);
	set_cvar_string(_sb_internal_lighting_multitexture_restore, buffer);
}

/*
	Switch gl_ext_multitexture for the legacy lightmap blend, remembering the
	user's original value. The renderer reads this cvar in R_Init, so a change
	only takes effect after VID_LoadRefresh; allow_restart controls whether we
	flag that reload. Startup restore uses allow_restart=false before the
	first renderer init.
*/
void lighting_sync_multitexture(bool allow_restart) {
	if (!gl_ext_multitexture || !detour_Cvar_Set2::oCvar_Set2) {
		return;
	}

	const bool need_off = lighting_needs_legacy_blend();
	const bool currently_off = gl_ext_multitexture->value == 0.0f;
	int restore = lighting_multitexture_restore_value();
	bool changed = false;
	const char* message = nullptr;

	if (need_off) {
		if (!currently_off) {
			if (restore < 0) {
				restore = static_cast<int>(gl_ext_multitexture->value + 0.5f);
				lighting_set_multitexture_restore_value(restore);
			}
			detour_Cvar_Set2::oCvar_Set2(const_cast<char*>("gl_ext_multitexture"),
			                             const_cast<char*>("0"), true);
			changed = true;
			message = "forced off for the legacy blend";
		}
	} else if (restore >= 0) {
		if (currently_off) {
			char buffer[16];
			std::snprintf(buffer, sizeof(buffer), "%d", restore);
			detour_Cvar_Set2::oCvar_Set2(const_cast<char*>("gl_ext_multitexture"),
			                             buffer, true);
			changed = true;
			message = "restored after the legacy blend";
		}
		// Keep the saved value until the renderer actually has the restored
		// value. Config.cfg can be executed after PostCvarInit and write the
		// old forced "0" back; RefDllLoaded will then retry and clear it.
		if (allow_restart) {
			lighting_set_multitexture_restore_value(-1);
		}
	}

	if (changed) {
		if (allow_restart) {
			cvar_t* vid_ref = findCvar(const_cast<char*>("vid_ref"));
			if (vid_ref) {
				vid_ref->modified = true;
			}
		}
		PrintOut(PRINT_DEV, "lighting_blend: gl_ext_multitexture %s; renderer restart %s.\n",
		         message, allow_restart ? "requested" : "left to the next load");
	}
}

void lighting_restore_multitexture_at_startup(void) {
	// Make sure the preset and custom blend values are current before deciding
	// whether the legacy path still needs multitexture off.
	lighting_apply_effective_settings(false);
	lighting_sync_multitexture(false);
}

/*
	CVar change callback for the independent Overbright Lighting toggle.
	The stored Lighting Style is left untouched; turning the toggle off returns
	to that curve style.
*/
void lighting_overbright_change(cvar_t * cvar) {
	if (s_syncing_cvars || !cvar) {
		return;
	}
	if (cvar->value != 0.0f) {
		lighting_refresh_map_light_stats();
	}
	lighting_apply_effective_settings(true);
	PrintOut(PRINT_LOG, "lighting_blend: overbright lighting %s\n",
	         cvar->value != 0.0f ? "on" : "off");
}

void lighting_overbright_profile_change(cvar_t * cvar) {
	if (s_syncing_cvars || !cvar) {
		return;
	}
	const int profile = lighting_overbright_profile_selected();
	if (profile != lighting_curve_detail::kOverbrightProfileCustom) {
		const lighting_curve_detail::CurvePoints points =
			lighting_curve_detail::overbright_profile_points(profile);
		lighting_write_overbright_curve_cvars(points.p);
	}
	if (lighting_raven_product_requested()) {
		lighting_apply_effective_settings(true);
	}
}

void lighting_preset_change(cvar_t * cvar) {
	if (s_syncing_cvars || !cvar) {
		return;
	}

	// Coming from Raven, the manual blend cvars often still hold the legacy
	// DST_C / SRC_C pair. Using that pair together with the custom lightmap
	// curve stacks two different overbright effects and produces very harsh,
	// channel-clipped colour. Custom starts as curve-only; the player can put
	// the legacy pair back explicitly if they want the stack.
	const int requested = static_cast<int>(cvar->value + 0.5f);

	if (requested == LIGHTING_PRESET_RAVEN_2X) {
		// Old configs and scripts used preset 4 for Raven 2x. The UI now
		// exposes it as the independent Overbright Lighting toggle; keep the
		// old value working and leave a curve style selected underneath.
		set_cvar_string(_sofbuddy_lighting_overbright, "1");
		set_cvar_string(cvar, "0");
		lighting_apply_effective_settings(true);
		return;
	}

	// Built-in curve presets are tuned for Preserve brightness. Selecting one
	// switches the colour mode back to that default, while Custom leaves the
	// player's current mode alone.
	if (is_curve_preset(requested) && _sofbuddy_lighting_preserve_color) {
		set_cvar_string(_sofbuddy_lighting_preserve_color, "1");
	}

	if (requested == LIGHTING_PRESET_CUSTOM &&
	    lightblend_preset != LIGHTING_PRESET_CUSTOM &&
	    custom_lightblend_src == GL_DST_COLOR &&
	    custom_lightblend_dst == GL_SRC_COLOR) {
		set_cvar_string(_sofbuddy_lightblend_src, "GL_ZERO");
		set_cvar_string(_sofbuddy_lightblend_dst, "GL_SRC_COLOR");
		custom_lightblend_src = GL_ZERO;
		custom_lightblend_dst = GL_SRC_COLOR;
	}

	lighting_apply_effective_settings(true);
}

void lighting_curve_change(cvar_t * cvar) {
	if (s_syncing_cvars || !cvar) {
		return;
	}
	if (lightblend_preset != LIGHTING_PRESET_CUSTOM) {
		if (is_curve_preset(lightblend_preset)) {
			// The user touched a slider while a built-in preset was active.
			// Keep their edit and drop into Custom instead of snapping back.
			set_cvar_string(_sofbuddy_lighting_preset, "5");
			PrintOut(PRINT_LOG, "lighting_blend: curve slider edited; switched Lighting Style to Custom\n");
			lighting_apply_effective_settings(true);
		} else {
			// Keep the values ready for when the player selects Custom, but
			// do not disturb Vanilla or Raven.
			PrintOut(PRINT_DEV, "lighting_blend: stored custom curve value %s=%s (select Custom to use it)\n",
			         cvar->name, cvar->string ? cvar->string : "");
		}
		return;
	}
	lighting_apply_effective_settings(true);
}

void lighting_overbright_curve_change(cvar_t * cvar) {
	if (s_syncing_cvars || !cvar) {
		return;
	}

	// Keep the profile list honest: an edited curve becomes Custom unless it
	// happens to match another built-in overbright profile exactly.  Skip this
	// while the archived config is still loading; migration handles the
	// complete curve once all 11 values are available.
	if (s_overbright_profile_migrated) {
		lighting_curve_detail::CurvePoints current;
		lighting_read_overbright_curve_points(current);
		const int detected = lighting_curve_detail::overbright_profile_for_points(current);
		if (detected != lighting_overbright_profile_selected()) {
			char buffer[4];
			std::snprintf(buffer, sizeof(buffer), "%d", detected);
			set_cvar_string(_sofbuddy_lighting_overbright_profile, buffer);
		}
	}

	if (!lighting_raven_product_requested()) {
		// Keep the values ready for the next time Overbright is enabled.
		return;
	}
	lighting_apply_effective_settings(true);
}

void lighting_preserve_color_change(cvar_t * cvar) {
	if (s_syncing_cvars || !cvar) {
		return;
	}
	lighting_apply_effective_settings(true);

	// Preserve brightness and preserve colour share the same curve LUT, so
	// switching between them does not change the LUTs. Force a rebuild anyway
	// so existing lightmaps are re-uploaded with the newly selected maths.
	if (is_curve_preset(lightblend_preset) ||
	    lightblend_preset == LIGHTING_PRESET_CUSTOM ||
	    lighting_raven_product_requested()) {
		lighting_request_lightmap_rebuild();
	}
}

void lighting_normalize_change(cvar_t * cvar) {
	if (s_syncing_cvars || !cvar) {
		return;
	}

	// Enabling mid-map needs the observed maximum before the LUT can be built.
	// The stats are reset on every map load, so a cached scan is always for
	// the map that is currently running.
	if (lighting_normalization_enabled()) {
		lighting_refresh_map_light_stats();
	}

	lighting_apply_effective_settings(true);

	if (lighting_normalization_enabled()) {
		const float gain = lighting_normalize_gain();
		PrintOut(PRINT_LOG,
		         "lighting_blend: map lighting normalization %s (min=%d max=%d/255, surfaces=%d, up=%d%%, down=%d%%, gain=%.2fx)\n",
		         lighting_normalize_mode_label(lighting_normalize_mode()),
		         s_map_min_light, s_map_max_light, s_map_max_surfaces,
		         lighting_normalize_up_percent(), lighting_normalize_down_percent(), gain);
	} else {
		PrintOut(PRINT_LOG, "lighting_blend: map lighting normalization off\n");
	}
}

void lighting_normalize_amount_change(cvar_t * cvar) {
	if (s_syncing_cvars || !cvar) {
		return;
	}
	// The map min/max do not change, but the LUT and any baked lightmaps do.
	lighting_apply_effective_settings(true);
	PrintOut(PRINT_LOG, "lighting_blend: normalization amounts up=%d%% down=%d%%\n",
	         lighting_normalize_up_percent(), lighting_normalize_down_percent());
}

void lighting_ghoul_min_light_change(cvar_t * cvar) {
	if (s_syncing_cvars || !cvar) {
		return;
	}
	lighting_update_ghoul_min_light();
	PrintOut(PRINT_LOG, "lighting_blend: Ghoul minimum light %s\n",
	         cvar->value >= 0.5f ? "bypassed" : "vanilla");
}

/*
	Called before R_BeginRegistration starts loading a new BSP.  Clear the old
	map's normalized range so the initial lightmap pass is built without stale
	normalization; the post-hook scans the new map and refreshes it.
*/
void lighting_cancel_pending_rebuild(void) {
	s_rebuild_pending = false;
	s_rebuild_delay = 0;
}

void lighting_on_map_load_begin(void) {
	// The archived config has finished loading by the time a map is started.
	// Migrate the pre-profile overbright curve now, when all 11 slider values
	// are available, instead of during the partial config exec.
	if (RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_LIGHTING_BLEND)) {
		lighting_migrate_overbright_profile_once();
	}
	set_cvar_string(_sb_internal_lighting_style0_status, "");
	s_map_max_light = 0;
	s_map_max_surfaces = 0;
	s_map_max_scanned = false;
	// Do not apply the old map's LUT to entity samples while the new map is
	// loading; lighting_on_map_loaded refreshes the flag after the scan.
	s_entity_light_lut_active = false;
	s_map_min_light = 0;
	s_map_light_sum = 0;
	s_map_light_sq_sum = 0;
	s_map_light_count = 0;
	s_map_light_median = 0;
	s_map_light_p75 = 0;
	s_map_light_p90 = 0;
	s_map_light_mean = 0;
	s_map_light_std = 0;
	memset(s_map_light_histogram, 0, sizeof(s_map_light_histogram));
	// The old world model is about to be replaced.  A rebuild queued while the
	// previous map was still loaded must not run against the half-loaded or
	// partially freed model; the new map is built with the current LUT below.
	lighting_cancel_pending_rebuild();
	lighting_apply_effective_settings(false);
	PrintOut(PRINT_LOG, "lighting_blend: map load begin; normalization range reset\n");
}

/*
	R_BeginRegistration returns the new world model after the BSP, surfaces
	and static lightmap samples have been loaded.  Scan its samples, rebuild
	the LUT for the new map's range, and queue a lightmap refresh if the LUT
	changed.  This also clears the previous map's normalization when off.
*/
void lighting_on_map_loaded(void* model) {
	if (RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_LIGHTING_BLEND)) {
		lighting_migrate_overbright_profile_once();
	}
	s_map_max_light = 0;
	s_map_max_surfaces = 0;
	s_map_max_scanned = false;

	const bool overbright_requested =
		RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_LIGHTING_BLEND) &&
		_sofbuddy_lighting_overbright &&
		_sofbuddy_lighting_overbright->value != 0.0f;
	if (lighting_normalization_enabled() || overbright_requested) {
		s_map_max_light = lighting_scan_model_max_light(model, &s_map_max_surfaces);
		s_map_max_scanned = true;
		PrintOut(PRINT_LOG,
		         "lighting_blend: map light stats samples=%lld min=%d max=%d mean=%d std=%d median=%d p75=%d p90=%d surfaces=%d\n",
		         s_map_light_count, s_map_min_light, s_map_max_light, s_map_light_mean,
		         s_map_light_std, s_map_light_median, s_map_light_p75, s_map_light_p90,
		         s_map_max_surfaces);
	}

	unsigned char previous_lightmap[256];
	unsigned char previous_preserve[256];
	unsigned char previous_normalization[256];
	memcpy(previous_lightmap, lightmap_curve_lut, sizeof(previous_lightmap));
	memcpy(previous_preserve, preserve_color_curve_lut, sizeof(previous_preserve));
	memcpy(previous_normalization, s_normalization_lut, sizeof(previous_normalization));
	lighting_apply_effective_settings(false);
	const bool changed =
		memcmp(previous_lightmap, lightmap_curve_lut, sizeof(previous_lightmap)) != 0 ||
		memcmp(previous_preserve, preserve_color_curve_lut, sizeof(previous_preserve)) != 0 ||
		memcmp(previous_normalization, s_normalization_lut, sizeof(previous_normalization)) != 0;
	if (changed) {
		lighting_request_lightmap_rebuild();
	}

	const float gain = lighting_normalize_gain();
	PrintOut(PRINT_LOG,
	         "lighting_blend: map loaded; overbright=%s normalization=%s min=%d max=%d/255 surfaces=%d gain=%.2fx\n",
	         overbright_requested ? "on" : "off",
	         lighting_normalize_mode_label(lighting_normalize_mode()),
	         s_map_min_light, s_map_max_light, s_map_max_surfaces, gain);
}

void lighting_request_lightmap_rebuild(void) {
	s_rebuild_pending = true;
	s_rebuild_delay = 2;
}

static void resolve_gl_upload_functions(void) {
	if (s_glBindTexture && s_glTexSubImage2D && s_glGetIntegerv && s_glTexEnvi) {
		return;
	}
	HMODULE gl = GetModuleHandleA("opengl32.dll");
	if (!gl) {
		return;
	}
	if (!s_glBindTexture) {
		s_glBindTexture = reinterpret_cast<glBindTexture_t>(GetProcAddress(gl, "glBindTexture"));
	}
	if (!s_glTexSubImage2D) {
		s_glTexSubImage2D = reinterpret_cast<glTexSubImage2D_t>(GetProcAddress(gl, "glTexSubImage2D"));
	}
	if (!s_glGetIntegerv) {
		s_glGetIntegerv = reinterpret_cast<glGetIntegerv_t>(GetProcAddress(gl, "glGetIntegerv"));
	}
	if (!s_glTexEnvi) {
		s_glTexEnvi = reinterpret_cast<glTexEnvi_t>(GetProcAddress(gl, "glTexEnvi"));
	}
}

/*
	Preserve-mode post-process for one finished R_BuildLightMap buffer.
	The inline LUT is identity in these modes, so the buffer contains normal
	gamma-corrected lightmap values.  Preserve brightness applies the curve to
	luminance; preserve colour applies it to the brightest channel.  Both scale
	RGB by the same factor.  Map normalization is folded into that scalar so it
	also scales uniformly and does not break the preserved channel ratios.
*/
void lighting_apply_preserve_color_to_buffer(int surf_int, void* dest, int pitch) {
	const int color_mode = lighting_color_mode();
	if (!RuntimeFeatures::Enabled(RuntimeFeatures::Feature::RUNTIME_FEATURE_LIGHTING_BLEND) ||
	    color_mode == LIGHTING_COLOR_PER_CHANNEL ||
	    !surf_int || !dest || pitch <= 0) {
		return;
	}

	unsigned char* surf = reinterpret_cast<unsigned char*>(static_cast<uintptr_t>(surf_int));
	int width;
	int height;
	const bool has_samples = *reinterpret_cast<void**>(surf + kSurfSamplesOffset) != nullptr;
	if (has_samples) {
		width = *reinterpret_cast<unsigned char*>(surf + kSurfLightmapWidthOffset);
		height = *reinterpret_cast<unsigned char*>(surf + kSurfLightmapHeightOffset);
	} else {
		width = (static_cast<int>(*reinterpret_cast<short*>(surf + 0x14)) >> 4) + 1;
		height = (static_cast<int>(*reinterpret_cast<short*>(surf + 0x16)) >> 4) + 1;
	}
	if (width <= 0 || height <= 0 || width > 128 || height > 128 || pitch < width * 4) {
		return;
	}

	const unsigned char* normalize_lut =
		s_normalization_lut_active ? s_normalization_lut : nullptr;
	void (*apply)(unsigned char&, unsigned char&, unsigned char&,
	              const unsigned char*, const unsigned char*) =
		color_mode == LIGHTING_COLOR_PRESERVE_BRIGHTNESS
			? lighting_curve_detail::preserve_luminance_byte
			: lighting_curve_detail::preserve_value_byte;

	unsigned char* row = reinterpret_cast<unsigned char*>(dest);
	for (int y = 0; y < height; ++y) {
		unsigned char* pixel = row;
		for (int x = 0; x < width; ++x) {
			apply(pixel[0], pixel[1], pixel[2], preserve_color_curve_lut, normalize_lut);
			pixel += 4;
		}
		row += pitch;
	}
}

/*
	R_LightPoint returns the lightmap light used by entities, particles, and the
	Ghoul model renderer.  It samples raw lightmap bytes, so it bypasses the
	lightmap LUT that normalizes and curves world surfaces.  Mirror the world
	transform here: gamma, normalization reference, then the same per-channel
	or preserve-mode curve.  The caller only enables this when the LUT differs
	from stock, so Vanilla entity lighting is untouched.
*/
void lighting_apply_entity_light_lut(float* color) {
	if (!s_entity_light_lut_active || !color) {
		return;
	}

	const unsigned char* normalize_lut =
		s_normalization_lut_active ? s_normalization_lut : nullptr;

	int raw[3];
	for (int i = 0; i < 3; ++i) {
		float value = color[i];
		if (!(value > 0.0f)) {
			value = 0.0f;
		} else if (value > 1.0f) {
			value = 1.0f;
		}
		raw[i] = static_cast<int>(value * 255.0f + 0.5f);
	}

	const int color_mode = lighting_color_mode();
	unsigned char out[3];
	if (color_mode == LIGHTING_COLOR_PER_CHANNEL) {
		out[0] = lightmap_curve_lut[raw[0]];
		out[1] = lightmap_curve_lut[raw[1]];
		out[2] = lightmap_curve_lut[raw[2]];
	} else {
		unsigned char r = lighting_gamma_table ? lighting_gamma_table[raw[0]]
		                                      : static_cast<unsigned char>(raw[0]);
		unsigned char g = lighting_gamma_table ? lighting_gamma_table[raw[1]]
		                                      : static_cast<unsigned char>(raw[1]);
		unsigned char b = lighting_gamma_table ? lighting_gamma_table[raw[2]]
		                                      : static_cast<unsigned char>(raw[2]);
		if (color_mode == LIGHTING_COLOR_PRESERVE_BRIGHTNESS) {
			lighting_curve_detail::preserve_luminance_byte(
				r, g, b, preserve_color_curve_lut, normalize_lut);
		} else {
			lighting_curve_detail::preserve_value_byte(
				r, g, b, preserve_color_curve_lut, normalize_lut);
		}
		out[0] = r;
		out[1] = g;
		out[2] = b;
	}

	static bool logged = false;
	if (!logged) {
		logged = true;
		PrintOut(PRINT_LOG,
		         "lighting_blend: entity light LUT mode=%s normalize=%s raw=(%d,%d,%d) out=(%d,%d,%d)\n",
		         lighting_color_mode_label(color_mode),
		         normalize_lut ? lighting_normalize_mode_label(lighting_normalize_mode()) : "off",
		         raw[0], raw[1], raw[2], out[0], out[1], out[2]);
	}

	color[0] = static_cast<float>(out[0]) * (1.0f / 255.0f);
	color[1] = static_cast<float>(out[1]) * (1.0f / 255.0f);
	color[2] = static_cast<float>(out[2]) * (1.0f / 255.0f);
}

/*
	Experimental Raven-compatible multitexture path.

	GL_RenderLightmappedPoly normally leaves unit 1 at GL_MODULATE, so the
	fragment is T * L.  The patched CheckComplexStates call inside that function
	switches unit 1 to a combiner that multiplies PREVIOUS (unit 0 = T) by the
	lightmap and scales the result by 2, matching Raven's 2 * T * L product
	with multitexture on.  The patched glEnd call restores GL_MODULATE.
*/
void lighting_set_lightmap_combine(bool enable) {
	if (!gl_select_texture_sgis || !*gl_select_texture_sgis ||
	    !engine_active_texture) {
		return;
	}
	if (!enable && !s_combine_active) {
		return;
	}

	resolve_gl_upload_functions();
	if (!s_glTexEnvi) {
		return;
	}

	// Use the engine's own qglActiveTextureARB wrapper so gl_state_currenttmu
	// stays in sync with the real GL texture unit.  Calling the raw
	// glSelectTextureSGIS pointer desyncs the engine's texture-binding cache
	// and corrupts world textures.
	engine_active_texture(GL_TEXTURE1_ARB);
	if (enable) {
		s_glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
		s_glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);
		s_glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PREVIOUS_ARB);
		s_glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_TEXTURE);
		s_glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 2);
		s_glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, GL_MODULATE);
		s_glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB, GL_PREVIOUS_ARB);
		s_glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_ARB, GL_TEXTURE);
		s_glTexEnvi(GL_TEXTURE_ENV, GL_ALPHA_SCALE, 1);
		s_combine_active = true;
	} else {
		s_glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		s_combine_active = false;
	}
	engine_active_texture(GL_TEXTURE0_ARB);
}

void lighting_check_complex_states_for_lightmap(void) {
	if (oCheckComplexStates) {
		oCheckComplexStates();
	}
	if (s_overbright_combiner_enabled) {
		lighting_set_lightmap_combine(true);
		static bool logged = false;
		if (!logged) {
			logged = true;
			PrintOut(PRINT_LOG, "lighting_blend: raven 2x using the multitexture combiner\n");
		}
	}
}

void lighting_glend_for_lightmap(void) {
	if (glEnd) {
		glEnd();
	}
	// Fast path while Overbright is off; only restore if this poly actually
	// enabled the combiner.
	if (s_combine_active) {
		lighting_set_lightmap_combine(false);
	}
}

/*
	Re-upload every lit world surface from its existing lightmap samples using
	the current LUT.  Animated-style surfaces are included with the current
	style values, then their cache state is refreshed so a later style change
	still triggers a normal rebuild.  The atlas coordinates and texture indices
	are unchanged, so this is just a refresh, not a re-pack.
*/
void lighting_rebuild_world_lightmaps(void) {
	if (!r_worldmodel_ptr || !*r_worldmodel_ptr ||
	    !gl_state_lightmap_textures_ptr ||
	    !detour_R_BuildLightMap::oR_BuildLightMap) {
		s_rebuild_pending = false;
		return;
	}

	resolve_gl_upload_functions();
	if (!s_glBindTexture || !s_glTexSubImage2D || !s_glGetIntegerv) {
		PrintOut(PRINT_BAD, "lighting_blend: lightmap rebuild skipped (OpenGL upload functions unavailable)\n");
		s_rebuild_pending = false;
		return;
	}

	static unsigned char scratch[128 * 128 * 4];
	int rebuilt = 0;
	int skipped = 0;

	auto rebuild_model = [&](unsigned char* model) {
		const int num_surfaces = *reinterpret_cast<int*>(model + kModelNumSurfacesOffset);
		unsigned char* surfaces = *reinterpret_cast<unsigned char**>(model + kModelSurfacesOffset);
		if (!surfaces || num_surfaces <= 0 || num_surfaces > 65536) {
			return;
		}

		for (int i = 0; i < num_surfaces; ++i) {
			unsigned char* surf = surfaces + static_cast<size_t>(i) * kSurfaceStride;

			unsigned char* texinfo = *reinterpret_cast<unsigned char**>(surf + kSurfTexinfoOffset);
			if (!texinfo) {
				++skipped;
				continue;
			}
			const int flags = *reinterpret_cast<int*>(texinfo + kTexinfoFlagsOffset);
			if (flags & kNonLitSurfaceFlags) {
				++skipped;
				continue;
			}

			const int lightmap_index = *reinterpret_cast<int*>(surf + kSurfLightmapIndexOffset);
			if (lightmap_index <= 0 || lightmap_index >= 128) {
				++skipped;
				continue;
			}

			int width;
			int height;
			const bool has_samples = *reinterpret_cast<void**>(surf + kSurfSamplesOffset) != nullptr;
			if (has_samples) {
				// R_BuildLightMap itself uses these two bytes whenever samples
				// is present, so do not fall back to extents here: that would
				// upload a stale scratch row over the atlas.
				width = *reinterpret_cast<unsigned char*>(surf + kSurfLightmapWidthOffset);
				height = *reinterpret_cast<unsigned char*>(surf + kSurfLightmapHeightOffset);
			} else {
				width = (static_cast<int>(*reinterpret_cast<short*>(surf + 0x14)) >> 4) + 1;
				height = (static_cast<int>(*reinterpret_cast<short*>(surf + 0x16)) >> 4) + 1;
			}

			const int atlas_x = *reinterpret_cast<int*>(surf + kSurfLightmapXOffset);
			const int atlas_y = *reinterpret_cast<int*>(surf + kSurfLightmapYOffset);
			if (width <= 0 || height <= 0 || width > 128 || height > 128 ||
			    atlas_x < 0 || atlas_y < 0 || atlas_x + width > 128 || atlas_y + height > 128) {
				++skipped;
				continue;
			}

			int* dlight_frame = reinterpret_cast<int*>(surf + kSurfDlightFrameOffset);
			const int saved_dlight_frame = *dlight_frame;
			// Never bake per-frame dynamic lights into the static atlas.
			*dlight_frame = 0;
			detour_R_BuildLightMap::oR_BuildLightMap(reinterpret_cast<int>(surf), scratch, width * 4);
			lighting_apply_preserve_color_to_buffer(reinterpret_cast<int>(surf), scratch, width * 4);
			*dlight_frame = saved_dlight_frame;

			const unsigned int texture = static_cast<unsigned int>(*gl_state_lightmap_textures_ptr + lightmap_index);
			s_glBindTexture(GL_TEXTURE_2D, texture);
			s_glTexSubImage2D(GL_TEXTURE_2D, 0, atlas_x, atlas_y, width, height,
			                  GL_RGBA, GL_UNSIGNED_BYTE, scratch);

			// Record the style values that were just baked.  If an animated
			// style changes, isCachedLight() will still rebuild the surface.
			if (lighting_set_cache_state) {
				lighting_set_cache_state(reinterpret_cast<int>(surf));
			}
			++rebuilt;
		}
	};

	int previous_texture = 0;
	s_glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);

	unsigned char* world = reinterpret_cast<unsigned char*>(*r_worldmodel_ptr);
	rebuild_model(world);

	s_glBindTexture(GL_TEXTURE_2D, static_cast<unsigned int>(previous_texture));

	s_rebuild_pending = false;
	s_rebuild_delay = 0;
	PrintOut(PRINT_LOG, "lighting_blend: rebuilt %d world lightmaps (%d skipped)\n",
	         rebuilt, skipped);
}

void lighting_frame_tick(void) {
	static bool logged = false;
	if (!logged) {
		logged = true;
		PrintOut(PRINT_LOG, "lighting_blend: R_SortAndDraw post hook reached\n");
	}
	lighting_gl_modulate_tick();
	{
		// Style 0 can be map-authored or animated.  It changes rarely; update
		// the menu readout a few times per second instead of every frame.
		static unsigned style0_tick = 0;
		if (++style0_tick >= 16) {
			style0_tick = 0;
			lighting_update_style0_status();
		}
	}
	if (lighting_gamma_table) {
		// Gamma is a user setting, not per-frame animation. Checking the 256-byte
		// table every frame is unnecessary; 16 frames is still visually immediate
		// when vid_gamma/brightness changes.
		static unsigned gamma_tick = 0;
		if (++gamma_tick >= 16) {
			gamma_tick = 0;
			if (!s_have_last_gamma || memcmp(s_last_gamma, lighting_gamma_table, sizeof(s_last_gamma))) {
				const bool first = !s_have_last_gamma;
				memcpy(s_last_gamma, lighting_gamma_table, sizeof(s_last_gamma));
				s_have_last_gamma = true;
				if (!first) {
					lighting_refresh_lut_from_gamma();
					lighting_update_normalize_status();
					lighting_update_ghoul_min_light();
					lighting_request_lightmap_rebuild();
				}
			}
		}
	}

	if (!s_rebuild_pending) {
		return;
	}
	if (s_rebuild_delay > 0) {
		--s_rebuild_delay;
		return;
	}
	lighting_rebuild_world_lightmaps();
}

void lighting_reset_runtime_state(void) {
	s_have_last_gamma = false;
	s_rebuild_pending = false;
	s_rebuild_delay = 0;
	s_combine_active = false;
	s_entity_light_lut_active = false;
	s_ghoul_min_light_warned = false;
	s_glBindTexture = nullptr;
	s_glTexSubImage2D = nullptr;
	s_glGetIntegerv = nullptr;
	s_glTexEnvi = nullptr;
}

/*
	Redirect R_BuildLightMap's three gamma-table reads to our LUT.  The
	instruction forms are checked first so a different ref_gl build is left
	untouched instead of being corrupted.
*/
bool lighting_patch_lut_reads(void) {
	if (!lighting_gamma_table) {
		return false;
	}

	struct Patch {
		unsigned int operand_rva;
		unsigned char op0;
		unsigned char op1;
	};
	static const Patch patches[] = {
		{0x00099D5, 0x8A, 0x97}, // mov dl,  byte_3004C520[edi]
		{0x00099DD, 0x8A, 0x8B}, // mov cl,  byte_3004C520[ebx]
		{0x00099E6, 0x8A, 0x95}, // mov dl,  byte_3004C520[ebp]
	};

	for (size_t i = 0; i < sizeof(patches) / sizeof(patches[0]); ++i) {
		unsigned char* instruction = reinterpret_cast<unsigned char*>(
			rvaToAbsRef(reinterpret_cast<void*>(patches[i].operand_rva - 2)));
		if (!instruction) {
			return false;
		}
		if (instruction[0] != patches[i].op0 || instruction[1] != patches[i].op1) {
			PrintOut(PRINT_BAD,
			         "lighting_blend: per-channel LUT patch %u skipped (unexpected opcodes %02X %02X)\n",
			         static_cast<unsigned>(i), instruction[0], instruction[1]);
			return false;
		}
		const unsigned int original_operand = *reinterpret_cast<unsigned int*>(instruction + 2);
		if (original_operand != (unsigned int)(uintptr_t)lighting_gamma_table) {
			PrintOut(PRINT_BAD,
			         "lighting_blend: per-channel LUT patch %u skipped (operand 0x%X, expected 0x%X)\n",
			         static_cast<unsigned>(i), original_operand,
			         (unsigned int)(uintptr_t)lighting_gamma_table);
			return false;
		}
	}

	for (size_t i = 0; i < sizeof(patches) / sizeof(patches[0]); ++i) {
		void* operand = rvaToAbsRef(reinterpret_cast<void*>(patches[i].operand_rva));
		writeUnsignedIntegerAt(operand, (unsigned int)(uintptr_t)lightmap_curve_lut);
		FlushInstructionCache(GetCurrentProcess(), operand, sizeof(unsigned int));
	}

	PrintOut(PRINT_LOG, "lighting_blend: per-channel lightmap curve LUT installed at %p\n", lightmap_curve_lut);
	return true;
}

static void Cmd_Lighting_Rebuild_f(void) {
	lighting_request_lightmap_rebuild();
	PrintOut(PRINT_OUT, "lighting_blend: lightmap rebuild queued for the next frame.\n");
}

static void Cmd_Lighting_Reset_f(void) {
	if (!detour_Cvar_Set2::oCvar_Set2) {
		return;
	}

	// Reset the custom blend first, then the preset, so an intermediate
	// non-default factor cannot leave the Custom preset selected.
	detour_Cvar_Set2::oCvar_Set2(const_cast<char*>("_sofbuddy_lightblend_src"), const_cast<char*>("GL_ZERO"), true);
	detour_Cvar_Set2::oCvar_Set2(const_cast<char*>("_sofbuddy_lightblend_dst"), const_cast<char*>("GL_SRC_COLOR"), true);
	lighting_reset_curve_cvars();
	lighting_reset_overbright_curve_cvars();
	set_cvar_string(_sofbuddy_lighting_overbright_profile, "5");
	detour_Cvar_Set2::oCvar_Set2(const_cast<char*>("_sofbuddy_lighting_preserve_color"), const_cast<char*>("1"), true);
	detour_Cvar_Set2::oCvar_Set2(const_cast<char*>("_sofbuddy_lighting_normalize"), const_cast<char*>("0"), true);
	detour_Cvar_Set2::oCvar_Set2(const_cast<char*>("_sofbuddy_lighting_normalize_up"), const_cast<char*>("100"), true);
	detour_Cvar_Set2::oCvar_Set2(const_cast<char*>("_sofbuddy_lighting_normalize_down"), const_cast<char*>("100"), true);
	detour_Cvar_Set2::oCvar_Set2(const_cast<char*>("_sofbuddy_lighting_ghoul_min_light"), const_cast<char*>("0"), true);
	detour_Cvar_Set2::oCvar_Set2(const_cast<char*>("_sofbuddy_lighting_overbright"), const_cast<char*>("0"), true);
	detour_Cvar_Set2::oCvar_Set2(const_cast<char*>("_sofbuddy_lighting_preset"), const_cast<char*>("0"), true);
	detour_Cvar_Set2::oCvar_Set2(const_cast<char*>("_sofbuddy_lighting_cutoff"), const_cast<char*>("64"), true);
	detour_Cvar_Set2::oCvar_Set2(const_cast<char*>("_sofbuddy_water_size"), const_cast<char*>("64"), true);
	detour_Cvar_Set2::oCvar_Set2(const_cast<char*>("_sofbuddy_shiny_spherical"), const_cast<char*>("1"), true);

	lighting_apply_effective_settings(true);
	PrintOut(PRINT_OUT, "lighting_blend: lighting defaults restored.\n");
}

static void Cmd_Lighting_Curve_Reset_f(void) {
	lighting_reset_curve_cvars();
	lighting_apply_effective_settings(true);
	PrintOut(PRINT_OUT, "lighting_blend: custom curve reset to linear (0,10,...,100).\n");
}

static void Cmd_Lighting_Overbright_Curve_Reset_f(void) {
	lighting_reset_overbright_curve_cvars();
	set_cvar_string(_sofbuddy_lighting_overbright_profile, "5");
	lighting_apply_effective_settings(true);
}

static void Cmd_Lighting_Status_f(void) {
	char curve[160];
	int used = 0;
	curve[0] = '\0';
	for (int i = 0; i < lighting_curve_detail::kCurvePointCount; ++i) {
		const int value = _sofbuddy_lighting_curve[i]
			? static_cast<int>(_sofbuddy_lighting_curve[i]->value + 0.5f)
			: i * 10;
		if (i == 0) {
			used = std::snprintf(curve, sizeof(curve), "%d", value);
		} else {
			used += std::snprintf(curve + used, sizeof(curve) - used, ",%d", value);
		}
		if (used >= static_cast<int>(sizeof(curve))) break;
	}
	PrintOut(PRINT_OUT, "lighting_blend: preset=%s src=0x%X dst=0x%X color=%s normalize=%s up=%d%% down=%d%% min=%d max=%d gain=%.2fx overbright=%s combiner=%s entity_lut=%s curve=%s\n",
	         preset_name(lightblend_preset), lightblend_src, lightblend_dst,
	         lighting_color_mode_label(lighting_color_mode()),
	         lighting_normalize_mode_label(lighting_normalize_mode()),
	         lighting_normalize_up_percent(), lighting_normalize_down_percent(),
	         s_map_min_light, s_map_max_light, lighting_normalize_gain(),
	         _sofbuddy_lighting_overbright && _sofbuddy_lighting_overbright->value != 0.0f ? "on" : "off",
	         lighting_raven_combiner_active() ? "on" : "off",
	         s_entity_light_lut_active ? "on" : "off",
	         curve);

	if (s_map_max_scanned) {
		PrintOut(PRINT_OUT,
		         "lighting_blend: map min=%d max=%d mean=%d median=%d p75=%d p90=%d surfaces=%d\n",
		         s_map_min_light, s_map_max_light, s_map_light_mean, s_map_light_median,
		         s_map_light_p75, s_map_light_p90, s_map_max_surfaces);
	}

	float style0_r;
	float style0_g;
	float style0_b;
	if (lighting_read_style0(style0_r, style0_g, style0_b)) {
		const float style0_value = (style0_r + style0_g + style0_b) / 3.0f;
		const char style0_letter = lighting_style0_letter(style0_value);
		char letter_buffer[8] = "";
		if (style0_letter) {
			std::snprintf(letter_buffer, sizeof(letter_buffer), " (%c)", style0_letter);
		}
		PrintOut(PRINT_OUT,
		         "lighting_blend: map_style0=%.2f%s rgb=%.2f/%.2f/%.2f\n",
		         style0_value, letter_buffer,
		         style0_r, style0_g, style0_b);
	}

	if (lighting_raven_product_requested()) {
		char ob_curve[160];
		int ob_used = 0;
		ob_curve[0] = '\0';
		const lighting_curve_detail::CurvePoints defaults =
			lighting_curve_detail::overbright_profile_points(
				lighting_curve_detail::kOverbrightProfileLateHighlights);
		for (int i = 0; i < lighting_curve_detail::kCurvePointCount; ++i) {
			const int value = _sofbuddy_lighting_overbright_curve[i]
				? static_cast<int>(_sofbuddy_lighting_overbright_curve[i]->value + 0.5f)
				: static_cast<int>(defaults.p[i] + 0.5f);
			if (i == 0) {
				ob_used = std::snprintf(ob_curve, sizeof(ob_curve), "%d", value);
			} else {
				ob_used += std::snprintf(ob_curve + ob_used, sizeof(ob_curve) - ob_used, ",%d", value);
			}
			if (ob_used >= static_cast<int>(sizeof(ob_curve))) break;
		}
		PrintOut(PRINT_OUT, "lighting_blend: overbright_precurve=%s profile=%s\n",
		         ob_curve,
		         lighting_curve_detail::overbright_profile_name(
		             lighting_overbright_profile_selected()));
	}
}

static void Cmd_Lighting_Curve_Templates_f(void) {
	PrintOut(PRINT_OUT, "lighting_blend: curve templates:\n");
	for (const LightingCurvePreset& preset : s_curve_presets) {
		PrintOut(PRINT_OUT, "  %s  %s\n", preset.alias, preset.name);
	}
	PrintOut(PRINT_OUT, "usage: sofbuddy_lighting_curve_template <alias>\n");
}

static void Cmd_Lighting_Curve_Template_f(void) {
	if (!detour_Cmd_Argc::oCmd_Argc || !detour_Cmd_Argv::oCmd_Argv) {
		return;
	}
	if (detour_Cmd_Argc::oCmd_Argc() < 2) {
		Cmd_Lighting_Curve_Templates_f();
		return;
	}

	const char* token = detour_Cmd_Argv::oCmd_Argv(1);
	const LightingCurvePreset* preset = find_curve_preset_by_token(token);
	if (!preset) {
		PrintOut(PRINT_BAD, "lighting_blend: unknown curve template '%s'\n", token ? token : "");
		Cmd_Lighting_Curve_Templates_f();
		return;
	}

	lighting_write_curve_cvars(preset->values);
	set_cvar_string(_sofbuddy_lighting_preserve_color, "1");
	set_cvar_string(_sofbuddy_lighting_preset, "5");
	lighting_apply_effective_settings(true);
	PrintOut(PRINT_OUT,
	         "lighting_blend: applied curve template '%s' (%s); Lighting Style = Custom.\n",
	         preset->name, preset->alias);
}

static void Cmd_Lighting_Preset_Commit_f(void) {
	const int preset = _sofbuddy_lighting_preset
		? static_cast<int>(_sofbuddy_lighting_preset->value + 0.5f)
		: LIGHTING_PRESET_VANILLA;

	if (!detour_Cbuf_AddText::oCbuf_AddText) {
		return;
	}

	if (preset == LIGHTING_PRESET_CUSTOM) {
		// Keep the menu open and reveal the curve/blend controls.
		detour_Cbuf_AddText::oCbuf_AddText("sofbuddy_close_menu;reloadall;sofbuddy_menu sof_buddy/lighting\n");
	} else {
		// Close the menu so the new preset is visible immediately.
		detour_Cbuf_AddText::oCbuf_AddText("popmenu\n");
	}
}

/*
	Commit the independent Overbright Lighting toggle. The list writes the cvar
	first (which applies the setting), then this rebuilds the page so the curve
	rows appear or disappear immediately.
*/
static void Cmd_Lighting_Overbright_Commit_f(void) {
	lighting_apply_effective_settings(true);
	lighting_refresh_ui_rmf();
	if (detour_Cbuf_AddText::oCbuf_AddText) {
		detour_Cbuf_AddText::oCbuf_AddText("sofbuddy_close_menu;reloadall;sofbuddy_menu sof_buddy/lighting\n");
	}
}

/*
	Live-hub version of the Overbright toggle commit.  The hub is already open
	over the game; rebuild the page so the Style/Pre-Curve rows swap.  A plain
	`refresh` only reinitialises widgets and would not re-evaluate cinclude.
*/
static void Cmd_Lighting_Overbright_Live_Commit_f(void) {
	lighting_apply_effective_settings(true);
	lighting_refresh_ui_rmf();
	if (detour_Cbuf_AddText::oCbuf_AddText) {
		detour_Cbuf_AddText::oCbuf_AddText("sofbuddy_close_menu;reloadall;sofbuddy_menu sof_buddy/lighting_live\n");
	}
}

/*
	Live-hub version of the normalization mode commit.  The Up/Down amount
	sliders are conditional includes, so rebuild the hub after switching mode.
*/
static void Cmd_Lighting_Normalize_Live_Commit_f(void) {
	lighting_apply_effective_settings(true);
	if (detour_Cbuf_AddText::oCbuf_AddText) {
		detour_Cbuf_AddText::oCbuf_AddText("sofbuddy_close_menu;reloadall;sofbuddy_menu sof_buddy/lighting_live\n");
	}
}

void lighting_register_commands(void) {
	if (!detour_Cmd_AddCommand::oCmd_AddCommand) {
		return;
	}
	detour_Cmd_AddCommand::oCmd_AddCommand(const_cast<char*>("sofbuddy_lighting_rebuild"), Cmd_Lighting_Rebuild_f);
	detour_Cmd_AddCommand::oCmd_AddCommand(const_cast<char*>("sofbuddy_lighting_status"), Cmd_Lighting_Status_f);
	detour_Cmd_AddCommand::oCmd_AddCommand(const_cast<char*>("sofbuddy_lighting_reset"), Cmd_Lighting_Reset_f);
	detour_Cmd_AddCommand::oCmd_AddCommand(const_cast<char*>("sofbuddy_lighting_curve_reset"), Cmd_Lighting_Curve_Reset_f);
	detour_Cmd_AddCommand::oCmd_AddCommand(const_cast<char*>("sofbuddy_lighting_overbright_curve_reset"), Cmd_Lighting_Overbright_Curve_Reset_f);
	detour_Cmd_AddCommand::oCmd_AddCommand(const_cast<char*>("sofbuddy_lighting_curve_template"), Cmd_Lighting_Curve_Template_f);
	detour_Cmd_AddCommand::oCmd_AddCommand(const_cast<char*>("sofbuddy_lighting_curve_templates"), Cmd_Lighting_Curve_Templates_f);
	detour_Cmd_AddCommand::oCmd_AddCommand(const_cast<char*>("sofbuddy_lighting_preset_commit"), Cmd_Lighting_Preset_Commit_f);
	detour_Cmd_AddCommand::oCmd_AddCommand(const_cast<char*>("sofbuddy_lighting_overbright_commit"), Cmd_Lighting_Overbright_Commit_f);
	detour_Cmd_AddCommand::oCmd_AddCommand(const_cast<char*>("sofbuddy_lighting_overbright_live_commit"), Cmd_Lighting_Overbright_Live_Commit_f);
	detour_Cmd_AddCommand::oCmd_AddCommand(const_cast<char*>("sofbuddy_lighting_normalize_live_commit"), Cmd_Lighting_Normalize_Live_Commit_f);
	detour_Cmd_AddCommand::oCmd_AddCommand(const_cast<char*>("sofbuddy_gl_modulate_confirm"), Cmd_Lighting_ModulateConfirm_f);
	detour_Cmd_AddCommand::oCmd_AddCommand(const_cast<char*>("sofbuddy_gl_modulate_cancel"), Cmd_Lighting_ModulateCancel_f);
}

/*
	CVar change callback for lightblend cvars

	Called when _sofbuddy_lightblend_src or _sofbuddy_lightblend_dst cvars are changed.
	This function is externally referenced in cvars.cpp.
*/
void lightblend_change(cvar_t * cvar) {
	if (s_syncing_cvars || !cvar) {
		return;
	}

	const bool is_src = !strcmp(cvar->name, "_sofbuddy_lightblend_src");
	const int value = parse_blend_value(cvar->string, is_src);
	if (is_src) {
		custom_lightblend_src = value;
	} else {
		custom_lightblend_dst = value;
	}

	// Console users and old aliases changing the manual blend without selecting
	// Custom should still get the old behaviour.
	if (lightblend_preset != LIGHTING_PRESET_CUSTOM &&
	    (custom_lightblend_src != GL_ZERO || custom_lightblend_dst != GL_SRC_COLOR)) {
		set_cvar_string(_sofbuddy_lighting_preset, "5");
	}

	PrintOut(PRINT_LOG, "lighting_blend: CVar changed: %s\n", cvar->name);
	lighting_apply_effective_settings(true);
}

bool is_blending = false;

// r_blendlightmaps_pre_callback and r_blendlightmaps_post_callback implementations moved to hooks/ directory


/*
  This only gets called when gl_ext_multitexture is 0.
*/
// src = Incoming pixel Data (Light Data)
// dest = Pixel Data in Color Buffer (Texture Data)
void __stdcall glBlendFunc_R_BlendLightmaps(unsigned int sfactor,unsigned int dfactor)
{
	if ( is_blending ) {
		real_glBlendFunc(lightblend_src,lightblend_dst);
	}
	else {
		real_glBlendFunc(sfactor,dfactor);
	}
}


static void __cdecl hkAdjustTexCoordsImpl(float* player_pos, float* vertex, float* in_normal, float* in_tangent, float* in_bitangent, float* out_new_s_t, void* caller_stack);

void __cdecl hkAdjustTexCoords(float* player_pos, float* vertex, float* in_normal, float* in_tangent, float* in_bitangent, float* out_new_s_t)
{
    void* esp_entry = __builtin_frame_address(0);
    hkAdjustTexCoordsImpl(player_pos, vertex, in_normal, in_tangent, in_bitangent, out_new_s_t, esp_entry);

}


static void __cdecl hkAdjustTexCoordsImpl(float* player_pos, float* vertex, float* in_normal, float* in_tangent, float* in_bitangent, float* out_new_s_t, void* caller_stack)
{
	unsigned int surf_index = *(unsigned int*)((char*)caller_stack + 0xD8 - 0xA4);

	// Hot path: ref_surfaces is refreshed on each ref.dll load (so it's safe across vid_restart).
	if (!ref_surfaces) {
		// Fail safe: behave like the original call site as best as we can.
		oAdjustTexCoords(player_pos, vertex, in_normal, in_tangent, in_bitangent, out_new_s_t);
		glTexCoord2f(out_new_s_t[0], out_new_s_t[1]);
		return;
	}

	void** surface_ptr = ref_surfaces + surf_index;
	void* cplane_ptr = (void*)((char*)(*surface_ptr) + 4);
	float* surface_normal = *(float**)cplane_ptr;
	float normal_x = surface_normal[0];
	float normal_y = surface_normal[1];
	float normal_z = surface_normal[2];

	void ** mtexinfo_ptr = (void**)((char*)(*surface_ptr) + 0x34);
	int flags = *(int*)((char*)(*mtexinfo_ptr) + 0x20);


	// Visualize normal direction once per surface using glVertex3f calls inside GL_TRIANGLE_FAN
	static unsigned int last_drawn_surf_index = 0xFFFFFFFF;
	static int vertex_count_per_surface = 0;
	if (surf_index != last_drawn_surf_index) {
		last_drawn_surf_index = surf_index;
		vertex_count_per_surface = 0;
	}
#if 0
	// Draw on every 3rd vertex to ensure visibility (not just first vertex which might be culled)
	if (vertex_count_per_surface % 3 == 0) {
		// Validate normal vector before drawing
		float normal_mag_sq = in_normal[0] * in_normal[0] + in_normal[1] * in_normal[1] + in_normal[2] * in_normal[2];
		bool is_zero = (normal_mag_sq < 1.0e-10f);
		bool has_nan = (in_normal[0] != in_normal[0] || in_normal[1] != in_normal[1] || in_normal[2] != in_normal[2]);
		bool has_inf = (normal_mag_sq > 1.0e10f);
		bool is_too_small = (normal_mag_sq > 0.0f && normal_mag_sq < 0.5f);
		bool is_too_large = (normal_mag_sq > 2.0f);

		if (is_zero || has_nan || has_inf || is_too_small || is_too_large) {
			PrintOut(PRINT_LOG, "DEBUG: Invalid normal detected for surface %u: in_normal = (%.6f, %.6f, %.6f), mag_sq = %.6f, zero=%d nan=%d inf=%d too_small=%d too_large=%d\n",
			         surf_index, in_normal[0], in_normal[1], in_normal[2], normal_mag_sq, is_zero, has_nan, has_inf, is_too_small, is_too_large);
		} else {
			const float normal_scale = 100.0f;
			float start_x = vertex[0];
			float start_y = vertex[1];
			float start_z = vertex[2];
			float end_x = start_x + in_normal[0] * normal_scale;
			float end_y = start_y + in_normal[1] * normal_scale;
			float end_z = start_z + in_normal[2] * normal_scale;

			// Draw multiple green vertices at the start to establish the color strongly
			glColor3f(0.0f, 1.0f, 0.0f);
			for (int j = 0; j < 5; j++) {
				glVertex3f(start_x, start_y, start_z);
			}

			// Draw multiple points along the normal with color interpolation from green to red
			const int num_points = 30;
			for (int i = 1; i <= num_points; i++) {
				float t = (float)i / (float)num_points;
				float r = t;  // Red increases from 0 to 1
				float g = 1.0f - t;  // Green decreases from 1 to 0
				glColor3f(r, g, 0.0f);
				glVertex3f(
					start_x + (end_x - start_x) * t,
					start_y + (end_y - start_y) * t,
					start_z + (end_z - start_z) * t
				);
			}

			// Draw arrowhead at the end (larger and red)
			const float arrowhead_size = 10.0f;
			glColor3f(1.0f, 0.0f, 0.0f);
			glVertex3f(end_x, end_y, end_z);
			glVertex3f(end_x - in_normal[0] * arrowhead_size + in_normal[1] * arrowhead_size * 0.3f,
			           end_y - in_normal[1] * arrowhead_size + in_normal[2] * arrowhead_size * 0.3f,
			           end_z - in_normal[2] * arrowhead_size - in_normal[0] * arrowhead_size * 0.3f);
			glVertex3f(end_x, end_y, end_z);
			glVertex3f(end_x - in_normal[0] * arrowhead_size - in_normal[1] * arrowhead_size * 0.3f,
			           end_y - in_normal[1] * arrowhead_size - in_normal[2] * arrowhead_size * 0.3f,
			           end_z - in_normal[2] * arrowhead_size + in_normal[0] * arrowhead_size * 0.3f);

			glColor3f(1.0f, 1.0f, 1.0f);
		}
	}
	#endif
	vertex_count_per_surface++;



	// Override input parameters with randomly selected basis
	// in_normal[0] = override_normal[0];
	// in_normal[1] = override_normal[1];
	// in_normal[2] = override_normal[2];

	// in_tangent[0] = override_tangent[0];
	// in_tangent[1] = override_tangent[1];
	// in_tangent[2] = override_tangent[2];

	// in_bitangent[0] = override_bitangent[0];
	// in_bitangent[1] = override_bitangent[1];
	// in_bitangent[2] = override_bitangent[2];
	float vertex_world[3];
	vertex_world[0] = vertex[0];
	vertex_world[1] = vertex[1];
	vertex_world[2] = vertex[2];
	if (flags & 0x00020000) {
		// Convert vertex from eye space to world space
		if (cam_vforward && cam_vup && cam_vright) {
			// PrintOut(PRINT_LOG, "DEBUG: cam_vforward = (%.6f, %.6f, %.6f)\n", cam_vforward[0], cam_vforward[1], cam_vforward[2]);
			// Quake-style eye space: z is forward-negative in view matrix
			vertex_world[0] = player_pos[0] + vertex[0] * cam_vright[0] + vertex[1] * cam_vup[0] - vertex[2] * cam_vforward[0];
			vertex_world[1] = player_pos[1] + vertex[0] * cam_vright[1] + vertex[1] * cam_vup[1] - vertex[2] * cam_vforward[1];
			vertex_world[2] = player_pos[2] + vertex[0] * cam_vright[2] + vertex[1] * cam_vup[2] - vertex[2] * cam_vforward[2];
		} else {
			vertex_world[0] = vertex[0];
			vertex_world[1] = vertex[1];
			vertex_world[2] = vertex[2];
		}

		// 	PrintOut(PRINT_LOG, "DEBUG: Env texture\n");
	// 	PrintOut(PRINT_LOG, "DEBUG: player_pos = (%.6f, %.6f, %.6f)\n", player_pos[0], player_pos[1], player_pos[2]);
	PrintOut(PRINT_LOG, "DEBUG: vertex = (%.6f, %.6f, %.6f)\n", vertex_world[0], vertex_world[1], vertex_world[2]);
	// PrintOut(PRINT_LOG, "DEBUG: in_normal = (%.6f, %.6f, %.6f)\n", in_normal[0], in_normal[1], in_normal[2]);
	// PrintOut(PRINT_LOG, "DEBUG: in_tangent = (%.6f, %.6f, %.6f)\n", in_tangent[0], in_tangent[1], in_tangent[2]);
	// PrintOut(PRINT_LOG, "DEBUG: in_bitangent = (%.6f, %.6f, %.6f)\n", in_bitangent[0], in_bitangent[1], in_bitangent[2]);

		#if 0
		// Env texture: Generate valid shiny texture coordinates
		// Calculate view vector from vertex to player
		float view_vec_x = player_pos[0] - vertex_world[0];
		float view_vec_y = player_pos[1] - vertex_world[1];
		float view_vec_z = player_pos[2] - vertex_world[2];

		// PrintOut(PRINT_LOG, "DEBUG: view_vec (before normalize) = (%.6f, %.6f, %.6f)\n", view_vec_x, view_vec_y, view_vec_z);

		// Calculate distance and normalize
		float distance = sqrtf(view_vec_x * view_vec_x + view_vec_y * view_vec_y + view_vec_z * view_vec_z);
		// PrintOut(PRINT_LOG, "DEBUG: distance = %.6f\n", distance);

		const float ZERO_VECTOR_EPSILON = 1.0e-10f;
		if (distance >= ZERO_VECTOR_EPSILON) {
			float inv_dist = 1.0f / distance;
			view_vec_x *= inv_dist;
			view_vec_y *= inv_dist;
			view_vec_z *= inv_dist;
		}

		// PrintOut(PRINT_LOG, "DEBUG: view_vec (after normalize) = (%.6f, %.6f, %.6f)\n", view_vec_x, view_vec_y, view_vec_z);

		// Project normalized view vector onto in_normal basis vector
		float dot_s = view_vec_x * in_normal[0] + view_vec_y * in_normal[1] + view_vec_z * in_normal[2];
		// PrintOut(PRINT_LOG, "DEBUG: dot_s = %.6f\n", dot_s);

		// Calculate rejection vector (perpendicular to in_normal)
		float rejection_x = view_vec_x - in_normal[0] * dot_s;
		float rejection_y = view_vec_y - in_normal[1] * dot_s;
		float rejection_z = view_vec_z - in_normal[2] * dot_s;

		// PrintOut(PRINT_LOG, "DEBUG: rejection = (%.6f, %.6f, %.6f)\n", rejection_x, rejection_y, rejection_z);

		// Project rejection onto in_tangent basis vector
		float dot_t = rejection_x * in_tangent[0] + rejection_y * in_tangent[1] + rejection_z * in_tangent[2];
		// PrintOut(PRINT_LOG, "DEBUG: dot_t = %.6f\n", dot_t);

		// Project rejection onto in_bitangent basis vector
		float dot_tex_s = rejection_x * in_bitangent[0] + rejection_y * in_bitangent[1] + rejection_z * in_bitangent[2];
		// PrintOut(PRINT_LOG, "DEBUG: dot_tex_s = %.6f\n", dot_tex_s);

		// Remap from [-1, 1] to [0, 1]
		const float ADD_CONSTANT = 1.0f;
		const float SCALE_CONSTANT = 0.5f;

		// PrintOut(PRINT_LOG, "before new_s_t is : %f, %f\n", out_new_s_t[0], out_new_s_t[1]);
		// out_new_s_t[0] = 1.0f - (dot_t + ADD_CONSTANT) * SCALE_CONSTANT;
		// out_new_s_t[1] = 1.0f - (dot_tex_s + ADD_CONSTANT) * SCALE_CONSTANT;
		out_new_s_t[0] = (dot_t + ADD_CONSTANT) * SCALE_CONSTANT;
		out_new_s_t[1] = (dot_tex_s + ADD_CONSTANT) * SCALE_CONSTANT;
		// PrintOut(PRINT_LOG, "after new_s_t is : %f, %f\n", out_new_s_t[0], out_new_s_t[1]);
		#endif
	}

	// Call original function with (potentially corrected) texture coordinates
	// PrintOut(PRINT_LOG, "DEBUG: before calling oAdjustTexCoords, out_new_s_t = (%.6f, %.6f)\n", out_new_s_t[0], out_new_s_t[1]);
	oAdjustTexCoords(player_pos, vertex_world, in_normal, in_tangent, in_bitangent, out_new_s_t);
	// PrintOut(PRINT_LOG, "DEBUG: after calling oAdjustTexCoords, out_new_s_t = (%.6f, %.6f)\n", out_new_s_t[0], out_new_s_t[1]);


    // Setup glTexCoord2f function pointer from dereference of 0xA42F4
    glTexCoord2f(out_new_s_t[0], out_new_s_t[1]);
	// glTexCoord2f(out_new_s_t[1], out_new_s_t[0] );

}

#endif // FEATURE_LIGHTING_BLEND
