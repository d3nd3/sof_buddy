# Detour System Documentation

## Overview

The detour system is a **data-driven hooking framework** that separates hook definitions from implementation. This allows multiple features to safely hook the same function with type-safe callbacks.

**TLDR: `hooks.json` targets function signatures (from `detours.yaml`), `callbacks.json` targets dispatch strings (from `DISPATCH_SHARED_HOOK` calls).**

## Quick Start

1. **Add feature to `features/FEATURES.txt`**
2. **Create feature directory**: `src/features/my_feature/`
3. **Create `hooks/` subdirectory**: `src/features/my_feature/hooks/`
4. **Create `callbacks/` subdirectory**: `src/features/my_feature/callbacks/`
5. **Create `hooks.json`** in `hooks/` subdirectory (if hooking game functions)
6. **Create `callbacks.json`** in `callbacks/` subdirectory (if using lifecycle hooks)
7. **Create individual hook files** in `hooks/` (one file per hook callback)
8. **Create individual callback files** in `callbacks/` (one file per lifecycle callback)
9. **Rebuild**: `make clean && make BUILD=debug`

## System Flow

```
detours.yaml    hooks.json files    callbacks.json files    generate_hooks.py
     |                 |                      |                      |
     +-----------------+----------------------+----------------------+
                            |
                            v
              build/generated_detours.h
              build/generated_registrations.h
                            |
                            v
                    RegisterAllFeatureHooks()
                            |
                            v
                    Runtime Hook Execution
```

## Components

### Data Files

**`detours.yaml`** - Central registry of all hookable functions
- Defines function signatures, addresses, modules, calling conventions
- Single source of truth for all detours

**`hooks.json`** - Feature-specific hook declarations
- Each feature folder can have a `hooks.json`
- Lists which functions the feature wants to hook
- Specifies callback names, phases (Pre/Post), and priorities
- Supports override hooks with `"override": true` for full control over function behavior
- **TLDR: `hooks.json` targets function signatures** (from `detours.yaml`)

**`callbacks.json`** - Feature-specific shared hook callback declarations
- Each feature folder can have a `callbacks.json`
- Lists which shared lifecycle hooks the feature wants to register callbacks for
- Specifies callback names, phases (Pre/Post), and priorities
- Used for lifecycle events like `EarlyStartup`, `RefDllLoaded`, `GameDllLoaded`, `PostCvarInit`
- **Callbacks are functions that get dispatched by override hooks** - they are not automatically called, but are executed when a core override hook calls `DISPATCH_SHARED_HOOK`
- **TLDR: `callbacks.json` targets dispatch strings** (from `DISPATCH_SHARED_HOOK` calls)

### Code Generation

**`tools/generate_hooks.py`** - Build-time code generator
- Reads `detours.yaml`, all `hooks/hooks.json` files, and all `callbacks/callbacks.json` files
- **Only generates code for detours that are actually used** (not all from `detours.yaml`)
- Detects usage by:
  - Scanning `hooks/hooks.json` files for referenced functions
  - Scanning source files for `namespace detour_*` overrides
  - Scanning source files for `o*` function pointer references (e.g., `oDraw_StretchPic`)
- Generates `build/generated_detours.h` - Type-safe hook managers and detour functions (subset of `detours.yaml`)
- Generates `build/generated_registrations.h` - Callback registration code for both function hooks and shared lifecycle hooks
- **Automatically finds `hooks.json` and `callbacks.json` files in `hooks/` and `callbacks/` subdirectories**

**Important:** `generated_detours.h` is a **subset** of `detours.yaml`. If `detours.yaml` has 100 detours but only 20 are used, only those 20 are generated. This reduces code size, binary size, and startup overhead.

### Runtime System

**`TypedSharedHookManager<Ret, Args...>`** - Type-safe callback manager
- Manages Pre and Post callbacks for each detour
- Ensures type safety at compile time
- Dispatches callbacks in priority order

