/*
	CVars - Deprecated/Obsolete
	
	This file previously contained centralized cvar declarations for all features.
	CVars have been moved to their respective feature directories:
	
	- src/features/media_timers/cvars.cpp
	- src/features/lighting_blend/cvars.cpp
	- src/features/texture_mapping_min_mag/cvars.cpp
	- src/features/scaled_ui/cvars.cpp
	
	Each feature now manages its own cvars locally.
	
	This file is kept for reference but can be deleted once the migration is confirmed working.
*/

#include "sof_buddy.h"
#include "sof_compat.h"
#include "features.h"

/*
	OBSOLETE CVars - The following cvars were from deleted/refactored features:
	
	_sofbuddy_classic_timers       - sof_buddy.cpp (removed)
	_sofbuddy_whiteraven_lighting  - ref_fixes.cpp (removed)
	con_maxlines                   - scaled_ui.cpp (removed - unused experiment)
	con_buffersize                 - scaled_ui.cpp (removed - unused experiment)
	test                           - testing only
	test2                          - testing only
	
	MIGRATED CVars - Now in their respective feature directories:
	
	_sofbuddy_font_scale           - src/features/scaled_ui/cvars.cpp
	_sofbuddy_console_size         - src/features/scaled_ui/cvars.cpp
	_sofbuddy_hud_scale            - src/features/scaled_ui/cvars.cpp
*/