#include "feature_config.h"

#if FEATURE_RAW_MOUSE

#include "../shared.h"
#include "generated_detours.h"

using detour_Sys_SendKeyEvents::tSys_SendKeyEvents;

void sys_sendkeyevents_override_callback(tSys_SendKeyEvents original) {
  (void)original;
  // Raw queue is drained only in getcursorpos_override_callback
  // (raw_mouse_drain_pending_raw_for_cursor). Per-frame delta reset here.
  raw_mouse_consume_deltas();
  Sys_SendKeyEvents_Replacement();
}

#endif
