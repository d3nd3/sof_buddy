# cl_maxfps Singleplayer Fix

## Overview
Makes the `cl_maxfps` cvar work properly in singleplayer mode and fixes the black loading screen bug after cinematics.

## Problems Solved

### 1. cl_maxfps Not Working in Singleplayer
- **Problem**: `cl_maxfps` cvar is ignored in singleplayer mode
- **Solution**: 
  - Patch `CL_Frame()` to enable frame limiting in singleplayer
  - Hook `sp_Sys_Mil` to set `_sp_cl_frame_delay` to 0 before calling original
- **Memory Patches**: NOPs at `0x2000D973` and `0x2000D974`
- **Detour Hook**: `sp_Sys_Mil` at `o_sofplus+0xFA60` sets `_sp_cl_frame_delay` at `o_sofplus+0x331BC` to 0

### 2. Black Loading Screen After Cinematics  
- **Problem**: Game shows black screen instead of loading screen after cinematics
- **Root Cause**: Desynchronization between game.dll and exe freeze states
- **Solution**: Hook `CinematicFreeze()` to synchronize freeze states

## Technical Details

### Module-Specific Hook Architecture
This feature demonstrates the automatic module-specific hook system:

```cpp
// Apply exe patches early
REGISTER_SHARED_HOOK_CALLBACK(EarlyStartup, cl_maxfps_singleplayer, cl_maxfps_EarlyStartup, 70);

// Install sp_Sys_Mil hook during EarlyStartup if sofplus is loaded
// (check extern HMODULE o_sofplus)

// Hook game.dll function - automatically applied when game.dll loads
REGISTER_HOOK(CinematicFreeze, 0x50075190, void, __cdecl, bool bEnable);
```

### sp_Sys_Mil Detour Implementation
The feature now uses a detour/hook to `sp_Sys_Mil` to ensure `_sp_cl_frame_delay` is always set to 0:

```cpp
// Hook sp_Sys_Mil function from sofplus
sp_Sys_Mil = (int(*)(void))((char*)o_sofplus + 0xFA60);

// Create detour that sets _sp_cl_frame_delay to 0 before calling original
void* trampoline = DetourCreate((void*)sp_Sys_Mil, (void*)hk_sp_Sys_Mil, DETOUR_TYPE_JMP, DETOUR_LEN_AUTO);

// Hook implementation
static int hk_sp_Sys_Mil(void) {
    // Set _sp_cl_frame_delay to 0 before calling original
    if (o_sofplus) {
        *(int*)(o_sofplus + 0x331BC) = 0x00;
    }
    
    // Call original function
    if (orig_sp_Sys_Mil) {
        return orig_sp_Sys_Mil();
    }
    
    return 0;
}
```

### Memory Layout

#### SoF.exe Memory  
- `CL_Frame() patch`: `0x2000D973`, `0x2000D974` - Enable singleplayer frame limiting
- `Freeze sync target`: `0x201E7F5B` - Exe's cinematic freeze flag

#### sofplus.dll Memory
- `sp_Sys_Mil()`: `o_sofplus+0xFA60` - Function that controls frame timing
- `_sp_cl_frame_delay`: `o_sofplus+0x331BC` - Frame delay variable (set to 0 by hook)

#### game.dll Memory
- `CinematicFreeze()`: `0x50075190` - Function that controls cinematic state  
- `Freeze state source`: `0x5015D8D5` - Game.dll's cinematic freeze flag

### Implementation Flow

#### 1. Early Startup (EarlyStartup)
```
SoF Buddy loads
  → cl_maxfps_EarlyStartup() runs
  → Patches CL_Frame() in exe memory  
  → Enables cl_maxfps in singleplayer
```

#### 2. Early sp_Sys_Mil Hook Installation
```
SoF Buddy loads
  → cl_maxfps_EarlyStartup() runs
  → If sofplus (extern o_sofplus) is loaded
     → Sets up sp_Sys_Mil hook at o_sofplus+0xFA60
  → Automatically applies CinematicFreeze hook at 0x50075190
  → Hooks are ready for operation
```

#### 3. Frame Timing Control (Runtime)
```
Game calls sp_Sys_Mil()
  → hk_sp_Sys_Mil() intercepts
  → Sets _sp_cl_frame_delay to 0 at o_sofplus+0x331BC
  → Calls orig_sp_Sys_Mil() (original function)
  → Ensures cl_maxfps works in singleplayer
```

#### 4. Cinematic State Sync (Runtime)
```
Game calls CinematicFreeze(bEnable)
  → hkCinematicFreeze() intercepts
  → Calls oCinematicFreeze() (original function)
  → Syncs freeze state between game.dll and exe
  → Prevents black loading screens
```

## Configuration
No CVars - fixes are automatically applied.

## Benefits
- **cl_maxfps works in singleplayer** - Frame limiting now functions correctly
- **No more black loading screens** - Cinematics transition properly to loading screens
- **Robust DLL handling** - Properly handles game.dll reloading scenarios
- **Makes `_sp_cl_frame_delay` obsolete** - Standard cl_maxfps now works everywhere
- **Dynamic frame delay control** - sp_Sys_Mil hook ensures frame delay is always 0

## Known Issues Fixed
- [GitHub Issue #3](https://github.com/d3nd3/sof_buddy/issues/3) - Black Loading After Cinematic nyc1

## Development Notes
- Uses automatic module-specific hook registration for game.dll functions
- Demonstrates manual detour creation for sofplus.dll functions
- Combines exe memory patching + DLL function hooking + sofplus detour
- Hooks are applied automatically when modules load via InitializeGameHooks()
- sp_Sys_Mil hook ensures _sp_cl_frame_delay is always 0 for proper cl_maxfps behavior
- Critical for singleplayer gameplay experience
- Part of core game stability improvements
