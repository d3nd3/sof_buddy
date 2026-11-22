#pragma once

/*
	Lightmap tone curve.

	The renderer applies a 256-entry gamma table to lightmap texels before they
	are uploaded.  We replace that lookup for lightmaps only with a curve that
	has control points at 0, 10, 20 ... 100% of the lightmap input.  Each point
	is an output percentage in the same 0..100 range:

	    0   = black
	   50   = half brightness
	  100   = white

	Values are clamped at 100.  To get overbright, map lower inputs to 100:
	0,20,40,60,80,100,100,100,100,100,100 is 2x gain until white; after that
	the lightmap is already clipped.  This is the same as the old 0..400%
	control points, but matches the intuitive "input light -> output light"
	model and avoids showing values that can never survive the 8-bit clamp.

	The curve is applied to the lightmap before the texture is multiplied, so
	the lightmap saturates while texture detail survives.  Per-channel mode
	applies it to R/G/B separately.  The preserve modes apply it to a single
	brightness value and scale RGB together: either Rec.709 luminance
	(preserve brightness) or the brightest channel, HSV value (preserve
	colour).
*/

#include <cmath>

namespace lighting_curve_detail {

constexpr int kCurvePointCount = 11;

struct CurvePoints {
	float p[kCurvePointCount];
};

inline float clampf(float v, float lo, float hi) {
	return v < lo ? lo : (v > hi ? hi : v);
}

inline CurvePoints sanitize(CurvePoints points) {
	for (int i = 0; i < kCurvePointCount; ++i) {
		points.p[i] = clampf(points.p[i], 0.0f, 100.0f);
	}
	return points;
}

/*
	Evaluate the piecewise-linear curve at x in [0, 1].
	Returns a normalized value in [0, 1] (already clamped to white).
*/
inline float eval(CurvePoints points, float x) {
	points = sanitize(points);
	x = clampf(x, 0.0f, 1.0f);

	for (int i = 1; i < kCurvePointCount; ++i) {
		const float x1 = static_cast<float>(i) / static_cast<float>(kCurvePointCount - 1);
		const float x0 = static_cast<float>(i - 1) / static_cast<float>(kCurvePointCount - 1);
		if (x <= x1) {
			const float span = x1 - x0;
			const float t = span > 0.0f ? (x - x0) / span : 0.0f;
			const float y = points.p[i - 1] + (points.p[i] - points.p[i - 1]) * t;
			return clampf(y, 0.0f, 100.0f) * 0.01f;
		}
	}
	return clampf(points.p[kCurvePointCount - 1], 0.0f, 100.0f) * 0.01f;
}

/*
	Apply the curve to one byte of an already gamma-corrected lightmap value.
*/
inline unsigned char apply_byte(unsigned char base, CurvePoints points) {
	const float y = eval(points, static_cast<float>(base) * (1.0f / 255.0f));
	int v = static_cast<int>(std::lround(y * 255.0f));
	if (v < 0) v = 0;
	if (v > 255) v = 255;
	return static_cast<unsigned char>(v);
}

/*
	Unbiased variant for the Overbright pre-curve.  The 2x combine doubles the
	stored lightmap value, so rounding a half level up would make every odd
	texel one level brighter than vanilla.  Round to nearest-even instead, so
	odd values alternate above/below the original and the average stays put.
*/
inline unsigned char apply_byte_nearest_even(unsigned char base, CurvePoints points) {
	const float y = eval(points, static_cast<float>(base) * (1.0f / 255.0f));
	int v = static_cast<int>(std::nearbyint(y * 255.0f));
	if (v < 0) v = 0;
	if (v > 255) v = 255;
	return static_cast<unsigned char>(v);
}

/*
	Map lighting normalization.

	Some maps never use the full 8-bit lightmap range: their brightest sample
	may be, say, 196/255.  Normalizing remaps the gamma-corrected lightmap
	value before the tone curve is applied:

	    Up   (1): move max toward 255, scaling the range.  The fixed helper
	              uses 255 and a 4x cap; the live UI uses an amount percentage.
	    Down (2): move min toward 0 while max keeps its original value, so
	              values above the map maximum (dynamic lights) keep their
	              headroom.  The live UI controls how far min moves.
	    Both (3): apply Down then Up.  100%/100% is a full black/white stretch.

	The references are gamma-corrected lightmap values, because the tone curve
	operates on the gamma table's output.  The `*_amount` helpers below are the
	percentage-based path used by the live UI; the older fixed helpers are kept
	for compatibility and tests.
*/
constexpr int kNormalizationModeOff = 0;
constexpr int kNormalizationModeUp = 1;
constexpr int kNormalizationModeDown = 2;
constexpr int kNormalizationModeBoth = 3;

constexpr int kNormalizationMinReference = 64;

inline int sanitize_normalization_mode(int mode) {
	if (mode < kNormalizationModeOff) {
		return kNormalizationModeOff;
	}
	if (mode > kNormalizationModeBoth) {
		return kNormalizationModeBoth;
	}
	return mode;
}

inline bool normalization_mode_uses_min(int mode) {
	mode = sanitize_normalization_mode(mode);
	return mode == kNormalizationModeDown || mode == kNormalizationModeBoth;
}

inline int effective_normalize_reference(int observed_max) {
	if (observed_max <= 0) {
		return 0;
	}
	if (observed_max > 255) {
		observed_max = 255;
	}
	return observed_max < kNormalizationMinReference ? kNormalizationMinReference : observed_max;
}

inline unsigned char normalize_light_index(unsigned char value, int observed_max) {
	const int reference = effective_normalize_reference(observed_max);
	if (reference <= 0 || reference >= 255) {
		return value;
	}

	int scaled = (static_cast<int>(value) * 255 + reference / 2) / reference;
	if (scaled > 255) {
		scaled = 255;
	}
	return static_cast<unsigned char>(scaled);
}

/*
	Mode-aware normalization for one gamma-corrected value.

	Down keeps the original maximum and only stretches the values between the
	minimum and maximum towards black; values above the maximum are returned
	unchanged so dynamic lights still stand out.  Both stretches the full
	minimum..maximum range to 0..255 and clamps anything above the maximum.
*/
inline unsigned char normalize_light_index_mode(unsigned char value, int mode,
                                                int min_reference, int max_reference) {
	mode = sanitize_normalization_mode(mode);
	if (mode == kNormalizationModeOff) {
		return value;
	}
	if (mode == kNormalizationModeUp) {
		return normalize_light_index(value, max_reference);
	}

	if (min_reference < 0) {
		min_reference = 0;
	}
	if (max_reference > 255) {
		max_reference = 255;
	}
	if (max_reference <= min_reference) {
		return value;
	}

	const int v = static_cast<int>(value);
	const int span = max_reference - min_reference;
	if (v <= min_reference) {
		return 0;
	}

	if (mode == kNormalizationModeDown) {
		if (v > max_reference) {
			return value;
		}
		int scaled = ((v - min_reference) * max_reference + span / 2) / span;
		if (scaled < 0) scaled = 0;
		if (scaled > 255) scaled = 255;
		return static_cast<unsigned char>(scaled);
	}

	// Both.
	if (v >= max_reference) {
		return 255;
	}
	int scaled = ((v - min_reference) * 255 + span / 2) / span;
	if (scaled < 0) scaled = 0;
	if (scaled > 255) scaled = 255;
	return static_cast<unsigned char>(scaled);
}

/*
	Build the 256-entry normalization LUT that all light paths share.  It is
	indexed by a gamma-corrected byte and composed before the tone curve.
*/
inline void build_normalization_lut(unsigned char* lut, int mode,
                                    int min_reference, int max_reference) {
	if (!lut) {
		return;
	}
	for (int i = 0; i < 256; ++i) {
		lut[i] = normalize_light_index_mode(static_cast<unsigned char>(i), mode,
		                                    min_reference, max_reference);
	}
}

/*
	Percentage-based normalization (the live UI).

	Each mode has an amount in 0..100:

	  Up   - target_white = max + pct * (255 - max).  0% leaves the maximum,
	         100% maps it to 255.  The whole range is scaled, so black stays
	         black and values above max keep their relative headroom.
	  Down - target_black = min - pct * min.  0% leaves the minimum, 100%
	         maps it to 0; the maximum stays fixed.  Values above max are left
	         untouched so dynamic lights still stand out.
	  Both - Down is applied first, then Up.  At 100/100 this is the same full
	         min..max -> 0..255 stretch as the fixed Both mode.

	The target references are gamma-corrected lightmap values, because the
	normalization LUT runs before the tone curve.
*/
inline int sanitize_normalize_percent(int percent) {
	if (percent < 0) {
		return 0;
	}
	if (percent > 100) {
		return 100;
	}
	return percent;
}

inline int normalize_target_min(int min_reference, int down_percent) {
	if (min_reference <= 0) {
		return 0;
	}
	if (min_reference > 255) {
		min_reference = 255;
	}
	down_percent = sanitize_normalize_percent(down_percent);
	const int remove = (min_reference * down_percent + 50) / 100;
	int target = min_reference - remove;
	if (target < 0) {
		target = 0;
	}
	return target;
}

inline int normalize_target_max(int max_reference, int up_percent) {
	if (max_reference <= 0) {
		return 0;
	}
	if (max_reference > 255) {
		max_reference = 255;
	}
	up_percent = sanitize_normalize_percent(up_percent);
	const int add = ((255 - max_reference) * up_percent + 50) / 100;
	int target = max_reference + add;
	if (target > 255) {
		target = 255;
	}
	return target;
}

inline unsigned char normalize_light_index_up_amount(unsigned char value, int max_reference,
                                                     int up_percent) {
	if (max_reference <= 0 || max_reference >= 255) {
		return value;
	}
	if (sanitize_normalize_percent(up_percent) <= 0) {
		return value;
	}
	const int target = normalize_target_max(max_reference, up_percent);
	if (target <= max_reference) {
		return value;
	}
	int scaled = (static_cast<int>(value) * target + max_reference / 2) / max_reference;
	if (scaled > 255) {
		scaled = 255;
	}
	return static_cast<unsigned char>(scaled);
}

inline unsigned char normalize_light_index_down_amount(unsigned char value, int min_reference,
                                                       int max_reference, int down_percent) {
	if (min_reference < 0) {
		min_reference = 0;
	}
	if (max_reference > 255) {
		max_reference = 255;
	}
	if (max_reference <= min_reference) {
		return value;
	}
	if (sanitize_normalize_percent(down_percent) <= 0) {
		return value;
	}
	const int black = normalize_target_min(min_reference, down_percent);
	if (black >= min_reference) {
		return value;
	}

	const int v = static_cast<int>(value);
	if (v <= 0) {
		return 0;
	}
	if (v < min_reference) {
		int out = (v * black + min_reference / 2) / min_reference;
		if (out < 0) out = 0;
		if (out > 255) out = 255;
		return static_cast<unsigned char>(out);
	}
	if (v > max_reference) {
		return value;
	}

	const int span = max_reference - min_reference;
	int out = black +
	          ((v - min_reference) * (max_reference - black) + span / 2) / span;
	if (out < 0) out = 0;
	if (out > 255) out = 255;
	return static_cast<unsigned char>(out);
}

inline unsigned char normalize_light_index_both_amount(unsigned char value, int min_reference,
                                                       int max_reference, int up_percent,
                                                       int down_percent) {
	const unsigned char down =
		normalize_light_index_down_amount(value, min_reference, max_reference, down_percent);
	return normalize_light_index_up_amount(down, max_reference, up_percent);
}

inline unsigned char normalize_light_index_mode_amount(unsigned char value, int mode,
                                                       int min_reference, int max_reference,
                                                       int up_percent, int down_percent) {
	mode = sanitize_normalization_mode(mode);
	switch (mode) {
		case kNormalizationModeUp:
			return normalize_light_index_up_amount(value, max_reference, up_percent);
		case kNormalizationModeDown:
			return normalize_light_index_down_amount(value, min_reference, max_reference,
			                                         down_percent);
		case kNormalizationModeBoth:
			return normalize_light_index_both_amount(value, min_reference, max_reference,
			                                         up_percent, down_percent);
		default:
			return value;
	}
}

inline void build_normalization_lut_amount(unsigned char* lut, int mode,
                                           int min_reference, int max_reference,
                                           int up_percent, int down_percent) {
	if (!lut) {
		return;
	}
	for (int i = 0; i < 256; ++i) {
		lut[i] = normalize_light_index_mode_amount(static_cast<unsigned char>(i), mode,
		                                           min_reference, max_reference,
		                                           up_percent, down_percent);
	}
}

inline float normalize_gain_amount(int mode, int min_reference, int max_reference,
                                   int up_percent, int down_percent) {
	mode = sanitize_normalization_mode(mode);
	if (mode == kNormalizationModeOff || max_reference <= 0) {
		return 1.0f;
	}
	if (min_reference < 0) {
		min_reference = 0;
	}
	if (max_reference > 255) {
		max_reference = 255;
	}
	const int black = (mode == kNormalizationModeDown || mode == kNormalizationModeBoth)
	                      ? normalize_target_min(min_reference, down_percent)
	                      : min_reference;
	const int white = (mode == kNormalizationModeUp || mode == kNormalizationModeBoth)
	                      ? normalize_target_max(max_reference, up_percent)
	                      : max_reference;
	if (mode == kNormalizationModeUp) {
		return max_reference > 0
		           ? static_cast<float>(white) / static_cast<float>(max_reference)
		           : 1.0f;
	}
	const int span = max_reference - min_reference;
	if (span <= 0) {
		return 1.0f;
	}
	const float up_gain = max_reference > 0
	                          ? static_cast<float>(white) / static_cast<float>(max_reference)
	                          : 1.0f;
	const float final_black = static_cast<float>(black) * up_gain;
	return (static_cast<float>(white) - final_black) / static_cast<float>(span);
}

/*
	Convenience presets.  Keep in sync with lighting_preset_e.
*/
inline CurvePoints identity() {
	CurvePoints p;
	for (int i = 0; i < kCurvePointCount; ++i) {
		p.p[i] = static_cast<float>(i * 10);
	}
	return p;
}

inline CurvePoints make_curve(const float (&values)[kCurvePointCount]) {
	CurvePoints p;
	for (int i = 0; i < kCurvePointCount; ++i) {
		p.p[i] = values[i];
	}
	return p;
}

/*
	Overbright-only pre-curve profiles.  These are deliberately separate from
	the Lighting Style curve presets: the combination of the Overbright 2x
	product and these profiles produces a different result from applying the
	same curve to the vanilla lightmap.

	The product is `2 * T * C(L)`.  A point preserves vanilla only when
	`C(L) = L / 2`; a point at identity doubles that tone.  Late highlights is
	the default: it stays at half through 70% input, then lifts 80-100%, so
	only the top of the map's range pops.  Popping highlights leaves the half
	curve at 60% and lifts harder above that.  The thresholds are in
	gamma-corrected lightmap space, not linear light.

	"2x everything" is the old linear identity, so selecting it reproduces the
	original stock 2 x T x L product.
*/
constexpr int kOverbrightProfilePoppingHighlights = 0;
constexpr int kOverbrightProfilePoppingMidHighlight = 1;
constexpr int kOverbrightProfilePreserveDarks = 2;
constexpr int kOverbrightProfile2xEverything = 3;
// Custom stays at 4 so archived configs keep their previous meaning.  The new
// profile is appended at 5 and the UI maps its label before Custom.
constexpr int kOverbrightProfileCustom = 4;
constexpr int kOverbrightProfileLateHighlights = 5;

inline CurvePoints overbright_profile_points(int profile) {
	static const float popping_highlights[kCurvePointCount] = {
		0, 5, 10, 15, 20, 25, 30, 45, 60, 80, 100
	};
	static const float popping_mid_highlight[kCurvePointCount] = {
		0, 5, 10, 15, 20, 25, 40, 55, 70, 85, 100
	};
	static const float preserve_darks[kCurvePointCount] = {
		0, 5, 10, 15, 23, 33, 45, 59, 75, 93, 100
	};
	static const float everything_2x[kCurvePointCount] = {
		0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100
	};
	static const float late_highlights[kCurvePointCount] = {
		0, 5, 10, 15, 20, 25, 30, 35, 45, 65, 100
	};
	switch (profile) {
		case kOverbrightProfilePoppingHighlights: return make_curve(popping_highlights);
		case kOverbrightProfilePoppingMidHighlight: return make_curve(popping_mid_highlight);
		case kOverbrightProfilePreserveDarks: return make_curve(preserve_darks);
		case kOverbrightProfile2xEverything: return make_curve(everything_2x);
		default: return make_curve(late_highlights);
	}
}

inline const char* overbright_profile_name(int profile) {
	switch (profile) {
		case kOverbrightProfilePoppingHighlights: return "Popping highlights";
		case kOverbrightProfilePoppingMidHighlight: return "Popping mid+highlight";
		case kOverbrightProfilePreserveDarks: return "Preserve Darks only";
		case kOverbrightProfile2xEverything: return "2x everything";
		case kOverbrightProfileCustom: return "Custom";
		default: return "Late highlights";
	}
}

inline const char* overbright_profile_summary(int profile) {
	switch (profile) {
		case kOverbrightProfilePoppingHighlights:
			return "Stronger pop above 60% input.";
		case kOverbrightProfilePoppingMidHighlight:
			return "Brighter midtones and highlights than Popping highlights.";
		case kOverbrightProfilePreserveDarks:
			return "Keeps shadows dark and lifts mostly midtones and highlights.";
		case kOverbrightProfile2xEverything:
			return "Linear identity; the original stock 2 x T x L overbright look.";
		case kOverbrightProfileCustom:
			return "Your edited Overbright Pre-Curve.";
		default:
			return "Default: vanilla through 70% input, then only the top range pops.";
	}
}

inline bool curve_points_equal(CurvePoints a, CurvePoints b) {
	for (int i = 0; i < kCurvePointCount; ++i) {
		if (std::fabs(a.p[i] - b.p[i]) > 0.001f) {
			return false;
		}
	}
	return true;
}

inline int overbright_profile_for_points(CurvePoints points) {
	for (int profile = kOverbrightProfilePoppingHighlights;
	     profile <= kOverbrightProfile2xEverything; ++profile) {
		if (curve_points_equal(points, overbright_profile_points(profile))) {
			return profile;
		}
	}
	if (curve_points_equal(points, overbright_profile_points(kOverbrightProfileLateHighlights))) {
		return kOverbrightProfileLateHighlights;
	}
	return kOverbrightProfileCustom;
}

inline CurvePoints overbright2x() {
	static const float values[kCurvePointCount] = {
		0, 20, 40, 60, 80, 100, 100, 100, 100, 100, 100
	};
	return make_curve(values);
}

inline CurvePoints overbright4x() {
	static const float values[kCurvePointCount] = {
		0, 40, 80, 100, 100, 100, 100, 100, 100, 100, 100
	};
	return make_curve(values);
}

inline CurvePoints soft_glow() {
	static const float values[kCurvePointCount] = {
		5, 17, 29, 42, 56, 70, 82, 94, 100, 100, 100
	};
	return make_curve(values);
}

/*
	Luminance-preserving variant.  The inline R_BuildLightMap LUT is left at
	the original gamma table in this mode, so the RGB values here are already
	gamma-corrected.  Apply the curve to their luminance and scale all three
	channels by the same factor.  This preserves hue and saturation while no
	channel clips, but a saturated pixel whose luma target needs more than 1.0
	in its brightest channel still clips at the top.

	When normalize_lut is non-null, the luminance used for the curve lookup is
	first mapped through the shared normalization LUT (Up/Down/Both).  RGB is
	still scaled by one factor, so map normalization does not break the channel
	ratios.
*/
inline void preserve_luminance_byte(unsigned char& r, unsigned char& g, unsigned char& b,
                                    const unsigned char* curve_lut,
                                    const unsigned char* normalize_lut = nullptr) {
	if (!curve_lut) {
		return;
	}

	// Rec.709 luma with integer weights summing to 256.
	const int luma = (static_cast<int>(r) * 54 +
	                  static_cast<int>(g) * 183 +
	                  static_cast<int>(b) * 19 + 128) >> 8;
	const int curve_input = normalize_lut ? normalize_lut[luma] : luma;
	const int target = curve_lut[curve_input];

	if (luma == 0) {
		// A lifted black point becomes neutral grey; there is no hue to keep.
		r = g = b = static_cast<unsigned char>(target);
		return;
	}
	if (target == luma) {
		return;
	}

	const int half = luma / 2;
	int rr = (static_cast<int>(r) * target + half) / luma;
	int gg = (static_cast<int>(g) * target + half) / luma;
	int bb = (static_cast<int>(b) * target + half) / luma;

	if (rr > 255) rr = 255;
	if (gg > 255) gg = 255;
	if (bb > 255) bb = 255;
	if (rr < 0) rr = 0;
	if (gg < 0) gg = 0;
	if (bb < 0) bb = 0;

	r = static_cast<unsigned char>(rr);
	g = static_cast<unsigned char>(gg);
	b = static_cast<unsigned char>(bb);
}

/*
	Strict preserve-colour variant.  Index the curve by the brightest channel
	(HSV value) and scale all three channels by the same factor.  Because no
	channel is larger than the value used for the lookup, the result can never
	exceed 255: hue and saturation ratios are preserved exactly and there is
	no clipping/whitening.  The trade-off is that a colour already near white
	has no headroom and cannot be brightened further by the curve.

	When normalize_lut is non-null, the value used for the curve lookup is
	first mapped through the shared normalization LUT (Up/Down/Both), while RGB
	is scaled by one factor.
*/
inline void preserve_value_byte(unsigned char& r, unsigned char& g, unsigned char& b,
                                const unsigned char* curve_lut,
                                const unsigned char* normalize_lut = nullptr) {
	if (!curve_lut) {
		return;
	}

	int value = r > g ? r : g;
	if (b > value) {
		value = b;
	}
	const int curve_input = normalize_lut ? normalize_lut[value] : value;
	const int target = curve_lut[curve_input];

	if (value == 0) {
		// A lifted black point becomes neutral grey; there is no hue to keep.
		r = g = b = static_cast<unsigned char>(target);
		return;
	}
	if (target == value) {
		return;
	}

	const int half = value / 2;
	// Every channel is <= value, so these results are <= target <= 255.
	const int rr = (static_cast<int>(r) * target + half) / value;
	const int gg = (static_cast<int>(g) * target + half) / value;
	const int bb = (static_cast<int>(b) * target + half) / value;

	r = static_cast<unsigned char>(rr);
	g = static_cast<unsigned char>(gg);
	b = static_cast<unsigned char>(bb);
}

} // namespace lighting_curve_detail
