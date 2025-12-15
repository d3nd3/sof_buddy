#pragma once

#include "feature_config.h"

#if FEATURE_TEAMICONS_OFFSET

extern double fovfix_x;
extern double fovfix_y;
extern float teamviewFovAngle;

extern float *engine_fovY;
extern float *engine_fovX;
extern unsigned int *realHeight;
extern unsigned int *virtualHeight;
extern int *icon_min_y;
extern int *icon_height;

extern "C" int TeamIconInterceptFix(void);

void teamicons_offset_RefDllLoaded(char const* name);

#endif // FEATURE_TEAMICONS_OFFSET


