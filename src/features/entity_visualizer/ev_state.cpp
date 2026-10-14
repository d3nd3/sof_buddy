#include "feature_config.h"

#if FEATURE_ENTITY_VISUALIZER

#include "ev_internal.h"

namespace ev {

bool IsEnabled() {
	return _sofbuddy_entity_edit && _sofbuddy_entity_edit->value != 0.0f;
}

} // namespace ev

#endif // FEATURE_ENTITY_VISUALIZER