**`SharedHookManager`** (`src/core/shared_hook_manager.cpp`) - Shared lifecycle hook manager
- Manages callbacks for shared lifecycle events (EarlyStartup, RefDllLoaded, etc.)
- Allows multiple features to register callbacks for the same lifecycle event
- Dispatches callbacks in priority order
- Used for events that don't correspond to specific function detours

**`DetourSystem`** (`src/core/detours.cpp`) - Detour application and tracking
- Applies detours to target addresses
- Tracks registered detours to prevent duplicates
- Resolves addresses from modules (ref.dll, game.dll, etc.)
- Manages detour lifecycle (registration, application, removal)

## Adding a New Feature

### Step 1: Enable the Feature

Add your feature name to `features/FEATURES.txt`:
```
my_feature
```

The `core` feature is always enabled. Other features must be listed in `FEATURES.txt` to be processed.

### Step 2: Create Feature Directory Structure

```bash
mkdir -p src/features/my_feature/hooks
mkdir -p src/features/my_feature/callbacks
```

### Step 3: Check if Function Exists in `detours.yaml`

Before hooking a function, check if it's already defined in `detours.yaml`. If not, add it:

```yaml
functions:
  - name: MyFunction
    module: SofExe
    identifier: "0x00123456"
    return_type: int
    calling_convention: __cdecl
    detour_len: 0
    params:
      - type: int
        name: param1
      - type: char*
        name: param2
```

### Step 4: Create `hooks.json` (Optional)

If you want to hook game functions, create `src/features/my_feature/hooks/hooks.json`:

```json
[
  {
    "function": "MyFunction",
    "callback": "my_function_callback",
    "phase": "Post",
    "priority": 50
  }
]
```

**Fields:**
- `function` - Function name from `detours.yaml` (required, case-sensitive)
- `callback` - Your callback function name (required)
- `phase` - `"Pre"` or `"Post"` (defaults to `"Post"`)
- `priority` - Execution order, higher = first (defaults to `0`)
- `override` - `true` for override hooks (defaults to `false`)

### Step 5: Create `callbacks.json` (Optional)

If you want to use lifecycle hooks, create `src/features/my_feature/callbacks/callbacks.json`:

```json
[
  {
    "hook": "EarlyStartup",
    "callback": "my_feature_early_startup",
    "priority": 70,
    "phase": "Post"
  },
  {
    "hook": "RefDllLoaded",
    "callback": "my_feature_ref_dll_loaded",
    "priority": 50,
    "phase": "Post"
  }
]
```

**Available lifecycle hooks:**
- `EarlyStartup` - After DllMain, before CVars (dispatched in `lifecycle_EarlyStartup()`)
- `PreCvarInit` - After filesystem init, before CVars (dispatched by `FS_InitFilesystem` override)
- `PostCvarInit` - After all CVars created (dispatched by `Cbuf_AddLateCommands` override)
- `RefDllLoaded` - After ref.dll loads (dispatched by `VID_LoadRefresh` override)
- `GameDllLoaded` - After game.dll loads (dispatched by `Sys_GetGameApi` override)
- `PlayerDllLoaded` - After player.dll loads (not currently implemented)

**Important**: 
- **Callbacks are functions that get dispatched by override hooks** - they are not automatically called
- Core lifecycle hooks are implemented as override hooks in `src/core/hooks.json`
- These override hooks manually call `DISPATCH_SHARED_HOOK` to dispatch lifecycle callbacks
- **The `"hook"` field value must exactly match the string used in `DISPATCH_SHARED_HOOK` calls** (case-sensitive)
- Hook names are string literals, not constants or enums - they must match between `DISPATCH_SHARED_HOOK(hook_name, ...)` calls and `callbacks.json`

### Step 6: Implement Your Callbacks

Create individual files for each hook and callback. The recommended structure is one file per hook/callback:

**`src/features/my_feature/hooks/my_function.cpp`** (for function hooks):
```cpp
#include "feature_config.h"

#if FEATURE_MY_FEATURE

#include "sof_compat.h"
#include "util.h"
#include "generated_detours.h"

int my_function_callback(int result, int param1, char* param2) {
    return result;
}

#endif // FEATURE_MY_FEATURE
```

