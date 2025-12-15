# TeamIcons Offset Fix

## Purpose
Fixes team icon positioning in widescreen resolutions. The game's default team icon system assumes a 4:3 aspect ratio, causing icons to appear in incorrect positions on modern widescreen displays.

## Callbacks
- **RefDllLoaded** (Post, Priority: 70)
  - `teamicons_offset_RefDllLoaded()` - Initializes engine variable pointers and applies memory patches

## Hooks
- **drawTeamIcons** (Pre, Priority: 100)
  - `drawteamicons_pre_callback()` - Calculates FOV correction multipliers for widescreen displays

## OverrideHooks
None

## CustomDetours
- **TeamIconInterceptFix** (ref.dll, via inline assembly intercept)
  - `TeamIconInterceptFix()` - Intercepts height calculation via E8 call injection at `0x30003187`
  - Adjusts Y position for letterboxing: `adjusted_y = original_y + (realHeight - virtualHeight) * 0.5`
  - Returns corrected position or -20 (hide) if out of bounds

## Technical Details

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
  → drawteamicons_pre_callback() intercepts (Pre hook)
  → Calculates FOV correction multipliers:
      fovfix_x = fovX / tan(fovX * 0.5 radians)
      fovfix_y = fovY / tan(fovY * 0.5 radians)
  → Sets diagonal FOV = fovX * sqrt(2)
  → Calls original drawTeamIcons() with corrected values
  
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
