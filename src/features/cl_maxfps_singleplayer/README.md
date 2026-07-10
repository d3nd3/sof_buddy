# cl_maxfps Singleplayer Fix

## Purpose

Makes the `cl_maxfps` cvar work properly in singleplayer mode and fixes the black loading screen bug after cinematics.

## Callbacks

- **EarlyStartup** (Post, Priority: 70)
  - `cl_maxfps_EarlyStartup()` - Applies memory patches to enable frame limiting in singleplayer and installs sp_Sys_Mil hook
- **GameDllLoaded** (Post, Priority: 50)
  - `cl_maxfps_GameDllLoaded()` - Clears stale exe cinematic-freeze on gamex86.dll load
- **SV_ShutdownGameProgs** (Pre, Priority: 90)
  - `cl_maxfps_ShutdownGameProgs()` - Clears exe cinematic-freeze before game.dll unloads

## Hooks

- **Qcommon_Frame** (Pre, Priority: 75)
  - `mfps_qcommon_frame_pre()` - Mirrors `CL_Frame` throttle decision before `SV_Frame` runs
- **SV_Frame** (Override)
  - `mfps_sv_frame_override()` - Skips server tick when the frame would be render-throttled (keeps SV/CL paired)
- **CinematicFreeze** (Post, Priority: 100)
  - `cinematicfreeze_callback()` - Synchronizes freeze state between game.dll and exe to prevent black loading screens
- **M_PushMenu** (Pre, Priority: 85)
  - `mfps_M_PushMenu_pre()` - Clears stale exe cinematic-freeze before intermission/level menus (fixes tsr1 endcin → black screen)
- **ExitLevel** (Post, Priority: 100)
  - `mfps_ExitLevel_post()` - Clears exe freeze flag when game.dll clears cinematic state without `CinematicFreeze(false)`

## OverrideHooks

None

## CustomDetours

- **sp_Sys_Mil** (o_sofplus+0xFA60)
  - `hksp_Sys_Mil()` - Sets `_sp_cl_frame_delay` to 0 before calling original to ensure cl_maxfps works in singleplayer
  - Applied during EarlyStartup if sofplus.dll is loaded

## Technical Details

### Memory Patches

- **CL_Frame() patch**: `0x2000D973`, `0x2000D974` - Apply `cl_maxfps` render throttle during SP (vanilla bypasses when connected to local server)
- **Paired SV/CL throttle**: `Qcommon_Frame` Pre + `SV_Frame` override skip the server tick when `CL_Frame` would early-return, so AI/cinematic sim does not outrun the capped frame rate
- **Freeze sync target**: `0x201E7F5B` - Exe's cinematic freeze flag

### Module-Specific Hook Architecture

This feature demonstrates the automatic module-specific hook system:

- Exe patches applied during EarlyStartup
- sp_Sys_Mil detour applied when sofplus.dll is loaded
- game.dll CinematicFreeze hook automatically applied when game.dll loads

### Implementation Flow

#### 1. Early Startup

```
SoF Buddy loads
  → cl_maxfps_EarlyStartup() runs
  → Patches CL_Frame() in exe memory  
  → Enables cl_maxfps in singleplayer
  → If sofplus.dll loaded, installs sp_Sys_Mil hook
```

#### 2. Frame Timing Control (Runtime)

```
Game calls sp_Sys_Mil()
  → hksp_Sys_Mil() intercepts
  → Sets _sp_cl_frame_delay to 0 at o_sofplus+0x331BC
  → Calls orig_sp_Sys_Mil() (original function)
  → Ensures cl_maxfps works in singleplayer
```

#### 3. Cinematic State Sync (Runtime)

```
Game calls CinematicFreeze(bEnable)
  → cinematicfreeze_callback() intercepts
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

## Known Issues Fixed

- [GitHub Issue #3](https://github.com/d3nd3/sof_buddy/issues/3) - Black Loading After Cinematic nyc1

