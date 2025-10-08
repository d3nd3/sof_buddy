# TeamIcons Offset Fix

## Overview
Fixes team icon positioning in widescreen resolutions. The game's default team icon system assumes a 4:3 aspect ratio, causing icons to appear in incorrect positions on modern widescreen displays.

## Problems Solved

### Team Icons Misaligned in Widescreen
- **Problem**: Team icons appear off-center or misplaced on 16:9/16:10 displays
- **Root Cause**: FOV and position calculations hardcoded for 4:3 aspect ratio
- **Solution**: Dynamic FOV correction and letterbox compensation

### Incorrect Vertical Positioning
- **Problem**: Icons positioned incorrectly when real screen height differs from virtual height
- **Root Cause**: No adjustment for letterboxing/pillarboxing
- **Solution**: Intercept height calculations and adjust for aspect ratio differences

## Technical Details

### Hook Implementation
```cpp
// Hook the team icons drawing function
REGISTER_HOOK(drawTeamIcons, 0x30003040, void, __cdecl, 
              float* targetPlayerOrigin, char* playerName, 
              char* imageNameTeamIcon, int redOrBlue);
```

### Memory Layout

#### ref_gl.dll Memory
- `drawTeamIcons()`: `0x30003040` - Main team icon rendering function
- `FOV X multiplier`: `0x3000313F` - Patched to point to `fovfix_x`
- `FOV Y multiplier`: `0x30003157` - Patched to point to `fovfix_y`
- `Height intercept call`: `0x30003187` - Patched to call `TeamIconInterceptFix()`
- `Diagonal FOV reference`: `0x200157A8` - Patched to point to `teamviewFovAngle`

#### Engine Variables
- `engine_fovX`: `0x3008FCFC` - Current horizontal FOV
- `engine_fovY`: `0x3008FD00` - Current vertical FOV
- `realHeight`: `0x3008FFC8` - Actual screen height
- `virtualHeight`: `0x3008FCF8` - Virtual rendering height
- `icon_min_y`: `0x20225ED4` - Minimum Y coordinate for icons
- `icon_height`: `0x20225EDC` - Icon display height

### Implementation Flow

```
Game renders team icons
  → hkdrawTeamIcons() intercepts
  → Calculates FOV correction multipliers:
      fovfix_x = fovX / tan(fovX * 0.5 radians)
      fovfix_y = fovY / tan(fovY * 0.5 radians)
  → Sets diagonal FOV = fovX * sqrt(2)
  → Calls odrawTeamIcons() with corrected values
  
During icon positioning:
  → Game calls patched height calculation at 0x30003187
  → TeamIconInterceptFix() intercepts
  → Adjusts Y position for letterboxing:
      adjusted_y = original_y + (realHeight - virtualHeight) * 0.5
  → Returns corrected position or -20 (hide) if out of bounds
```

### FOV Correction Mathematics

The fix uses the following mathematical approach:

```
Standard FOV scaling:
  fovfix = fov / tan(degToRad(fov * 0.5))

For widescreen displays:
  - Converts screen-space coordinates to angular coordinates
  - Accounts for aspect ratio differences from 4:3
  - Uses diagonal FOV for proper widescreen scaling
  
Diagonal FOV calculation:
  diagonal_fov = fovX * sqrt(2)  // Pythagorean theorem for diagonal
```

### Assembly Intercept

The `TeamIconInterceptFix()` function uses inline assembly to extract the FPU stack value:
```cpp
asm volatile("fstps %0" : "=m"(thedata));
```
This captures the Y coordinate being calculated by the game before final positioning.

## Configuration
No CVars required - the fix is automatically applied based on current FOV and screen dimensions.

## Benefits
- **Proper widescreen support** - Team icons display correctly on all aspect ratios
- **Dynamic adjustment** - Automatically adapts to FOV and resolution changes
- **Maintains gameplay** - Icons only hidden when genuinely out of bounds
- **Zero performance impact** - Calculations done only during icon rendering

## Usage Example
Simply play the game normally. Team icons will automatically position correctly regardless of:
- Screen resolution (1920x1080, 2560x1440, etc.)
- Aspect ratio (16:9, 16:10, 21:9 ultrawide)
- Custom FOV settings
- Letterboxed or pillarboxed displays

## Development Notes
- Uses direct memory patching for FOV multiplier references
- Intercepts height calculation via E8 call injection at `0x30003187`
- FPU stack manipulation requires `extern "C"` linkage
- Returns `-20` to hide icons that would appear off-screen
- Part of the widescreen compatibility improvements

## Archived notes
/*
the code takes the middle of screen
uses it as a starting point for math
then adds an offset to it
to get the location of the teamicon
the offset value is a byproduct of the fov, so it needs the (viewport height)
the starting point is a byproduct of the entire screen somehow, so it needs the window height
the default code uses viewport height for both
i had to seperate it

it takes half the height, as a starting location
it tries to get center of the window
but .. if you take half of the viewport, when its squahsed
and add that to edge of window
it doesnt take you to center of window
thats the bug
get it ?

@ctrl the TeamIcon are drawn 2d, so can draw outside the 3d viewport.  The positioning of the teamicon is using variable "halfHeight" to get use center of screen as origin to the relative positioning of teamicon.  When viewport is smaller vertically than window in gl_mode 3 @ fov111 , halfHeight is no longer center of screen.  So made the code use Half of Window Height properly.  Since the 2d teamicon is free to draw outside the viewport in gaps above and below.. had to put in a valid check if its off viewport co or dinates..  Also found the default draw only within fov == 95 hard coded ( it uses cl.frame sent from server fov of 95 ) .. modified specific location where its using this to calculate if teamicon within fov and adjusted to 1.4x fovX.  The code by default uses 95 in all directions, thats why its fine Vertically usually but teamcion not show on wide fov.. So changing thsi to 1.4x fovX meant its always visible. (1.4 is just number to ensure it reaches diagonal distance of screen to all corners).

@ctrl *if its off viewport co-ordinates i just set its position to be -20 .. so its not seen (drawn completley offscreen)
@ctrl to fix the showing top and bottom of screen in gl_mode 3 111 fov issue
*/
