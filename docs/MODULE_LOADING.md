# Module Loading Lifecycle System

## Overview
Handles DLL loading events and applies hooks to specific modules at the right time.

## Problem Solved
- **DLL Reloading**: Some DLLs can be reloaded, clearing their memory
- **Module-Specific Addresses**: Addresses in DLLs must be applied AFTER the DLL is loaded
- **RVA to Absolute**: Relative addresses must be resolved to absolute when the module loads

## Module Loading Events

### Available Lifecycle Phases

| Phase | When | Purpose | Address Range |
|-------|------|---------|---------------|
| RefDllLoaded | After ref.dll loads | Apply graphics/rendering detours | ref_gl.dll RVAs |
| GameDllLoaded | After game.dll loads | Apply game logic detours | gamex86.dll RVAs |
| PlayerDllLoaded | After player.dll loads | Apply player-specific detours | player.dll RVAs |

### Hook Registration with Module Specification

The hook system requires you to explicitly specify the target module when registering hooks:

```cpp
#include "feature_macro.h"

// For ref.dll hooks - specify RefDll module
REGISTER_HOOK(GL_BuildPolygonFromSurface, (void*)0x00016390, RefDll, void, __cdecl, void* msurface_s)
{
    // Hook implementation
    // oGL_BuildPolygonFromSurface(msurface_s);
}

// For game.dll hooks - specify GameDll module
REGISTER_HOOK_LEN(DrawFunction, (void*)0x00012340, GameDll, 5, void, __cdecl, int x, int y);

// For SoF.exe hooks - specify SofExe module
REGISTER_HOOK_VOID(InitFunction, (void*)0x00045600, SofExe, void, __cdecl);
```

### Module Values

- `RefDll` - ref_gl.dll (ref.dll graphics renderer)
- `GameDll` - gamex86.dll (game logic)
- `SofExe` - SoF.exe (main executable)
- `PlayerDll` - player.dll (player-specific logic)

### RVA Address Handling

Addresses < `0x10000000` are treated as Relative Virtual Addresses (RVAs). The hook system automatically:
1. Detects RVA addresses when registering
2. Stores them with the specified module
3. Resolves them to absolute addresses when the module loads
4. Applies the hook immediately

**No manual address resolution needed** - just specify the RVA and module:

```cpp
// Automatically resolves 0x00016390 to absolute address when ref_gl.dll loads
REGISTER_HOOK(MyFunction, (void*)0x00016390, RefDll, void, __cdecl, int param);
```

### Module Loading Callbacks

For features that need initialization when modules load:

```cpp
#include "shared_hook_manager.h"

static void my_feature_RefDllLoaded(void);
REGISTER_SHARED_HOOK_CALLBACK(RefDllLoaded, my_feature, my_feature_RefDllLoaded, 70, Post);

static void my_feature_RefDllLoaded(void)
{
    PrintOut(PRINT_LOG, "My Feature: ref.dll loaded, hooks applied automatically\n");
}
```

## Implementation Details

### Hooks Used

| Function | Address | Module | Purpose |
|----------|---------|--------|---------|
| VID_LoadRefresh | 0x20066E10 | SoF.exe | Detects ref.dll loading |
| Sys_GetGameApi | 0x20065F20 | SoF.exe | Detects game.dll loading |

### Hook Application Flow

1. **Static Initialization**: Features register hooks with `REGISTER_HOOK`, specifying module and RVA
2. **Module Load Detection**: `VID_LoadRefresh` or `Sys_GetGameApi` detects DLL load
3. **RVA Resolution**: Hook manager resolves RVAs to absolute addresses for the loaded module
4. **Hook Application**: Detours are applied to the resolved absolute addresses
5. **Callbacks Executed**: `RefDllLoaded`, `GameDllLoaded`, etc. callbacks are fired

### Example: ref.dll Hook

```cpp
// In your feature hooks.cpp
REGISTER_HOOK(GL_Something, (void*)0x00012345, RefDll, void, __cdecl, float x, float y)
{
    // Your hook implementation
    PrintOut(PRINT_LOG, "GL_Something called\n");
    oGL_Something(x, y);
}
```

When `ref_gl.dll` loads:
1. Hook registered with RVA `0x00012345` and module `RefDll`
2. System detects ref.dll loading
3. System resolves: `0x00012345` + `base of ref_gl.dll` = absolute address
4. Hook is applied to the absolute address
5. Your hook now intercepts calls

## Benefits

- **Clean API**: One line to register a hook with all necessary information
- **Automatic Resolution**: No manual `rvaToAbsRef()` calls needed
- **Type Safety**: Module specified explicitly prevents mismatches
- **Timing Safety**: Hooks applied only after module is loaded
- **No Manual Hook Manager Calls**: All handled automatically
