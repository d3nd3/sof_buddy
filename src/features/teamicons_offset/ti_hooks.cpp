/*
	TeamIcons Offset Fix - Widescreen FOV Correction
	
	Fixes TeamIcons location in widescreen resolutions by:
	- Adjusting FOV calculations for team icons
	- Correcting vertical position offset for different aspect ratios
	- Fixing icon positioning based on real vs virtual screen dimensions
*/

#include "feature_config.h"

#if FEATURE_TEAMICONS_OFFSET

#include "util.h"
#include <cmath>
#include "shared.h"

// Intentionally NOT static
double fovfix_x = 78.6;
double fovfix_y = 78.6;
float teamviewFovAngle = 95;

// Engine addresses
float* engine_fovY = nullptr;
float* engine_fovX = nullptr;
unsigned int* realHeight = nullptr;
unsigned int* virtualHeight = nullptr;
int* icon_min_y = nullptr;
int* icon_height = nullptr;

/*
	TeamIconInterceptFix - Assembly Intercept Function
	
	Called from 0x30003187 via inline E8 call patch.
	
	Fixes vertical positioning by accounting for letterboxing:
	- ST0 contains: HalfHeight - (various calculations)
	- We adjust for the difference between real and virtual height
	- This centers icons properly when aspect ratio doesn't match native
	
	Returns: Adjusted Y coordinate, or -20 if out of bounds
*/
extern "C" int TeamIconInterceptFix(void)
{
	SOFBUDDY_ASSERT(realHeight != nullptr);
	SOFBUDDY_ASSERT(virtualHeight != nullptr);
	SOFBUDDY_ASSERT(icon_min_y != nullptr);
	SOFBUDDY_ASSERT(icon_height != nullptr);
	
	// Extract the FPU stack value (ST0) containing preliminary Y position
	float thedata;
	/* Use the single-precision FPU store mnemonic to avoid an
	   assembler warning about missing operand size. 'fstps' writes
	   ST(0) as a 32-bit float. Use an output constraint so the
	   compiler knows the memory is written. */
	asm volatile ("fstps %0" : "=m" (thedata));

	// Get current screen dimensions
	unsigned int rHeight = *realHeight;
	unsigned int vHeight = *virtualHeight;
	SOFBUDDY_ASSERT(rHeight > 0);
	SOFBUDDY_ASSERT(vHeight > 0);

	// Adjust for letterboxing (when real height != virtual height)
	// This happens with widescreen aspect ratios
	float HalfHeight = thedata + (rHeight - vHeight) * 0.5f;

	// Bounds checking - hide icon if outside visible area
	int minY = *icon_min_y;
	int maxY = minY + *icon_height;
	SOFBUDDY_ASSERT(*icon_height >= 0);
	
	if (HalfHeight < minY) return -20;  // Above screen
	if (HalfHeight > maxY) return -20;  // Below screen

	return (int)HalfHeight;
}


#endif // FEATURE_TEAMICONS_OFFSET