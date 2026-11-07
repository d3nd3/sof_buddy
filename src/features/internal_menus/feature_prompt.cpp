#include "feature_config.h"

#if FEATURE_INTERNAL_MENUS

#include "shared.h"
#include "generated_detours.h"
#include "runtime_features.h"
#include "util.h"

#include <cstring>
#include <string>

namespace {

struct FeatureRule {
	const char* cvar;
	const char* feature;
	const char* feature2;
	const char* feature3;
	const char* label;
};

constexpr FeatureRule kRules[] = {
    {"_sofbuddy_font_scale", "scaled_con", nullptr, nullptr, "Scaled Console"},
    {"_sofbuddy_font_scale_auto", "scaled_con", nullptr, nullptr, "Scaled Console"},
    {"_sofbuddy_console_size", "scaled_con", nullptr, nullptr, "Scaled Console"},
    {"_sofbuddy_font_pack", "scaled_con", "scaled_hud", "scaled_menu", "Scaled UI"},
    {"_sofbuddy_hud_scale", "scaled_hud", nullptr, nullptr, "Scaled HUD"},
    {"_sofbuddy_hud_scale_auto", "scaled_hud", nullptr, nullptr, "Scaled HUD"},
    {"_sofbuddy_crossh_scale", "scaled_hud", nullptr, nullptr, "Scaled HUD"},
    {"_sofbuddy_crossh_scale_auto", "scaled_hud", nullptr, nullptr, "Scaled HUD"},
    {"_sofbuddy_scale_cinematic_pics", "scaled_hud", nullptr, nullptr, "Scaled HUD"},
    {"_sofbuddy_icons_autoscale", "scaled_hud", nullptr, nullptr, "Scaled HUD"},
    {"_sofbuddy_scale_round_auto", "scaled_con", "scaled_hud", "scaled_menu", "Scaled UI"},
    {"_sofbuddy_scale_round_ratio", "scaled_con", "scaled_hud", "scaled_menu", "Scaled UI"},
    {"_sofbuddy_menu_scale", "scaled_menu", nullptr, nullptr, "Scaled Menu"},
    {"_sofbuddy_menu_scale_auto", "scaled_menu", nullptr, nullptr, "Scaled Menu"},
    {"_sofbuddy_menu_tip_appear", "scaled_menu", nullptr, nullptr, "Scaled Menu"},
    {"_sofbuddy_menu_font_size_style", "scaled_menu", nullptr, nullptr, "Scaled Menu"},
    {"_sofbuddy_minfilter_unmipped", "texture_mapping_min_mag", nullptr, nullptr, "Texture Mapping"},
    {"_sofbuddy_magfilter_unmipped", "texture_mapping_min_mag", nullptr, nullptr, "Texture Mapping"},
    {"_sofbuddy_minfilter_mipped", "texture_mapping_min_mag", nullptr, nullptr, "Texture Mapping"},
    {"_sofbuddy_magfilter_mipped", "texture_mapping_min_mag", nullptr, nullptr, "Texture Mapping"},
    {"_sofbuddy_minfilter_ui", "texture_mapping_min_mag", nullptr, nullptr, "Texture Mapping"},
    {"_sofbuddy_magfilter_ui", "texture_mapping_min_mag", nullptr, nullptr, "Texture Mapping"},
    {"_sofbuddy_lighting_overbright", "lighting_blend", nullptr, nullptr, "Lighting Blend"},
    {"_sofbuddy_lighting_cutoff", "lighting_blend", nullptr, nullptr, "Lighting Blend"},
    {"_sofbuddy_water_size", "lighting_blend", nullptr, nullptr, "Lighting Blend"},
    {"_sofbuddy_lightblend_src", "lighting_blend", nullptr, nullptr, "Lighting Blend"},
    {"_sofbuddy_lightblend_dst", "lighting_blend", nullptr, nullptr, "Lighting Blend"},
    {"_sofbuddy_shiny_spherical", "lighting_blend", nullptr, nullptr, "Lighting Blend"},
    {"_sofbuddy_http_maps", "http_maps", nullptr, nullptr, "HTTP Maps"},
    {"_sofbuddy_http_maps_dl_1", "http_maps", nullptr, nullptr, "HTTP Maps"},
    {"_sofbuddy_http_maps_dl_2", "http_maps", nullptr, nullptr, "HTTP Maps"},
    {"_sofbuddy_http_maps_dl_3", "http_maps", nullptr, nullptr, "HTTP Maps"},
    {"_sofbuddy_http_maps_crc_1", "http_maps", nullptr, nullptr, "HTTP Maps"},
    {"_sofbuddy_http_maps_crc_2", "http_maps", nullptr, nullptr, "HTTP Maps"},
    {"_sofbuddy_http_maps_crc_3", "http_maps", nullptr, nullptr, "HTTP Maps"},
    {"_sofbuddy_http_show_providers", "http_maps", nullptr, nullptr, "HTTP Maps"},
    {"_sofbuddy_rawmouse", "raw_mouse", nullptr, nullptr, "Raw Mouse"},
    {"_sofbuddy_high_priority", "media_timers", nullptr, nullptr, "Media Timers"},
    {"_sofbuddy_sleep", "media_timers", nullptr, nullptr, "Media Timers"},
    {"_sofbuddy_sleep_jitter", "media_timers", nullptr, nullptr, "Media Timers"},
    {"_sofbuddy_sleep_busyticks", "media_timers", nullptr, nullptr, "Media Timers"},
    {"_sofbuddy_cl_maxfps_exact", "media_timers", nullptr, nullptr, "Media Timers"},
    {"_sofbuddy_entity_edit", "entity_visualizer", nullptr, nullptr, "Entity Visualizer"},
    {"_sofbuddy_entities_draw_verbose", "entity_visualizer", nullptr, nullptr, "Entity Visualizer"},
    {"_sofbuddy_map_debug_map", "entity_visualizer", nullptr, nullptr, "Entity Visualizer"},
    {"_sofbuddy_map_study_deathmatch", "entity_visualizer", nullptr, nullptr, "Entity Visualizer"},
    {"_sofbuddy_crosshair_theme", "more_crosshairs", nullptr, nullptr, "More Crosshairs"},
};

constexpr char kFeatureCvar[] = "_sb_internal_feature_prompt_feature";
constexpr char kSettingCvar[] = "_sb_internal_feature_prompt_cvar";

bool s_buddy_menu_open = false;
bool s_prompt_open = false;
bool s_prompt_pending = false;

bool starts_with(const char* value, const char* prefix) {
	return value && prefix && std::strncmp(value, prefix, std::strlen(prefix)) == 0;
}

const FeatureRule* find_rule(const char* name) {
	if (!name) return nullptr;
	for (const FeatureRule& rule : kRules)
		if (std::strcmp(name, rule.cvar) == 0) return &rule;
	return nullptr;
}

RuntimeFeatures::Feature feature_from_name(const char* name) {
	if (!name || !name[0]) return RuntimeFeatures::Feature::Count;
	const std::string cvar = std::string("_sofbuddy_feature_") + name;
	return RuntimeFeatures::FromCvarName(cvar.c_str());
}

bool any_feature_enabled(const FeatureRule& rule) {
	const char* names[] = {rule.feature, rule.feature2, rule.feature3};
	bool found = false;
	for (const char* name : names) {
		if (!name) continue;
		const RuntimeFeatures::Feature feature = feature_from_name(name);
		if (feature == RuntimeFeatures::Feature::Count) continue;
		found = true;
		if (RuntimeFeatures::Enabled(feature)) return true;
	}
	return !found;
}

void set_prompt_cvar(const char* name, const char* value) {
	if (!detour_Cvar_Get::oCvar_Get || !detour_Cvar_Set2::oCvar_Set2) return;
	cvar_t* cvar = detour_Cvar_Get::oCvar_Get(name, "", 0, nullptr);
	if (cvar)
		detour_Cvar_Set2::oCvar_Set2(const_cast<char*>(name),
		                             const_cast<char*>(value ? value : ""), true);
}

} // namespace

