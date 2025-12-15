#include "feature_config.h"

#if FEATURE_RAW_MOUSE

#include "shared.h"

int raw_mouse_delta_x = 0;
int raw_mouse_delta_y = 0;
POINT window_center = {0, 0};
std::vector<BYTE> g_heapBuffer;

#endif // FEATURE_RAW_MOUSE

