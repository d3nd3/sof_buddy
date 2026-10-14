#include "feature_config.h"

#if FEATURE_HTTP_MAPS

#include "util.h"
#include "../shared.h"

void http_maps_qcommon_frame_post_callback(int msec)
{
	(void)msec;
	static int* s_cls_state = (int*)rvaToAbsExe((void*)0x001C1F00);
	int state = s_cls_state ? *s_cls_state : -1;
	http_maps_on_frame_cls_state(state);
	if (state == (int)ca_active) return;
	if (!http_maps_frame_work_pending()) return;
	http_maps_pump();
	http_maps_run_deferred_continue_if_pending();
}

#endif // FEATURE_HTTP_MAPS
