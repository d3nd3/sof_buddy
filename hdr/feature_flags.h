#ifndef FEATURE_FLAGS_H
#define FEATURE_FLAGS_H

#include "sof_compat.h"
#include "features.h"

// Default-on for new granular flags unless explicitly turned OFF in features.h
// To disable, add `#define <FLAG>_OFF` in features.h before includes.

// Console paste fix is always on now

// Cinematic freeze fix is always on now

#if !defined(FEATURE_SOFPLUS_INTEGRATION) && !defined(FEATURE_SOFPLUS_INTEGRATION_OFF)
#define FEATURE_SOFPLUS_INTEGRATION
#endif

// Optional: summary printer
void features_print_summary(void);

#endif // FEATURE_FLAGS_H


