#pragma once

struct Sof2Feature {
	const char* name;
	void (*on_early_startup)(void);
	void (*on_post_cvar)(void);
};

void sof2_features_early_startup(void);
void sof2_features_post_cvar(void);
void sof2_lifecycle_start(void);
void sof2_lifecycle_on_wsock_ready(void);
