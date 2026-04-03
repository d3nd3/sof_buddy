#include "feature_config.h"

#if FEATURE_RAW_MOUSE

#include "../shared.h"
#include "generated_detours.h"

using detour_Sys_SendKeyEvents::tSys_SendKeyEvents;

void sys_sendkeyevents_override_callback(tSys_SendKeyEvents original) {
  // return original();
  /* Raw off: run the real Sys_SendKeyEvents (PeekMessage/GetMessage/Translate/
   * Dispatch until empty, then timeGetTime -> sys_frame_time per stock). That
   * is not redundant with WinMain — Q2-style code pumps in both places.
   * Raw on: replace that function with capped pump + same frame-time update. */

  raw_mouse_consume_deltas();
  Sys_SendKeyEvents_Replacement();
}

#endif
