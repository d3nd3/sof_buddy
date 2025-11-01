/*
	TeamIcons Offset Fix - Widescreen FOV Correction
	
	Fixes TeamIcons location in widescreen resolutions by:
	- Adjusting FOV calculations for team icons
	- Correcting vertical position offset for different aspect ratios
	- Fixing icon positioning based on real vs virtual screen dimensions
*/

#include "feature_config.h"

#if FEATURE_TEAMICONS_OFFSET

#include "feature_macro.h"
#include "shared_hook_manager.h"
#include "util.h"
#include <cmath>

// Intentionally NOT static
double fovfix_x = 78.6;
double fovfix_y = 78.6;
float teamviewFovAngle = 95;

// Engine addresses
static float* engine_fovY = nullptr;
static float* engine_fovX = nullptr;
static unsigned int* realHeight = nullptr;
static unsigned int* virtualHeight = nullptr;
static int* icon_min_y = nullptr;
static int* icon_height = nullptr;

// Function declarations for inline assembly intercept
extern "C" int TeamIconInterceptFix(void);
static void teamicons_offset_RefDllLoaded();

// Hook registrations placed after function declarations for visibility
// Use REGISTER_HOOK_LEN with 6-byte detour length to match original implementation
REGISTER_HOOK_LEN(drawTeamIcons, (void*)0x00003040, RefDll, 6, void, __cdecl, 
                  float* targetPlayerOrigin, char* playerName, 
                  char* imageNameTeamIcon, int redOrBlue);
REGISTER_SHARED_HOOK_CALLBACK(RefDllLoaded, teamicons_offset, teamicons_offset_RefDllLoaded, 70, Post);

/*
	drawTeamIcons Hook Implementation
	
	Calculates FOV correction multipliers for widescreen displays.
	The game's default team icon positioning assumes 4:3 aspect ratio.
	
	FOV calculation:
	- fovfix_x/y = fov / tan(fov*0.5 radians)
	- This converts screen-space coordinates to angular coordinates
	- Diagonal FOV = fovX * sqrt(2) for proper widescreen scaling
*/
void hkdrawTeamIcons(float* targetPlayerOrigin, char* playerName, 
                     char* imageNameTeamIcon, int redOrBlue)
{
	float fovY = *engine_fovY;
	float fovX = *engine_fovX;

	// Calculate FOV correction multipliers
	// These are used at 0x3000313F and 0x30003157
	fovfix_x = fovX / tan(degToRad(fovX * 0.5f));
	fovfix_y = fovY / tan(degToRad(fovY * 0.5f));

	// Calculate diagonal FOV for proper widescreen display
	float diagonalFov = fovX * 1.4142f;  // sqrt(2) ≈ 1.4142
	teamviewFovAngle = diagonalFov;

	// Call original function with corrected FOV values in place
	odrawTeamIcons(targetPlayerOrigin, playerName, imageNameTeamIcon, redOrBlue);
}

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

	// Adjust for letterboxing (when real height != virtual height)
	// This happens with widescreen aspect ratios
	float HalfHeight = thedata + (rHeight - vHeight) * 0.5f;

	// Bounds checking - hide icon if outside visible area
	int minY = *icon_min_y;
	int maxY = minY + *icon_height;
	
	if (HalfHeight < minY) return -20;  // Above screen
	if (HalfHeight > maxY) return -20;  // Below screen

	return (int)HalfHeight;
}

// Apply ref.dll-dependent memory patches when ref.dll is loaded
static void teamicons_offset_RefDllLoaded() {
    PrintOut(PRINT_LOG, "TeamIcons Offset: Initializing widescreen fixes...\n");

    engine_fovY = (float*)rvaToAbsRef((void*)0x0008FD00);
    engine_fovX = (float*)rvaToAbsRef((void*)0x0008FCFC);
    realHeight = (unsigned int*)rvaToAbsRef((void*)0x0008FFC8);
    virtualHeight = (unsigned int*)rvaToAbsRef((void*)0x0008FCF8);
    icon_min_y = (int*)rvaToAbsExe((void*)0x00225ED4);
    icon_height = (int*)rvaToAbsExe((void*)0x00225EDC);

    // Patch FOV multiplier references
    writeUnsignedIntegerAt(rvaToAbsRef((void*)0x0000313F), (unsigned int)&fovfix_x);
    writeUnsignedIntegerAt(rvaToAbsRef((void*)0x00003157), (unsigned int)&fovfix_y);

    // Patch call to our intercept function for vertical adjustment (E8 rel32)
    writeIntegerAt(rvaToAbsRef((void*)0x00003187), (int)&TeamIconInterceptFix - (int)rvaToAbsRef((void*)0x00003187) - 4);

    // Patch diagonal FOV reference used for view angle calculations (in exe)
    writeUnsignedIntegerAt(rvaToAbsExe((void*)0x00157A8), (unsigned int)&teamviewFovAngle);

    PrintOut(PRINT_LOG, "TeamIcons Offset: Widescreen fixes applied\n");
}

#endif // FEATURE_TEAMICONS_OFFSET