**`src/features/my_feature/hooks/my_function_pre.cpp`** (for Pre callbacks):
```cpp
#include "feature_config.h"

#if FEATURE_MY_FEATURE

#include "sof_compat.h"
#include "util.h"
#include "generated_detours.h"

void my_function_pre_callback(int& param1, char*& param2) {
    param1 = 42;
}

#endif // FEATURE_MY_FEATURE
```

**`src/features/my_feature/callbacks/my_feature_early.cpp`** (for lifecycle callbacks):
```cpp
#include "feature_config.h"

#if FEATURE_MY_FEATURE

#include "sof_compat.h"
#include "util.h"

void my_feature_early_startup(void) {
    PrintOut(PRINT_LOG, "My feature: Early startup\n");
}

#endif // FEATURE_MY_FEATURE
```

**`src/features/my_feature/callbacks/my_feature_refdll.cpp`**:
```cpp
#include "feature_config.h"

#if FEATURE_MY_FEATURE

#include "sof_compat.h"
#include "util.h"

void my_feature_ref_dll_loaded(char const* name) {
    PrintOut(PRINT_LOG, "My feature: Ref DLL loaded: %s\n", name ? name : "default");
}

#endif // FEATURE_MY_FEATURE
```

**Note**: The `*_hooks.cpp` file pattern is now optional. It's only needed if you have non-hook implementation code (helper functions, shared state, etc.) that doesn't belong in individual hook files.

### Step 7: Build and Test

```bash
make clean && make BUILD=debug
```

## Callback Signatures

### Function Hook Callbacks

- **Pre callbacks**: `void callback(Param1& p1, Param2& p2)` - Parameters by reference (can modify)
- **Post callbacks (void return)**: `void callback(Param1 p1, Param2 p2)` - Parameters by value (read-only)
- **Post callbacks (non-void return)**: `ReturnType callback(ReturnType result, Param1 p1, Param2 p2)` - Return value first, then parameters

### Lifecycle Hook Callbacks

- **No parameters**: `void callback(void)` - No parameters
- **With parameters**: 
  - `RefDllLoaded`: `void callback(char const* name)` - Receives DLL name
  - `GameDllLoaded`: `void callback(void* imports)` - Receives imports pointer

## Override Hooks

Override hooks provide **full control** over a function's behavior. Unlike normal hooks (which use Pre/Post callbacks), override hooks give you complete control over when, if, and how the original function is called.

### When to Use Overrides

1. **Don't want to call the original function** - Completely replace function behavior
2. **Want to adjust arguments to an original call** - Transform or modify parameters before calling original
3. **Want to call the original multiple times** - Implement retry logic or call with different parameters

### Declaring Override Hooks

Create `src/features/my_feature/hooks/hooks.json`:
```json
[
  {
    "function": "MyFunction",
    "callback": "my_override_callback",
    "override": true
  }
]
```

**Important**: The `override` field must be `true` (boolean, not string). The `phase` and `priority` fields are ignored for override hooks.

### Implementing Override Callbacks

The override callback signature includes all original function parameters plus the original function pointer as the **last parameter**:

```cpp
// For void functions
void my_override_callback(int param1, char* param2, detour_MyFunction::tMyFunction original) {
    // Use case 1: Don't call the original
    if (should_block()) {
        return;
    }
    
    // Use case 2: Adjust arguments before calling original
    int adjusted_param1 = clamp(param1, 0, 100);
    char* sanitized_param2 = sanitize(param2);
    original(adjusted_param1, sanitized_param2);
    
    // Use case 3: Call original multiple times
    original(param1, param2);
    original(param1 + 1, param2);
}

// For non-void functions
int my_override_callback(int param1, char* param2, detour_MyFunction::tMyFunction original) {
    if (should_block()) {
        return 0;  // Return custom value without calling original
    }
    
    int adjusted_param1 = clamp(param1, 0, 100);
    return original(adjusted_param1, param2);
}
```

