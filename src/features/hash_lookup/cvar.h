#pragma once

#include "feature_config.h"

#if FEATURE_HASH_LOOKUP

/** Creates the feature's gate cvar. Bootstrap-only (PostCvarInit). */
void HashLookup_InitCvars();

/** Whether _sofbuddy_hashmap asks for the index. Read once, at install time:
 *  the patches go in or they do not, and nothing re-reads this per lookup. */
bool HashLookup_Enabled();

#endif  // FEATURE_HASH_LOOKUP
