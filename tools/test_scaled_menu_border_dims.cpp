#include "../src/features/scaled_menu/border_frame_dims.h"

#include <cassert>

int main() {
	assert(scaled_menu_detail::resize_canvas_scale_is_applied(2.25f, 3.f));
	assert(!scaled_menu_detail::resize_canvas_scale_is_applied(1.f, 3.f));
	assert(!scaled_menu_detail::resize_canvas_scale_is_applied(100000.f, 3.f));
	assert(scaled_menu_detail::fixed_rect_parse_factor(1.f, false, 2.25f, 1.f) == 2.25f);
	assert(scaled_menu_detail::fixed_rect_parse_factor(1.f, false, 2.25f, 1.5f) == 1.5f);
	assert(scaled_menu_detail::fixed_rect_parse_factor(1.f, true, 2.25f, 1.f) == 1.f);
	assert(scaled_menu_detail::fixed_rect_parse_factor(1.f, false, 2.25f, 0.f) == 1.f);
	const float ticker_width_factor =
	    scaled_menu_detail::fixed_rect_parse_factor(1.f, false, 2.25f, 1.f, true);
	assert(ticker_width_factor == 1.f);
	const auto ticker_width =
	    scaled_menu_detail::scale_fixed_rect_dimension(604, 1.f, 0, ticker_width_factor);
	assert(ticker_width.value == 604);
	const auto fixed_width = scaled_menu_detail::scale_fixed_rect_dimension(20, 3.f, 22, 0.75f);
	assert(fixed_width.value == 15);
	assert(fixed_width.outer == 89);
	const auto unchanged = scaled_menu_detail::scale_fixed_rect_dimension(20, 3.f, 22, 1.f);
	assert(unchanged.value == 20);
	assert(unchanged.outer == 104);
	assert(scaled_menu_detail::border_dims_init_scale(1.f, 2.25f) == 2.25f);
	assert(scaled_menu_detail::border_dims_init_scale(1.5f, 2.25f) == 1.5f);
	assert(scaled_menu_detail::border_dims_init_scale(1.f, 1.f) == 1.f);
	assert(scaled_menu_detail::border_frame_dimension(270, 0.75f, true) == 270);
	assert(scaled_menu_detail::border_frame_dimension(120, 2.25f, false) == 270);
	const float serverbox_factor =
	    scaled_menu_detail::fixed_rect_parse_factor(1.f, false, 2.25f, 1.5f);
	const auto serverbox_height =
	    scaled_menu_detail::scale_fixed_rect_dimension(100, 1.5f, 0, serverbox_factor);
	assert(serverbox_height.value == 150);
	assert(serverbox_height.outer == 225);
	assert(serverbox_height.outer / 27 - 2 == 6);
	assert(scaled_menu_detail::scale_axis_coordinate(20.f, 10.f, 2.25f) == 32.5f);
	assert(scaled_menu_detail::scale_axis_coordinate(10.f, 20.f, 2.25f) == -2.5f);
}
