# Module Loading Lifecycle System

## Overview
**Core infrastructure** that provides lifecycle hooks for when DLLs are loaded/reloaded, allowing features to apply detours to module-specific memory addresses.

> **Note**: This is NOT a feature - it's structural code located in `src/module_loaders.cpp` that enables the module loading system for all features.

## Problem Solved
- **DLL Reloading**: Some DLLs can be reloaded, clearing their memory
- **Module-Specific Addresses**: Addresses starting with `0x5XXXXXXX` are in loaded DLLs
- **Timing**: Detours to DLL functions must be applied AFTER the DLL is loaded

## Module Loading Events

### Available Lifecycle Phases

| Phase | When | Purpose | Address Range |
|-------|------|---------|---------------|
| `RefDllLoaded` | After ref.dll loads | Apply graphics/rendering detours | `0x5XXXXXXX` (ref.dll) |
| `GameDllLoaded` | After game.dll loads | Apply game logic detours | `0x5XXXXXXX` (game.dll) |
| `PlayerDllLoaded` | After player.dll loads | Apply player-specific detours | `0x5XXXXXXX` (player.dll) |

### Usage Pattern

```cpp
#include "../../../hdr/shared_hook_manager.h"

// Register callback for when ref.dll is loaded
REGISTER_SHARED_HOOK_CALLBACK(RefDllLoaded, my_feature, setup_ref_hooks, 50);

static void setup_ref_hooks() {
    // Now safe to apply detours to ref.dll functions
    // Example: DetourCreate((void*)0x50075190, &my_function, DETOUR_TYPE_JMP, 7);
    PrintOut(PRINT_LOG, "My Feature: Applied ref.dll detours\n");
}
```

**⚠️ Important Timing Note**: Module loading phases occur **before** the PostCvarInit phase. Any CVars created in PostCvarInit callbacks will be **NULL** when accessed from module loading callbacks. See the CVar timing section in `src/features/FEATURES.md` for solutions.

## Implementation Details

### Hooks Used

| Function | Address | Module | Purpose |
|----------|---------|--------|---------|
| `VID_LoadRefresh` | `0x20066E10` | exe | Detects ref.dll loading |
| `Sys_GetGameApi` | `0x20065F20` | exe | Detects game.dll loading |

### Execution Flow

#### 1. ref.dll Loading
```
Game calls VID_LoadRefresh() 
  → hkVID_LoadRefresh() runs
  → oVID_LoadRefresh() loads ref.dll
  → DISPATCH_SHARED_HOOK(RefDllLoaded)
  → Features apply ref.dll detours
```

#### 2. game.dll Loading  
```
Game calls Sys_GetGameApi()
  → hkSys_GetGameApi() runs  
  → oSys_GetGameApi() loads game.dll
  → DISPATCH_SHARED_HOOK(GameDllLoaded)
  → Features apply game.dll detours
```

## Example: CinematicFreeze for cl_maxfps

The user's example shows how `cl_maxfps_singleplayer` needs to apply a detour after game.dll loads:

```cpp
// In cl_maxfps_singleplayer/hooks.cpp
REGISTER_SHARED_HOOK_CALLBACK(GameDllLoaded, cl_maxfps_singleplayer, apply_cinematic_fix, 60);

static void apply_cinematic_fix() {
    // Now safe to detour game.dll function
    DetourCreate((void*)0x50075190, &my_CinematicFreeze, DETOUR_TYPE_JMP, 7);
    PrintOut(PRINT_LOG, "cl_maxfps: Applied CinematicFreeze detour\n");
}
```

## Module Address Ranges

### Typical Memory Layout
- **SoF.exe**: `0x200XXXXX` - Main executable
- **ref.dll**: `0x5XXXXXXX` - Graphics/rendering (ref_gl.dll, ref_soft.dll)
- **game.dll**: `0x5XXXXXXX` - Game logic (gamex86.dll)
- **player.dll**: `0x5XXXXXXX` - Player models/animations

### Why This Matters
- DLLs can be **reloaded** (clearing their memory)
- Detours must be applied **after** the DLL loads
- Features targeting DLL functions need this lifecycle system

## Benefits
- **Reliable**: Detours always applied after DLL loads
- **Organized**: Clear separation by module type
- **Flexible**: Features can target specific modules
- **Robust**: Handles DLL reloading scenarios

## Future Extensions
Could add support for:
- `PlayerDllLoaded` - player model DLLs
- `SoundDllLoaded` - audio system DLLs  
- `NetworkDllLoaded` - networking DLLs
- Module-specific error handling
