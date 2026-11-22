#include "buddy_menu.h"
#include "feature.h"

extern void feature_baseline_early_startup(void);
extern void feature_video_modes_early_startup(void);
extern void feature_video_modes_post_cvar(void);
extern void feature_core_post_cvar(void);

static const Sof2Feature kFeatures[] = {
	{"video_modes", feature_video_modes_early_startup, feature_video_modes_post_cvar},
	{"baseline", feature_baseline_early_startup, nullptr},
	{"buddy_menu", nullptr, feature_buddy_menu_post_cvar},
	{"core", nullptr, feature_core_post_cvar},
};

void sof2_features_early_startup(void) {
	for (const Sof2Feature& f : kFeatures)
		if (f.on_early_startup) f.on_early_startup();
}

void sof2_features_post_cvar(void) {
	for (const Sof2Feature& f : kFeatures)
		if (f.on_post_cvar) f.on_post_cvar();
}
