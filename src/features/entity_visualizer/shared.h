#pragma once

#include "feature_config.h"

#if FEATURE_ENTITY_VISUALIZER

// Opaque engine types – we never depend on their layout here.
struct edict_s;
struct usercmd_s;

typedef edict_s   edict_t;
typedef usercmd_s usercmd_t;

// CVars: PostCvarInit registers feature cvars (see callbacks/ev_postcvar.cpp).
// Gameplay hooks live in ev_hooks.cpp.

#endif // FEATURE_ENTITY_VISUALIZER

