#pragma once

#include "feature_config.h"

#if FEATURE_SCALED_SCOPE

// Called by the shared Draw_StretchPic hook while cScope::Draw is active.
// The vinetta quad is redirected to the centred 4:3 scope frame so the
// vignette keeps its authored shape on widescreen displays.
void scaled_scope_note_stretch_pic(int& x, int& y, int& w, int& h, const char* name);

// Vertical design scale (vid_h / 480 equivalent) used for the scope's
// magnification readout. 1.0 when the scope overlay is not being drawn.
float scaled_scope_text_scale(void);

// True while the scope hook is inside cScope::Draw.
bool scaled_scope_active(void);

#endif // FEATURE_SCALED_SCOPE
