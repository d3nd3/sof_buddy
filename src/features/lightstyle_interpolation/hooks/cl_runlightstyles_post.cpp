/*
	Light Style Interpolation - CL_RunLightStyles post hook

	SoF advances animated light styles in 10 Hz steps: CL_RunLightStyles picks
	map[frame % length] and copies it into value[0..2]. The renderer then uses
	those values for this frame.

	When _sofbuddy_lightstyle_interpolation is enabled, recompute value[] by
	blending the current map entry with the next one, matching the Quake
	CL_RunLightStyles interpolation idea. When disabled, the stock values from
	the original function are left untouched.
*/

#include "feature_config.h"

#if FEATURE_LIGHTSTYLE_INTERPOLATION

#include "util.h"
#include "../shared.h"

#include <cstddef>

namespace {

constexpr int kMaxLightStyles = 256;
constexpr int kSkyLightStyle = 13;      // handled separately by the engine
constexpr int kLightStyleMapSize = 64;
constexpr double kLightStyleHz = 10.0;  // CL_RunLightStyles uses cl.time * 10

// Engine layout (sizeof == 0x110):
//   int   length;
//   float value[3];
//   float map[64];
struct clightstyle_t {
	int length;
	float value[3];
	float map[kLightStyleMapSize];
};

static_assert(offsetof(clightstyle_t, value) == 0x04, "unexpected value offset");
static_assert(offsetof(clightstyle_t, map) == 0x10, "unexpected map offset");
static_assert(sizeof(clightstyle_t) == 0x110, "unexpected clightstyle_t size");

constexpr unsigned int kClLightStyleRva = 0x0026B420;
constexpr unsigned int kClTimeRva = 0x001E8FC8;

} // namespace

void lightstyle_interpolation_run_post_callback(void) {
	if (!_sofbuddy_lightstyle_interpolation ||
	    _sofbuddy_lightstyle_interpolation->value == 0.0f) {
		return;
	}

	static clightstyle_t * styles = nullptr;
	static const float * cl_time = nullptr;

	if (!styles) {
		styles = static_cast<clightstyle_t *>(
			rvaToAbsExe(reinterpret_cast<void *>(kClLightStyleRva)));
		cl_time = static_cast<const float *>(
			rvaToAbsExe(reinterpret_cast<void *>(kClTimeRva)));
	}

	if (!styles || !cl_time) return;

	// cl.time is in seconds in SoF (the engine itself computes cl.time * 10).
	// Use double so the frame index matches the engine's x87 multiply/truncate.
	const double frame_time = static_cast<double>(*cl_time) * kLightStyleHz;
	const int lc = static_cast<int>(frame_time);
	if (lc < 0) return;

	const float frac = static_cast<float>(frame_time - static_cast<double>(lc));
	if (frac <= 0.0f) return;

	const int ln = lc + 1;

	for (int i = 0; i < kMaxLightStyles; ++i) {
		if (i == kSkyLightStyle) continue;

		clightstyle_t & ls = styles[i];
		if (ls.length <= 1) continue;

		const float current = ls.map[lc % ls.length];
		const float next = ls.map[ln % ls.length];
		const float value = current * (1.0f - frac) + next * frac;

		ls.value[0] = value;
		ls.value[1] = value;
		ls.value[2] = value;
	}
}

#endif // FEATURE_LIGHTSTYLE_INTERPOLATION