### Uniqueness Constraint

**Only one override is allowed per function across all features.** The build system validates this and will exit with an error if duplicates are found.

### Override vs Normal Hooks

| Feature | Normal Hooks | Override Hooks |
|---------|-------------|----------------|
| **Multiple Features** | ✅ Multiple features can hook same function | ❌ Only one override per function |
| **Pre/Post Phases** | ✅ Supports Pre and Post callbacks | ❌ No Pre/Post phases |
| **Priority System** | ✅ Callbacks execute in priority order | ❌ Priority ignored |
| **Automatic Original Call** | ✅ Original called automatically | ❌ You control when/if original is called |
| **Parameter Modification** | ✅ Pre callbacks can modify parameters | ✅ Full control over parameters |
| **Return Value Modification** | ✅ Post callbacks can modify return values | ✅ Full control over return values |

## Lifecycle Hooks

### What Are Callbacks?

**Callbacks** are functions that get dispatched by override hooks. They are registered via `callbacks.json` and executed when the corresponding override hook calls `DISPATCH_SHARED_HOOK`.

**Key Points**:
- Callbacks are **not** automatically called - they must be dispatched by an override hook
- Core lifecycle hooks are override hooks that manually call `DISPATCH_SHARED_HOOK` or `DISPATCH_SHARED_HOOK_ARGS` to dispatch callbacks
- Features register callbacks via `callbacks.json` to be notified when lifecycle events occur
- Multiple features can register callbacks for the same lifecycle event
- Some lifecycle hooks support parameters (e.g., `RefDllLoaded` receives `char const* name`, `GameDllLoaded` receives `void* imports`)

### Available Lifecycle Hooks

- **`EarlyStartup`** - Called during early initialization, before CVars are available (dispatched in `lifecycle_EarlyStartup()`)
- **`PreCvarInit`** - Called before CVar initialization (dispatched by `FS_InitFilesystem` override hook)
- **`PostCvarInit`** - Called after CVar initialization (dispatched by `Cbuf_AddLateCommands` override hook)
- **`RefDllLoaded`** - Called after ref.dll (renderer) has loaded (dispatched by `VID_LoadRefresh` override hook)
- **`GameDllLoaded`** - Called after game.dll has loaded (dispatched by `Sys_GetGameApi` override hook)

**Note**: Core lifecycle hooks (`FS_InitFilesystem`, `Cbuf_AddLateCommands`, `VID_LoadRefresh`, `Sys_GetGameApi`) are implemented as override hooks in `src/core/hooks/hooks.json`. They manually call `DISPATCH_SHARED_HOOK` to dispatch lifecycle callbacks.

**Important**: The hook names are **string literals** that must match exactly between:
- The identifier in `DISPATCH_SHARED_HOOK(hook_name, ...)` calls (e.g., `DISPATCH_SHARED_HOOK(RefDllLoaded, Post)`)
- The `"hook"` field in `callbacks/callbacks.json` files (e.g., `"hook": "RefDllLoaded"`)

The `DISPATCH_SHARED_HOOK` macro uses `#hook_name` to stringify the identifier, so the hook name in `callbacks/callbacks.json` must match the stringified identifier exactly (case-sensitive). If they don't match, callbacks won't be dispatched.

### Example: Multiple Features Registering for Same Event

**Feature A** (`callbacks/callbacks.json`):
```json
[{"hook": "RefDllLoaded", "callback": "feature_a_init", "priority": 70, "phase": "Post"}]
```

**Feature B** (`callbacks/callbacks.json`):
```json
[{"hook": "RefDllLoaded", "callback": "feature_b_init", "priority": 60, "phase": "Post"}]
```

When `RefDllLoaded` is dispatched:
1. `feature_a_init` (priority 70, executes first)
2. `feature_b_init` (priority 60, executes second)

## Rules and Caveats

### Feature Enablement

