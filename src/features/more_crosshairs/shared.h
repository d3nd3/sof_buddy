#pragma once

#include "feature_config.h"

#if FEATURE_MORE_CROSSHAIRS

void more_crosshairs_PostCvarInit(void);
void more_crosshairs_refresh_menu_preview(void);
const char* more_crosshairs_get_list_rmf(void);
void more_crosshairs_gl_findimage_pre(char* filename, int& imagetype, char& mimap, char& allowPicmip);
void* more_crosshairs_gl_findimage_fixup(void* result, char* filename, int imagetype, char mimap,
                                         char allowPicmip);

#endif
