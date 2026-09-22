# Media Timers

## Purpose
Replaces the game's default timer system with a high-precision QueryPerformanceCounter-based implementation. This provides more accurate timing for smoother gameplay, better frame rate control, and intelligent CPU usage optimization.

## Callbacks
- **EarlyStartup** (Post, Priority: 70)
  - `mediaTimers_EarlyStartup()` - Initializes timer system
- **PostCvarInit** (Post, Priority: 70)
  - `mediaTimers_PostCvarInit()` - Registers CVars and applies memory patches

## Hooks
None

## OverrideHooks
- **Sys_Milliseconds** (SoF.exe)
  - `sys_milliseconds_override_callback()` - Replaces default timer with QueryPerformanceCounter-based implementation

## CustomDetours
- **Qcommon_Frame** (SoFPlus, if loaded)
  - Custom frame timing integration for SoFPlus compatibility
- **Memory patches** at `0x20066412` - Main loop timing adjustments

## Technical Details

### High-Precision Timing
Uses Windows `QueryPerformanceCounter` instead of `timeGetTime()`:

- Elapsed ms is `uint32_t` (same 49.7-day wrap as `timeGetTime`), not `int`
  (which overflowed at 24.8 days).
- `curtime` is `SoF.exe` RVA `0x390D38` via `rvaToAbsExe` (VA `0x20390D38`).
- QPC origin is a ready-flag, not `base.QuadPart == 0` (a rebase can land on 0
  and used to restart the clock).
- A backward QPC still shifts the origin; a failed read after init returns the
  last elapsed ticks instead of killing the process.

### Frame Rate Limiting
Implements `cl_maxfps` for singleplayer with intelligent sleep/busy-wait hybrid:
- Sleeps when frame time is long enough
- Busy-waits for precise timing near target FPS
- Configurable thresholds via CVars

### SoFPlus Integration
Works seamlessly with SoFPlus addon system:
- Detects SoFPlus DLL loading
- Synchronizes timer state
- Handles frame timing for both systems

## Configuration CVars
- `cl_maxfps` - Maximum frame rate (default: 30)
- `_sofbuddy_high_priority` - CPU priority boost (0/1)
- `_sofbuddy_sleep` - Enable sleep optimization (0/1)
- `_sofbuddy_sleep_jitter` - Frame time borrowing (0/1)
- `_sofbuddy_sleep_busyticks` - Busy-wait threshold in ms (default: 2)

## Benefits
- Smoother gameplay with consistent frame timing
- Reduced CPU usage through smart sleep optimization
- Better compatibility with modern systems
- Frame rate limiting for singleplayer games
- Sub-millisecond timing accuracy
