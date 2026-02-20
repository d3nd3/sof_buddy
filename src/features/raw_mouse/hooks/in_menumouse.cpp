#include "feature_config.h"

#if FEATURE_RAW_MOUSE

#include "../shared.h"

void in_menumouse_pre_callback(void*& cvar1, void*& cvar2) {
  (void)cvar1;
  (void)cvar2;
  if (!raw_mouse_is_enabled() || raw_mouse_mode() != 2) return;
  if (raw_mouse_is_wine()) raw_mouse_drain_wm_input();
  raw_mouse_poll();
}

void in_menumouse_callback(void* cvar1, void* cvar2) {
  (void)cvar1;
  (void)cvar2;
  if (!raw_mouse_is_enabled()) return;
  if (raw_mouse_mode() == 1) raw_mouse_poll();
  raw_mouse_consume_deltas();
}

#endif // FEATURE_RAW_MOUSE