- **Features must be listed in `features/FEATURES.txt`** to be processed by the build system
- Only features listed in `FEATURES.txt` will have their `hooks/hooks.json` and `callbacks/callbacks.json` files processed
- The `core` feature is always processed regardless of `FEATURES.txt`
- Lines starting with `#` or `//` are treated as comments and ignored
- Empty lines are ignored

### File Location Requirements

- **`hooks.json` must be located in the `hooks/` subdirectory** (`src/features/<feature_name>/hooks/hooks.json`)
- **`callbacks.json` must be located in the `callbacks/` subdirectory** (`src/features/<feature_name>/callbacks/callbacks.json`)
- **Individual hook implementations** should be in `src/features/<feature_name>/hooks/<hook_name>.cpp` (one file per hook)
- **Individual callback implementations** should be in `src/features/<feature_name>/callbacks/<callback_name>.cpp` (one file per callback)
- Core hooks can be in `src/core/hooks/hooks.json`
- Core callbacks can be in `src/core/callbacks/callbacks.json`
- The `*_hooks.cpp` file pattern is optional and only needed for non-hook implementation code

### JSON File Format Requirements

- **`hooks.json` must be a valid JSON array** of hook objects
  - Each hook object must have: `function`, `callback`
  - Optional fields: `phase` (defaults to "Post"), `priority` (defaults to 0), `override` (defaults to false)
  - The `"function"` field must match a function name from `detours.yaml` (case-sensitive)

- **`callbacks.json` must be a valid JSON array** of callback objects
  - Each callback object must have: `hook`, `callback`
  - Optional fields: `phase` (defaults to "Post"), `priority` (defaults to 0)
  - The `"hook"` field must match the string used in `DISPATCH_SHARED_HOOK` calls (case-sensitive)

- **Malformed JSON files will cause errors** - the script will print errors to stderr but continue processing other features

### Function Name Matching

- **Function names in `hooks/hooks.json` must exactly match names in `detours.yaml`** (case-sensitive)
- If a function name doesn't exist in `detours.yaml`, a warning is printed and the hook is skipped
- Function names are matched exactly: `Draw_Pic` ≠ `draw_pic` ≠ `DrawPic`

### Callback Name Uniqueness

- **Callback names should be unique or prefixed with feature name** to avoid conflicts
- The script doesn't validate uniqueness - duplicate callback names across features will cause linker errors
- Best practice: prefix callback names with feature name (e.g., `scaledUI_EarlyStartup`, `raw_mouse_Init`)

### Priority and Phase Defaults

- **`priority` defaults to `0` if not specified** in `hooks/hooks.json` or `callbacks/callbacks.json`
  - Higher priority values execute first
  - Priority can be any integer value

- **`phase` defaults to `"Post"` if not specified** in `hooks/hooks.json` or `callbacks/callbacks.json`
  - Valid values: `"Pre"` or `"Post"`
  - Case-sensitive: must be exactly `"Pre"` or `"Post"`

### Detour Generation Rules

- **Only used detours are generated** - unused detours from `detours.yaml` are skipped
  - A detour is considered "used" if:
    1. It's referenced in any `hooks/hooks.json` file (including override hooks)
    2. It's referenced via `namespace detour_*` override in source files
    3. The original function pointer `o*` is referenced in source files
  - This reduces binary size and startup overhead

- **Detours are only generated for enabled features**
  - If a feature is not in `FEATURES.txt`, its hooks won't cause detours to be generated
  - However, if another enabled feature uses the same detour, it will still be generated

### Module Identifier Rules

- **Hex addresses must start with `0x` or `0X`** for direct address resolution
  - Example: `identifier: "0x00123456"`
  - Used for SofExe module functions

- **Export names are used for DLL functions** (resolved via `GetProcAddress`)
  - Example: `identifier: "GetCursorPos"`
  - Used for RefDll, GameDll, PlayerDll, and Windows API functions
  - Module determines which DLL to search:
    - `RefDll` → `ref.dll`
    - `GameDll` → `game.dll`
    - `PlayerDll` → `player.dll`
    - `SofExe` → main executable (GetModuleHandleA(NULL))
    - `Unknown` → `user32.dll` (default for Windows API)

