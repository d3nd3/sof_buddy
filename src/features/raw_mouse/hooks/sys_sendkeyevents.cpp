#include "feature_config.h"

#if FEATURE_RAW_MOUSE

#include "../shared.h"
#include "generated_detours.h"

using detour_Sys_SendKeyEvents::tSys_SendKeyEvents;

void sys_sendkeyevents_override_callback(tSys_SendKeyEvents original) {
  (void)original;
  raw_mouse_on_peek_returned_no_message();
  raw_mouse_consume_deltas();

  raw_mouse_process_raw_mouse();
  Sys_SendKeyEvents_Replacement();
}

#endif