void internal_menus_feature_prompt_init_cvars(void) {
	if (!detour_Cvar_Get::oCvar_Get) return;
	detour_Cvar_Get::oCvar_Get(kFeatureCvar, "", 0, nullptr);
	detour_Cvar_Get::oCvar_Get(kSettingCvar, "", 0, nullptr);
}

static void Cmd_SoFBuddy_FeaturePrompt_Close_f(void) {
	internal_menus_feature_prompt_note_closed();
	if (detour_Cmd_ExecuteString::oCmd_ExecuteString)
		detour_Cmd_ExecuteString::oCmd_ExecuteString("killmenu");
}

void internal_menus_feature_prompt_init_command(void) {
	static bool registered = false;
	if (registered || !detour_Cmd_AddCommand::oCmd_AddCommand) return;
	detour_Cmd_AddCommand::oCmd_AddCommand(
	    const_cast<char*>("sofbuddy_feature_prompt_close"),
	    Cmd_SoFBuddy_FeaturePrompt_Close_f);
	registered = true;
}

void internal_menus_feature_prompt_note_menu(const char* menu_file) {
	const bool prompt = menu_file && std::strstr(menu_file, "feature_prompt") != nullptr;
	if (!prompt) {
		s_prompt_open = false;
		s_prompt_pending = false;
	}
	s_buddy_menu_open = menu_file && starts_with(menu_file, "sof_buddy");
}

void internal_menus_feature_prompt_note_closed(void) {
	const bool closing_prompt = s_prompt_open;
	s_prompt_open = false;
	s_prompt_pending = false;
	if (closing_prompt) {
		s_buddy_menu_open = true;
		return;
	}
	auto* menu_system = *reinterpret_cast<void**>(
	    rvaToAbsExe(reinterpret_cast<void*>(0x00406554)));
	s_buddy_menu_open = menu_system != nullptr;
}

void internal_menus_feature_prompt_tick(void) {
	if (!s_prompt_pending) return;
	if (!s_buddy_menu_open || s_prompt_open) {
		if (!s_buddy_menu_open) s_prompt_pending = false;
		return;
	}
	s_prompt_pending = false;
	s_prompt_open = true;
	if (detour_Cmd_ExecuteString::oCmd_ExecuteString)
		detour_Cmd_ExecuteString::oCmd_ExecuteString(
		    "sofbuddy_menu sof_buddy/feature_prompt");
	else
		s_prompt_open = false;
}

bool internal_menus_feature_prompt_handle_cvar(const char* cvar_name) {
	if (!cvar_name || !s_buddy_menu_open) return false;
	const FeatureRule* rule = find_rule(cvar_name);
	if (!rule || any_feature_enabled(*rule)) return false;
	if (s_prompt_open || s_prompt_pending) return true;

	set_prompt_cvar(kFeatureCvar, rule->label);
	set_prompt_cvar(kSettingCvar, cvar_name);
	s_prompt_pending = true;
	return true;
}

#endif