### Original Function Pointer Access

- **Original function pointers follow the pattern `o{FunctionName}`** (e.g., `oDraw_Pic`, `oR_DrawFont`)
  - They are declared as `extern` in `generated_detours.h` and defined in `generated_detours.cpp`
  - They are scoped to namespace blocks: `detour_{FunctionName}::o{FunctionName}`
  - To access from multiple files, use `using` declarations: `using detour_Draw_Pic::oDraw_Pic;`
  - Or use fully qualified names: `detour_Draw_Pic::oDraw_Pic`
  - The script detects usage by searching for `o{FunctionName}` patterns in source files

- **If you reference `o{FunctionName}` in source code, the detour will be generated even if not in `hooks/hooks.json`**
  - The script scans all `.cpp`, `.c`, `.h`, `.hpp` files for `o*` function pointer references
  - This allows features to use original functions without declaring hooks

- **Why `extern` instead of `static`?**
  - `extern` ensures a single shared instance across all translation units
  - This is critical for the detour system - the trampoline address must be stored in the same variable that the hook function reads from
  - `static` would create separate instances per translation unit, causing NULL pointer issues

### Error Handling

- **JSON parsing errors are printed to stderr but don't stop the build**
  - Malformed `hooks.json` or `callbacks.json` files will cause warnings
  - The script continues processing other features
  - Check build output for warnings about missing functions or parse errors

- **Missing function warnings** are printed when a function in `hooks/hooks.json` doesn't exist in `detours.yaml`
  - The hook is skipped, but the build continues
  - Example: `Warning: Function 'MyFunction' not found in detours.yaml (feature: my_feature)`

### Source File Scanning

- **The script scans all `.cpp`, `.c`, `.h`, `.hpp` files** in the `src` directory
  - Used to detect `namespace detour_*` overrides and `o*` function pointer references
  - File encoding errors are ignored (files are read with `errors='ignore'`)
  - Scanning happens recursively through all subdirectories

### File Organization Best Practices

- **One hook per file**: Create individual `.cpp` files in `hooks/` for each hook callback (e.g., `draw_pic.cpp`, `gl_findimage.cpp`)
- **One callback per file**: Create individual `.cpp` files in `callbacks/` for each lifecycle callback (e.g., `my_feature_early.cpp`, `my_feature_refdll.cpp`)
- **Use `*_hooks.cpp` sparingly**: Only create a `*_hooks.cpp` file if you have non-hook implementation code (helper functions, shared state, custom detours, etc.) that doesn't belong in individual hook files
- **Shared state**: If multiple hooks need shared state, consider creating a separate `*_shared.cpp` file or placing it in a header file
- **File naming**: Name hook files after the function being hooked (e.g., `draw_pic.cpp` for `Draw_Pic` hook), and callback files after the callback name (e.g., `my_feature_early.cpp` for `my_feature_early_startup` callback)

## Technical Details

### Generated Code Structure

**Generated code uses namespaces: `detour_{FunctionName}`**
- Contains: type alias `t{FunctionName}`, original pointer `o{FunctionName}`, manager `manager`, hook function `hk{FunctionName}`
- Access original function: `detour_{FunctionName}::o{FunctionName}`
- Access manager: `detour_{FunctionName}::manager`

**Static initialization happens automatically** via `AutoDetour_{FunctionName}` structs
- These register detours at program startup before `main()`
- No manual registration needed for detours

**Callback registration happens in `RegisterAllFeatureHooks()`**
- Called during `lifecycle_EarlyStartup()` in core code
- Registers all callbacks from `hooks/hooks.json` and `callbacks/callbacks.json`
- Must be called before hooks can execute

### How It Works

1. **Build System**:
   - `make` runs `generate_hooks.py`
   - Generates hook managers and registration code
   - `RegisterAllFeatureHooks()` is called at startup
   - Callbacks are registered with their respective managers

