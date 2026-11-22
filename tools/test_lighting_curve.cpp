#include "../src/features/lighting_blend/lighting_curve.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <algorithm>

// This test uses assert(), so an NDEBUG build would silently skip every check
// and always "pass".  Fail the build instead.
#ifdef NDEBUG
#error "test_lighting_curve must be built without NDEBUG"
#endif

static void assert_near(float actual, float expected, float epsilon = 0.001f) {
	if (std::fabs(actual - expected) > epsilon) {
		std::printf("expected %.4f, got %.4f\n", expected, actual);
		assert(false);
	}
}

int main() {
	using namespace lighting_curve_detail;

	const CurvePoints identity_curve = lighting_curve_detail::identity();
	assert_near(eval(identity_curve, 0.0f), 0.0f);
	assert_near(eval(identity_curve, 0.25f), 0.25f);
	assert_near(eval(identity_curve, 0.5f), 0.5f);
	assert_near(eval(identity_curve, 0.75f), 0.75f);
	assert_near(eval(identity_curve, 1.0f), 1.0f);
	assert(apply_byte(0, identity_curve) == 0);
	assert(apply_byte(128, identity_curve) == 128);
	assert(apply_byte(255, identity_curve) == 255);

	const CurvePoints ob2 = lighting_curve_detail::overbright2x();
	assert_near(eval(ob2, 0.0f), 0.0f);
	assert_near(eval(ob2, 0.25f), 0.5f);
	assert_near(eval(ob2, 0.5f), 1.0f);
	assert_near(eval(ob2, 0.75f), 1.0f);
	assert_near(eval(ob2, 1.0f), 1.0f);
	assert(apply_byte(64, ob2) == 128);
	assert(apply_byte(128, ob2) == 255);
	assert(apply_byte(255, ob2) == 255);

	const CurvePoints ob4 = lighting_curve_detail::overbright4x();
	assert_near(eval(ob4, 0.10f), 0.4f);
	assert_near(eval(ob4, 0.20f), 0.8f);
	assert_near(eval(ob4, 0.30f), 1.0f);
	assert(apply_byte(26, ob4) >= 100); // ~10% -> ~40%
	assert(apply_byte(77, ob4) == 255); // 30% -> white

	const CurvePoints soft = lighting_curve_detail::soft_glow();
	assert(apply_byte(0, soft) > 0);
	assert(apply_byte(255, soft) == 255);
	assert_near(eval(soft, 1.0f), 1.0f);

	// Example from the UI: input 30% -> output 60%, i.e. 2x at that point.
	CurvePoints custom = lighting_curve_detail::identity();
	custom.p[3] = 60.0f;
	assert_near(eval(custom, 0.30f), 0.60f);
	assert(apply_byte(0, custom) == 0);
	assert(apply_byte(77, custom) > 77); // 30% input -> 60% output

	CurvePoints clamped = lighting_curve_detail::identity();
	clamped.p[0] = -10.0f;
	clamped.p[kCurvePointCount - 1] = 999.0f;
	clamped = sanitize(clamped);
	assert_near(clamped.p[0], 0.0f);
	assert_near(clamped.p[kCurvePointCount - 1], 100.0f);

	// Overbright-only pre-curve profiles are separate from Lighting Style.
	assert(overbright_profile_points(kOverbrightProfilePoppingHighlights).p[1] == 5);
	assert(overbright_profile_points(kOverbrightProfilePoppingHighlights).p[9] == 80);
	assert(overbright_profile_points(kOverbrightProfilePoppingMidHighlight).p[6] == 40);
	assert(overbright_profile_points(kOverbrightProfilePreserveDarks).p[8] == 75);
	assert(overbright_profile_points(kOverbrightProfile2xEverything).p[5] == 50);
	assert(overbright_profile_points(kOverbrightProfileLateHighlights).p[8] == 45);
	assert(overbright_profile_for_points(
		overbright_profile_points(kOverbrightProfilePreserveDarks)) == kOverbrightProfilePreserveDarks);
	assert(overbright_profile_for_points(
		overbright_profile_points(kOverbrightProfileLateHighlights)) == kOverbrightProfileLateHighlights);
	assert(overbright_profile_for_points(identity()) == kOverbrightProfile2xEverything);
	CurvePoints custom_profile = overbright_profile_points(kOverbrightProfilePoppingHighlights);
	custom_profile.p[5] = 99.0f;
	assert(overbright_profile_for_points(custom_profile) == kOverbrightProfileCustom);

	// The Overbright product is 2 * T * C(L), so C(L) must be L/2 to match
	// vanilla.  Popping highlights does that through 60% input; above that it
	// deliberately lifts.  Keep this invariant when tuning the profile.
	const CurvePoints popping =
		overbright_profile_points(kOverbrightProfilePoppingHighlights);
	for (int i = 0; i <= 153; ++i) {
		const int doubled = 2 * apply_byte(static_cast<unsigned char>(i), popping);
		assert(doubled >= i - 1 && doubled <= i + 1);
	}
	for (int i = 154; i < 256; ++i) {
		int doubled = 2 * apply_byte(static_cast<unsigned char>(i), popping);
		if (doubled > 255) doubled = 255;
		assert(doubled >= i - 1);
	}

	// The combined path uses nearest-even for the overbright pre-curve.  The
	// old lround variant biased the half curve up by one level on odd texels
	// (sum +33 over 0..153); nearest-even keeps the average centered.
	int rounding_bias = 0;
	for (int i = 0; i <= 153; ++i) {
		rounding_bias += 2 * apply_byte_nearest_even(static_cast<unsigned char>(i), popping) - i;
	}
	assert(rounding_bias >= -4 && rounding_bias <= 4);
	assert(apply_byte(17, popping) == 9);
	assert(apply_byte_nearest_even(17, popping) == 8);

	// Late highlights is the default: half through 70% input, then a top-end
	// lift.  It must preserve the low range just like Popping.
	const CurvePoints late =
		overbright_profile_points(kOverbrightProfileLateHighlights);
	int late_bias = 0;
	for (int i = 0; i <= 178; ++i) {
		const int doubled =
			2 * apply_byte_nearest_even(static_cast<unsigned char>(i), late);
		assert(doubled >= i - 1 && doubled <= i + 1);
		late_bias += doubled - i;
	}
	assert(late_bias >= -10 && late_bias <= 10);
	for (int i = 180; i < 256; ++i) {
		int doubled = 2 * apply_byte_nearest_even(static_cast<unsigned char>(i), late);
		if (doubled > 255) doubled = 255;
		assert(doubled >= i - 1);
	}

	// A curve that returns to identity above 50% is not a half curve: the x2
	// combine doubles those values instead of restoring vanilla.
	static const float jump_values[kCurvePointCount] = {
		0, 5, 10, 15, 20, 25, 60, 70, 80, 90, 100
	};
	const CurvePoints jump = make_curve(jump_values);
	assert(2 * apply_byte(153, jump) > 255);
	assert(2 * apply_byte(178, jump) > 255);

	// Map lighting normalization.
	assert(effective_normalize_reference(0) == 0);
	assert(effective_normalize_reference(32) == 64);
	assert(effective_normalize_reference(196) == 196);
	assert(effective_normalize_reference(300) == 255);
	assert(normalize_light_index(128, 0) == 128);
	assert(normalize_light_index(128, 255) == 128);
	assert(normalize_light_index(196, 196) == 255);
	assert(normalize_light_index(98, 196) == 128);
	assert(normalize_light_index(128, 196) == 167);
	assert(normalize_light_index(32, 32) == 128); // 4x gain cap
	assert(normalize_light_index(64, 32) == 255);
	assert(apply_byte(normalize_light_index(196, 196), lighting_curve_detail::identity()) == 255);
	assert(apply_byte(normalize_light_index(98, 196), lighting_curve_detail::identity()) == 128);

	// Mode-aware normalization.
	assert(sanitize_normalization_mode(-1) == kNormalizationModeOff);
	assert(sanitize_normalization_mode(99) == kNormalizationModeBoth);
	assert(normalize_light_index_mode(98, kNormalizationModeOff, 64, 196) == 98);

	// Up: max -> 255, min is ignored.
	assert(normalize_light_index_mode(196, kNormalizationModeUp, 64, 196) == 255);
	assert(normalize_light_index_mode(98, kNormalizationModeUp, 64, 196) == 128);

	// Down: min -> 0, the original max stays put; above max is untouched.
	assert(normalize_light_index_mode(64, kNormalizationModeDown, 64, 196) == 0);
	assert(normalize_light_index_mode(32, kNormalizationModeDown, 64, 196) == 0);
	assert(normalize_light_index_mode(130, kNormalizationModeDown, 64, 196) == 98);
	assert(normalize_light_index_mode(196, kNormalizationModeDown, 64, 196) == 196);
	assert(normalize_light_index_mode(240, kNormalizationModeDown, 64, 196) == 240);

	// Absolute minimum 0 makes Down a no-op, as requested.
	for (int i = 0; i < 256; ++i) {
		assert(normalize_light_index_mode(static_cast<unsigned char>(i),
		                                  kNormalizationModeDown, 0, 196) == i);
	}

	// Both: min -> 0 and max -> 255; above max clamps to white.
	assert(normalize_light_index_mode(64, kNormalizationModeBoth, 64, 196) == 0);
	assert(normalize_light_index_mode(32, kNormalizationModeBoth, 64, 196) == 0);
	assert(normalize_light_index_mode(130, kNormalizationModeBoth, 64, 196) == 128);
	assert(normalize_light_index_mode(196, kNormalizationModeBoth, 64, 196) == 255);
	assert(normalize_light_index_mode(240, kNormalizationModeBoth, 64, 196) == 255);
	assert(normalize_light_index_mode(98, kNormalizationModeBoth, 0, 196) == 128);

	// Flat ranges and Off leave bytes untouched.
	assert(normalize_light_index_mode(100, kNormalizationModeBoth, 100, 100) == 100);

	unsigned char normalize_down_lut[256];
	unsigned char normalize_both_lut[256];
	build_normalization_lut(normalize_down_lut, kNormalizationModeDown, 64, 196);
	build_normalization_lut(normalize_both_lut, kNormalizationModeBoth, 64, 196);
	assert(normalize_down_lut[64] == 0);
	assert(normalize_down_lut[130] == 98);
	assert(normalize_down_lut[196] == 196);
	assert(normalize_both_lut[130] == 128);
	assert(normalize_both_lut[196] == 255);

	// Percentage-based targets.
	assert(normalize_target_max(196, 0) == 196);
	assert(normalize_target_max(196, 50) == 226);
	assert(normalize_target_max(196, 100) == 255);
	assert(normalize_target_max(0, 100) == 0);
	assert(normalize_target_min(64, 0) == 64);
	assert(normalize_target_min(64, 50) == 32);
	assert(normalize_target_min(64, 100) == 0);

	// Amount-aware Up: a pure scale from black, no fixed 4x cap.
	assert(normalize_light_index_up_amount(196, 196, 0) == 196);
	assert(normalize_light_index_up_amount(196, 196, 100) == 255);
	assert(normalize_light_index_up_amount(98, 196, 100) == 128);
	assert(normalize_light_index_up_amount(0, 196, 100) == 0);

	// Amount-aware Down: move the black point, keep max, leave above-max alone.
	assert(normalize_light_index_down_amount(64, 64, 196, 0) == 64);
	assert(normalize_light_index_down_amount(64, 64, 196, 50) == 32);
	assert(normalize_light_index_down_amount(64, 64, 196, 100) == 0);
	assert(normalize_light_index_down_amount(130, 64, 196, 50) == 114);
	assert(normalize_light_index_down_amount(130, 64, 196, 100) == 98);
	assert(normalize_light_index_down_amount(196, 64, 196, 100) == 196);
	assert(normalize_light_index_down_amount(240, 64, 196, 100) == 240);

	// Both composes Down then Up.
	assert(normalize_light_index_both_amount(64, 64, 196, 100, 100) == 0);
	assert(normalize_light_index_both_amount(130, 64, 196, 100, 100) == 128);
	assert(normalize_light_index_both_amount(196, 64, 196, 100, 100) == 255);
	assert(normalize_light_index_both_amount(64, 64, 196, 0, 0) == 64);
	assert(normalize_light_index_both_amount(196, 64, 196, 0, 0) == 196);
	assert(normalize_light_index_both_amount(130, 64, 196, 50, 50) == 131);

	unsigned char amount_lut[256];
	build_normalization_lut_amount(amount_lut, kNormalizationModeBoth, 64, 196, 50, 50);
	assert(amount_lut[64] == 37);
	assert(amount_lut[130] == 131);
	assert(amount_lut[196] == 226);
	assert_near(normalize_gain_amount(kNormalizationModeUp, 64, 196, 100, 100),
	            255.0f / 196.0f);
	assert_near(normalize_gain_amount(kNormalizationModeDown, 64, 196, 100, 100),
	            196.0f / 132.0f);
	assert_near(normalize_gain_amount(kNormalizationModeBoth, 64, 196, 0, 0), 1.0f);

	// Luminance-preserving mode (Preserve brightness).
	unsigned char identity_lut[256];
	unsigned char overbright2_lut[256];
	for (int i = 0; i < 256; ++i) {
		identity_lut[i] = static_cast<unsigned char>(i);
		overbright2_lut[i] = apply_byte(static_cast<unsigned char>(i), ob2);
	}

	unsigned char r = 128, g = 128, b = 128;
	preserve_luminance_byte(r, g, b, identity_lut);
	assert(r == 128 && g == 128 && b == 128);

	r = 128; g = 128; b = 128;
	preserve_luminance_byte(r, g, b, overbright2_lut);
	assert(r == 255 && g == 255 && b == 255);

	// Luma scaling preserves ratios until the brightest channel clips.
	r = 200; g = 100; b = 50;
	preserve_luminance_byte(r, g, b, overbright2_lut);
	assert(r == 255);
	assert(g == 200);
	assert(b == 100);

	// A lifted black point becomes neutral grey.
	unsigned char lift_lut[256];
	for (int i = 0; i < 256; ++i) lift_lut[i] = static_cast<unsigned char>(std::min(25 + i, 255));
	r = 0; g = 0; b = 0;
	preserve_luminance_byte(r, g, b, lift_lut);
	assert(r == 25 && g == 25 && b == 25);

	// Strict preserve-colour mode (curve on HSV value, no clipping).
	r = 128; g = 128; b = 128;
	preserve_value_byte(r, g, b, identity_lut);
	assert(r == 128 && g == 128 && b == 128);

	r = 128; g = 64; b = 64;
	preserve_value_byte(r, g, b, overbright2_lut);
	assert(r == 255 && g == 128 && b == 128);

	// This is the case where luma mode clips: value mode keeps exact ratios.
	r = 200; g = 100; b = 50;
	preserve_value_byte(r, g, b, overbright2_lut);
	assert(r == 255 && g == 128 && b == 64);

	// A channel already at white has no headroom; nothing changes.
	r = 255; g = 128; b = 128;
	preserve_value_byte(r, g, b, overbright2_lut);
	assert(r == 255 && g == 128 && b == 128);

	// Midtone scales by the value curve without clipping.
	r = 10; g = 5; b = 2;
	preserve_value_byte(r, g, b, overbright2_lut);
	assert(r == 20 && g == 10 && b == 4);

	// Map normalization is folded into the scalar, so ratios still hold.
	r = 196; g = 98; b = 49;
	preserve_value_byte(r, g, b, overbright2_lut, normalize_both_lut);
	assert(r == 255 && g == 128 && b == 64);

	// The shared normalization LUT drives preserve mode too.
	r = 130; g = 130; b = 130;
	preserve_value_byte(r, g, b, identity_lut, normalize_both_lut);
	assert(r == 128 && g == 128 && b == 128);

	r = 130; g = 130; b = 130;
	preserve_value_byte(r, g, b, identity_lut, normalize_down_lut);
	assert(r == 98 && g == 98 && b == 98);

	r = 130; g = 130; b = 130;
	preserve_luminance_byte(r, g, b, identity_lut, normalize_both_lut);
	assert(r == 128 && g == 128 && b == 128);

	// A lifted black point becomes neutral grey.
	r = 0; g = 0; b = 0;
	preserve_value_byte(r, g, b, lift_lut);
	assert(r == 25 && g == 25 && b == 25);

	std::puts("lighting_curve tests passed");
	return 0;
}
