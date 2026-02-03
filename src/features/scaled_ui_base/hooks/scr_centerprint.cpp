#include "feature_config.h"

#if FEATURE_SCALED_HUD || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE

#include "features.h"
#include "util.h"
#include "generated_detours.h"
#include "../shared.h"

void hkSCR_CenterPrint(char * text, detour_SCR_CenterPrint::tSCR_CenterPrint original) {
	SOFBUDDY_ASSERT(original != nullptr);
	if (!text) {
		original(text);
		return;
	}
	float s = 1.0f;
	if (_sofbuddy_font_scale) s = _sofbuddy_font_scale->value;
	if (s <= 0.0f) s = 1.0f;
	int baseWidth = 40;
	int maxChars = (int)(baseWidth / s);
	if (maxChars < 1) maxChars = 1;
	char buf[1024];
	int bi = 0;
	int col = 0;
	for (int i = 0; text[i] && bi < (int)sizeof(buf) - 1; i++) {
		char c = text[i];
		buf[bi++] = c;
		if (c == '\n') {
			col = 0;
			continue;
		}
		col++;
		if (col >= maxChars && text[i + 1] && text[i + 1] != '\n') {
			if (bi < (int)sizeof(buf) - 1) buf[bi++] = '\n';
			col = 0;
		}
	}
	buf[bi] = 0;
	original(buf);
}

#endif