2. **Runtime Execution** (for function hooks):
   - Generated hook function `hkMyFunction` intercepts
   - `manager.DispatchPre(param1)` - Calls all Pre callbacks
   - Original function `oMyFunction(param1)` executes
   - `manager.DispatchPost(param1)` - Calls all Post callbacks

3. **Runtime Execution** (for lifecycle hooks):
   - Core override hook (e.g., `VID_LoadRefresh` override) calls the original function
   - Core override hook calls `DISPATCH_SHARED_HOOK(RefDllLoaded, Post)` to dispatch lifecycle callbacks
   - `SharedHookManager` dispatches all registered callbacks in priority order
   - Each feature's callback function is executed

### Detour Application

The actual detour patching is performed by the **DetourXS library** (`src/DetourXS/detourxs.cpp`):

1. **Registration Phase** (static initialization):
   - `generated_detours.h` creates static `AutoDetour_*` structs
   - Constructor calls `DetourSystem::RegisterDetour()`
   - Detour is stored but **not yet applied**

2. **Application Phase** (module load):
   - `DetourSystem::ApplyModuleDetours()` is called when module loads
   - For each registered detour:
     - Resolves address via `ResolveAddress()` (converts RVA to absolute address)
     - Calls `DetourCreate()` from DetourXS library
     - `DetourCreate()` patches the target function with a JMP instruction
     - Creates a trampoline containing original bytes + JMP back
     - Stores trampoline in `applied_detours` map and `o*` function pointer

3. **Module-Specific Application**:
   - **SoF.exe detours**: Applied in `lifecycle_EarlyStartup()`
   - **System DLL detours**: Applied in `lifecycle_EarlyStartup()`
   - **ref.dll detours**: Applied in `VID_LoadRefresh` override hook
   - **game.dll detours**: Applied in `Sys_GetGameApi` override hook

### Detour Cleanup

Detours are removed when modules unload or during shutdown:
- **game.dll detours**: Removed in `SV_ShutdownGameProgs` callback
- **All detours**: Can be removed via `DetourSystem::RemoveAllDetours()`

## Examples

### Complete Feature Example

**`features/FEATURES.txt`:**
```
my_feature
```

**`src/features/my_feature/hooks/hooks.json`:**
```json
[
  {
    "function": "Draw_Pic",
    "callback": "my_draw_pic_callback",
    "phase": "Post",
    "priority": 50
  }
]
```

**`src/features/my_feature/callbacks/callbacks.json`:**
```json
[
  {
    "hook": "PostCvarInit",
    "callback": "my_feature_init",
    "priority": 50,
    "phase": "Post"
  }
]
```

**`src/features/my_feature/hooks/draw_pic.cpp`:**
```cpp
#include "feature_config.h"

#if FEATURE_MY_FEATURE

#include "sof_compat.h"
#include "util.h"
#include "generated_detours.h"

void my_draw_pic_callback(int x, int y, char* name, int alpha) {
}

#endif // FEATURE_MY_FEATURE
```

**`src/features/my_feature/callbacks/my_feature_init.cpp`:**
```cpp
#include "feature_config.h"

#if FEATURE_MY_FEATURE

#include "sof_compat.h"
#include "util.h"

void my_feature_init(void) {
    PrintOut(PRINT_LOG, "My feature initialized\n");
}

#endif // FEATURE_MY_FEATURE
```

### Multiple Features Hooking Same Function

**Feature A** (`hooks/hooks.json`):
```json
[{"function": "Draw_Pic", "callback": "feature_a_callback", "priority": 100}]
```

**Feature B** (`hooks/hooks.json`):
```json
[{"function": "Draw_Pic", "callback": "feature_b_callback", "priority": 50}]
```

Both callbacks will execute when `Draw_Pic` is called, in priority order:
1. `feature_a_callback` (priority 100, Pre)
2. Original `Draw_Pic`
3. `feature_b_callback` (priority 50, Post)

## Priority System

Callbacks execute in priority order (higher numbers run first):
- **100+** - Critical system initialization
- **50-99** - Normal feature initialization
- **1-49** - User configuration
- **0** - Cleanup and finalization
