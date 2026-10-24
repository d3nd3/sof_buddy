#pragma once

#include "feature_config.h"

#if FEATURE_MORE_CROSSHAIRS

void more_crosshairs_PostCvarInit(void);
void more_crosshairs_refresh_menu_preview(void);
void* more_crosshairs_gl_findimage_fixup(void* result, char* filename, int imagetype, char mimap,
                                         char allowPicmip);

#endif
