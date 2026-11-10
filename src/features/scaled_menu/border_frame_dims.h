#pragma once

#include <cmath>

namespace scaled_menu_detail {

struct FixedRectDimension {
	int value;
	int outer;
};

inline bool resize_canvas_scale_is_applied(float canvas_scale, float max_expected_scale) {
	if (!std::isfinite(canvas_scale) || !std::isfinite(max_expected_scale) ||
	    max_expected_scale <= 0.f) {
		return false;
	}
	return canvas_scale > 1.001f && canvas_scale <= max_expected_scale * 2.f;
}

inline float fixed_rect_parse_factor(float contextual_factor, bool context_is_authoritative,
                                     float menu_scale, float widget_scale,
                                     bool preserve_authored_width = false) {
	// A ticker's width defines the banner's scrolling viewport, so keep its authored bound.
	if (preserve_authored_width) return 1.f;
	if (context_is_authoritative) return contextual_factor;
	if (!std::isfinite(menu_scale) || menu_scale <= 0.f ||
	    !std::isfinite(widget_scale) || widget_scale <= 1e-6f) {
		return 1.f;
	}
	return menu_scale / widget_scale;
}

inline FixedRectDimension scale_fixed_rect_dimension(int value, float widget_scale, int padding, float factor) {
	if (std::fabs(factor - 1.f) <= 0.001f) {
		const int outer = static_cast<int>(std::lround(static_cast<float>(value) * widget_scale)) + 2 * padding;
		return {value, outer};
	}
	const int scaled_value = static_cast<int>(std::lround(static_cast<float>(value) * factor));
	const int outer = static_cast<int>(std::lround(static_cast<float>(scaled_value) * widget_scale)) + 2 * padding;
	return {scaled_value, outer};
}

inline float border_dims_init_scale(float scale, float menu_scale) {
	if (std::fabs(scale - 1.f) <= 0.001f) return menu_scale;
	return scale;
}

inline int border_frame_dimension(int dimension, float factor, bool measured_box) {
	if (measured_box) return dimension;
	if (std::fabs(factor - 1.f) <= 0.001f) return dimension;
	return static_cast<int>(std::lround(static_cast<float>(dimension) * factor));
}

inline float scale_axis_coordinate(float coordinate, float pivot, float factor) {
	return pivot + (coordinate - pivot) * factor;
}

// Pixel offset that centres a measured text run inside a fixed-width text box.
// A label at least as wide as the box is left at its authored position.
inline int centered_text_offset(int box_width, int text_width) {
	if (box_width <= 0 || text_width <= 0 || text_width >= box_width) return 0;
	return (box_width - text_width) / 2;
}

} // namespace scaled_menu_detail
