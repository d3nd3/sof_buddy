# Media Timers

## What it does
Replaces the game's default timer system with a high-precision QueryPerformanceCounter-based implementation. This provides more accurate timing for smoother gameplay and better frame rate control.

## Key Features
- **High-precision timing**: Uses QueryPerformanceCounter instead of timeGetTime()
- **Frame rate limiting**: Implements cl_maxfps for singleplayer
- **Sleep optimization**: Intelligent sleep/busy-wait hybrid for CPU efficiency
- **SoFPlus integration**: Works seamlessly with SoFPlus addon system

## Technical Details
- **Hook**: `Sys_Milliseconds` at `0x20055930`
- **Implementation**: Replaces with QueryPerformanceCounter-based timing
- **Memory addresses**: Patches main loop at `0x20066412`

## Configuration CVars
- `cl_maxfps` - Maximum frame rate (default: 30)
- `_sofbuddy_high_priority` - CPU priority boost (0/1)
- `_sofbuddy_sleep` - Enable sleep optimization (0/1)
- `_sofbuddy_sleep_jitter` - Frame time borrowing (0/1)
- `_sofbuddy_sleep_busyticks` - Busy-wait threshold in ms (default: 2)

## How it works
1. Hooks `Sys_Milliseconds()` to use QueryPerformanceCounter
2. Implements intelligent frame limiting with sleep/busy-wait hybrid
3. Handles timer synchronization for SoFPlus compatibility
4. Provides sub-millisecond timing accuracy

## Benefits
- Smoother gameplay with consistent frame timing
- Reduced CPU usage through smart sleep optimization
- Better compatibility with modern systems
- Frame rate limiting for singleplayer games


## Advanced Notes
### Summary
When a menu file is parsed, the width and height are sometimes specified there. So we hook the functions which parse the menu files, and try to parse it ourselves to adjust the width and heights.


