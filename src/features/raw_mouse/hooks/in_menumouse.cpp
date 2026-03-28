#include "feature_config.h"

#if FEATURE_RAW_MOUSE

#include "sof_compat.h"

static int raw_mouse_in_menumouse_depth = 0;

bool raw_mouse_in_menumouse_scope() { return raw_mouse_in_menumouse_depth > 0; }

void raw_mouse_in_menumouse_pre(cvar_t*& cvar1, cvar_t*& cvar2) {
  (void)cvar1;
  (void)cvar2;
  ++raw_mouse_in_menumouse_depth;
}

void raw_mouse_in_menumouse_post(cvar_t* cvar1, cvar_t* cvar2) {
  (void)cvar1;
  (void)cvar2;
  if (raw_mouse_in_menumouse_depth > 0)
    --raw_mouse_in_menumouse_depth;
}

#endif
