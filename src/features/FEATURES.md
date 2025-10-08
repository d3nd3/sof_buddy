# SoF Buddy Features

## Adding New Features

Super simple! Just create a folder with two files:

### 1. Create Feature Folder
```bash
mkdir src/features/my_feature
```

### 2. Add Hook Code
**`src/features/my_feature/hooks.cpp`**
```cpp
#include "../../../hdr/feature_macro.h"

REGISTER_HOOK(MyFunction, 0x12345678, void, __cdecl, int param) {
    // Your hook logic here
    oMyFunction(param);
}
```

### 3. Add Documentation  
**`src/features/my_feature/README.md`**
```markdown
# My Feature

## What it does
Brief description...

## Technical Details
- Memory addresses
- Configuration
- Usage notes
```

### 4. Build
```bash
make
```

That's it! No Python scripts, no complex registration, no manifest files.

## Feature Enable/Disable

### Compile-time Configuration System
SoF Buddy uses a **compile-time feature toggle system** that eliminates disabled features from the binary entirely.

#### Configuration File: `features/FEATURES.txt`
```bash
# SoF Buddy Features Configuration
# Lines starting with # are comments
# Uncommented lines = enabled features  
# Commented lines (with //) = disabled features

# Core Features (always enabled)
console_protection
cl_maxfps_singleplayer  
media_timers

# Graphics Features
// hd_textures          # disabled - not compiled into binary
vsync_toggle
lighting_blend

# Debug/Development Features (disabled by default)
// new_system_bug       # disabled - used for testing only
```

#### Build Process
1. **Automatic Generation**: Make reads `FEATURES.txt` and generates `hdr/feature_config.h`
2. **Dependency Tracking**: Changes to `FEATURES.txt` automatically trigger regeneration
3. **Preprocessor Definitions**: Each feature becomes a macro like `FEATURE_CONSOLE_PROTECTION`

```bash
# The build system does this automatically:
$ make debug
Generating feature configuration from features/FEATURES.txt...
Generated feature configuration with 9 features
  ✓ console_protection
  ✓ cl_maxfps_singleplayer  
  ✗ hd_textures (disabled)
```

#### Generated Configuration (`hdr/feature_config.h`)
```cpp
#pragma once

/*
    Auto-generated from features/FEATURES.txt
    DO NOT EDIT MANUALLY - Edit FEATURES.txt instead
*/

#define FEATURE_CONSOLE_PROTECTION      1  // enabled
#define FEATURE_CL_MAXFPS_SINGLEPLAYER  1  // enabled
#define FEATURE_HD_TEXTURES             0  // disabled
#define FEATURE_NEW_SYSTEM_BUG          0  // disabled
```

#### Usage in Feature Code
Every feature file should include the config and wrap its implementation:

```cpp
#include "../../../hdr/feature_config.h"

#if FEATURE_MY_FEATURE

#include "../../../hdr/feature_macro.h"
// ... other includes

// Register hooks - only compiled if feature is enabled
REGISTER_HOOK(MyFunction, 0x12345678, void, __cdecl, int param);

// Hook implementation - only compiled if feature is enabled
void hkMyFunction(int param) {
    // Feature logic here
    oMyFunction(param);
}

#endif // FEATURE_MY_FEATURE
```

#### Workflow Example
```bash
# 1. Disable a feature
$ vim features/FEATURES.txt
# Comment out: // hd_textures

# 2. Rebuild (automatically regenerates config)
$ make clean && make debug
Generating feature configuration from features/FEATURES.txt...
Generated feature configuration with 8 features
  ✗ hd_textures (now disabled)

# 3. Result: hd_textures code is NOT compiled into the DLL
# Binary size is smaller, no runtime overhead
```

### Benefits of Compile-time Toggles

| Benefit | Description |
|---------|-------------|
| **Zero Runtime Overhead** | Disabled features don't exist in the binary at all |
| **Smaller Binary Size** | Only enabled features consume space |
| **No Performance Impact** | No runtime checks or branching |
| **Clean Dependencies** | Pure Make-based, no Python/external tools |
| **Automatic Tracking** | Changes to `FEATURES.txt` trigger rebuild |
| **Version Control Friendly** | Simple text file for configuration |

### Developer Guidelines

#### Adding a New Feature
1. Create your feature folder: `src/features/my_feature/`
2. Wrap your `hooks.cpp` with feature guard:
   ```cpp
   #include "../../../hdr/feature_config.h"
   #if FEATURE_MY_FEATURE
   // ... your feature code
   #endif
   ```
3. Add feature name to `features/FEATURES.txt`
4. Build and test

#### Disabling Features for Release
```bash
# For a minimal release build, disable debug features:
# features/FEATURES.txt
console_protection      # Core security - keep enabled
cl_maxfps_singleplayer  # Core fix - keep enabled
// hd_textures          # Optional - disable for smaller binary
// new_system_bug       # Debug only - disable for release
```

#### Build System Integration
- **Clean builds**: `make clean` removes generated config
- **Incremental builds**: Only regenerates when `FEATURES.txt` changes
- **Cross-platform**: Works on any system with Make and basic shell tools
- **No external dependencies**: Pure Make + shell commands

## Available Macros

### Direct Hooks (single feature)
- `REGISTER_HOOK(name, addr, ret, conv, ...)` - Hook with parameters
- `REGISTER_HOOK_LEN(name, addr, len, ret, conv, ...)` - Hook with explicit detour length (bytes)
- `REGISTER_HOOK_VOID(name, addr, ret, conv)` - Hook without parameters  
- `REGISTER_MODULE_HOOK(name, module, proc, ret, conv, ...)` - Hook DLL export

### Shared Hooks (multiple features)
- `REGISTER_SHARED_HOOK_CALLBACK(hook_name, feature_name, callback_name, priority)` - Register callback
- `DISPATCH_SHARED_HOOK(hook_name)` - Call all registered callbacks (in main hook)

### Lifecycle Callbacks (initialization phases)
- `REGISTER_SHARED_HOOK_CALLBACK(EarlyStartup, feature_name, callback_name, priority)` - Early startup phase
- `REGISTER_SHARED_HOOK_CALLBACK(PreCvarInit, feature_name, callback_name, priority)` - Pre-CVar init phase  
- `REGISTER_SHARED_HOOK_CALLBACK(PostCvarInit, feature_name, callback_name, priority)` - Post-CVar init phase

### Module Loading Callbacks (DLL loading phases)
- `REGISTER_SHARED_HOOK_CALLBACK(RefDllLoaded, feature_name, callback_name, priority)` - When ref.dll loads
- `REGISTER_SHARED_HOOK_CALLBACK(GameDllLoaded, feature_name, callback_name, priority)` - When game.dll loads
- `REGISTER_SHARED_HOOK_CALLBACK(PlayerDllLoaded, feature_name, callback_name, priority)` - When player.dll loads

## Examples

### Direct Hooks (single feature)
- `godmode/` - Player invincibility
- `console_protection/` - Security features

#### Hook with explicit detour length
```cpp
#include "../../../hdr/feature_macro.h"

// Creates a detour using 6 bytes at the original address
REGISTER_HOOK_LEN(MyFunction, 0x30003040, 6, void, __cdecl, int param) {
    // Your hook logic here
    oMyFunction(param);
}
```

### Shared Hooks (multiple features)
- `hud_manager/` - Coordinates HUD rendering (main dispatcher)
- `weapon_stats/` - Adds weapon info to HUD (callback)
- `enhanced_hud/` - Custom HUD elements (callback)

### Lifecycle Callbacks
- `config_manager/` - Demonstrates all three lifecycle phases

## When to use what?

**Direct hooks**: When only your feature needs the hook
**Shared hooks**: When multiple features need the same hook point  
**Lifecycle callbacks**: When your feature needs initialization at specific engine phases
**Module loading callbacks**: When your feature needs to hook DLL functions (0x5XXXXXXX addresses)

**Note about registering module hooks vs shared callbacks**: If your feature only needs to create detours (i.e. apply a hook directly to a known address or module export) you should use `REGISTER_MODULE_HOOK` (or `REGISTER_HOOK`/`REGISTER_HOOK_VOID`) directly in your feature. Use `REGISTER_SHARED_HOOK_CALLBACK` with the `GameDllLoaded` (or the appropriate `*DllLoaded`) lifecycle callback only when you need to perform additional initialization beyond simply creating detours — for example, when your feature must wait for the target module to be loaded before locating addresses, or when it needs to interact with other subsystems during module-load time. Registering a shared callback for module loading is intended for features that need the module-load ordering and coordination, not for plain detour creation.

## How Shared Hooks Work

### The Problem
Multiple features often need the same hook point (like HUD rendering). With direct hooks, only one feature can hook a function. Shared hooks solve this by using a **registration/dispatcher pattern**.

### The Solution: Registration + Dispatcher

1. **One main dispatcher** hooks the original function
2. **Multiple features register callbacks** for that hook
3. **Dispatcher calls all callbacks** in priority order

### Complete Example

**Step 1: Main Dispatcher** (`hud_manager/hooks.cpp`)
```cpp
#include "../../../hdr/feature_macro.h"
#include "../../../hdr/shared_hook_manager.h"

// Main hook - this replaces the original function
REGISTER_HOOK_VOID(RenderHUD, 0x140234560, void, __cdecl) {
    oRenderHUD();  // Call original first
    DISPATCH_SHARED_HOOK(RenderHUD);  // Then call all registered callbacks
}
```

**Step 2: Feature Callbacks** (`weapon_stats/hooks.cpp`)
```cpp
#include "../../../hdr/shared_hook_manager.h"

static void my_hud_callback() {
    // Draw weapon stats overlay
    PrintOut(PRINT_LOG, "Drawing weapon stats\n");
}

// Register callback with priority 75
REGISTER_SHARED_HOOK_CALLBACK(RenderHUD, weapon_stats, my_hud_callback, 75);
```

**Step 3: More Features** (`health_display/hooks.cpp`)
```cpp
static void draw_health() {
    // Draw health bar
}

// Higher priority = runs first
REGISTER_SHARED_HOOK_CALLBACK(RenderHUD, health_display, draw_health, 100);
```

### Execution Flow
1. Game calls `RenderHUD()`
2. Our dispatcher `hkRenderHUD()` runs
3. Calls `oRenderHUD()` (original function)
4. Calls `DISPATCH_SHARED_HOOK(RenderHUD)` which:
   - Calls `draw_health()` (priority 100)
   - Calls `my_hud_callback()` (priority 75)

### Priority System

```cpp
// Execution order (higher priority = earlier execution):
REGISTER_SHARED_HOOK_CALLBACK(RenderHUD, setup,     pre_render,  100);  // Runs 1st
REGISTER_SHARED_HOOK_CALLBACK(RenderHUD, health,    draw_health,  90);  // Runs 2nd  
REGISTER_SHARED_HOOK_CALLBACK(RenderHUD, weapons,   draw_ammo,    80);  // Runs 3rd
REGISTER_SHARED_HOOK_CALLBACK(RenderHUD, minimap,   draw_map,     70);  // Runs 4th
REGISTER_SHARED_HOOK_CALLBACK(RenderHUD, cleanup,   post_render, -10);  // Runs last
```

### Runtime Control

```cpp
// Disable a feature's callback
SharedHookManager::Instance().EnableCallback("RenderHUD", "weapon_stats", false);

// Re-enable it
SharedHookManager::Instance().EnableCallback("RenderHUD", "weapon_stats", true);

// Debug: See all registered callbacks
SharedHookManager::Instance().PrintHookInfo("RenderHUD");
```

### Best Practices

**Priority Guidelines:**
- **100+**: Setup/initialization 
- **50-99**: Critical elements (health, ammo)
- **0-49**: Secondary elements (minimap, stats)
- **-50 to -1**: Overlays (crosshair, notifications)  
- **-100+**: Cleanup/finalization

**Naming:**
- Hook name: `RenderHUD` (what function you're hooking)
- Feature name: `weapon_stats` (your feature's folder name)
- Callback name: `draw_ammo` (what your callback does)

**Error Handling:**
The system is robust - if one callback crashes, others still run.

## Lifecycle Callback System

The lifecycle system provides **three initialization phases** where features can register callbacks. This ensures features initialize in the correct order and have access to the right systems.

### Initialization Phases

| Phase | When | Purpose | What's Available |
|-------|------|---------|-----------------|
| **EarlyStartup** | Right after Winsock init | Initialize critical systems | Basic hooking, early logging |
| **PreCvarInit** | After filesystem, before CVars | Setup requiring filesystem | File system, directory paths, Com_Printf |  
| **PostCvarInit** | After all CVars created | Configuration and user settings | All CVars, complete command system |

### Module Loading Phases

| Phase | When | Purpose | Address Range |
|-------|------|---------|---------------|
| **RefDllLoaded** | After ref.dll loads | Hook graphics/rendering functions | `0x5XXXXXXX` (ref.dll) |
| **GameDllLoaded** | After game.dll loads | Hook game logic functions | `0x5XXXXXXX` (game.dll) |
| **PlayerDllLoaded** | After player.dll loads | Hook player-specific functions | `0x5XXXXXXX` (player.dll) |

**⚠️ Important Timing Note**: Module loading phases (RefDllLoaded, GameDllLoaded, PlayerDllLoaded) occur **before** the PostCvarInit phase. This means that any CVars created in PostCvarInit callbacks will be **NULL** when accessed from module loading callbacks. Plan your feature initialization accordingly!

### Example Usage

```cpp
#include "../../../hdr/shared_hook_manager.h"

// Register for initialization phases
REGISTER_SHARED_HOOK_CALLBACK(EarlyStartup, my_feature, early_init, 80);
REGISTER_SHARED_HOOK_CALLBACK(PreCvarInit, my_feature, setup_paths, 50);
REGISTER_SHARED_HOOK_CALLBACK(PostCvarInit, my_feature, load_config, 60);

// Register for module loading phases
REGISTER_SHARED_HOOK_CALLBACK(RefDllLoaded, my_feature, setup_ref_hooks, 70);
REGISTER_SHARED_HOOK_CALLBACK(GameDllLoaded, my_feature, setup_game_hooks, 60);

static void early_init() {
    // Initialize memory pools, basic structures
}

static void setup_paths() {
    // Create config directories, setup file paths
}

static void load_config() {
    // Load user configs, register commands
}

static void setup_ref_hooks() {
    // Apply detours to ref.dll functions (0x5XXXXXXX)
    // DetourCreate((void*)0x50075190, &my_ref_function, DETOUR_TYPE_JMP, 7);
    
    // ⚠️ WARNING: CVars created in PostCvarInit are NULL here!
    // If you need CVar values, either:
    // 1. Create CVars in PreCvarInit instead, or
    // 2. Use default values and let PostCvarInit update them later
}

static void setup_game_hooks() {
    // Apply detours to game.dll functions (0x5XXXXXXX)
    // DetourCreate((void*)0x50123456, &my_game_function, DETOUR_TYPE_JMP, 5);
    
    // ⚠️ WARNING: CVars created in PostCvarInit are NULL here!
}
```

### Priority System

Higher numbers = higher priority = run first:
- **100+**: Critical system initialization
- **50-99**: Normal feature initialization  
- **1-49**: User configuration and setup
- **0**: Cleanup and finalization

See `config_manager/` for a complete example of lifecycle callbacks!

**Note**: Module loading infrastructure is provided by `src/module_loaders.cpp` (core system, not a feature).

## Common Pitfalls: CVar Timing Issues

### The Problem
Many developers get caught by the timing between module loading and CVar initialization:

```cpp
// ❌ WRONG: This will crash with NULL pointer access
static void my_RefDllLoaded() {
    // CVars are NULL here because PostCvarInit hasn't run yet!
    if (my_cvar->value > 0) {  // CRASH: my_cvar is NULL
        // ...
    }
}

static void my_PostCvarInit() {
    // CVars are created here
    my_cvar = orig_Cvar_Get("my_cvar", "1", 0, NULL);
}
```

### Solutions

#### Solution 1: Create CVars in PreCvarInit
```cpp
// ✅ CORRECT: Create CVars before module loading
static void my_PreCvarInit() {
    my_cvar = orig_Cvar_Get("my_cvar", "1", 0, NULL);
}

static void my_RefDllLoaded() {
    // Now my_cvar is available
    if (my_cvar->value > 0) {
        // ...
    }
}
```

#### Solution 2: Use Default Values in Module Loading
```cpp
// ✅ CORRECT: Use defaults, update later
static void my_RefDllLoaded() {
    // Use default values, don't access CVars
    int default_value = 1;
    apply_settings(default_value);
}

static void my_PostCvarInit() {
    my_cvar = orig_Cvar_Get("my_cvar", "1", 0, my_change_callback);
    // Change callback will update settings when CVar changes
}

static void my_change_callback(cvar_t* cvar) {
    apply_settings(cvar->value);
}
```

#### Solution 3: Defer CVar-Dependent Logic
```cpp
// ✅ CORRECT: Do basic setup, defer CVar-dependent work
static void my_RefDllLoaded() {
    // Apply detours, but don't use CVar values yet
    DetourCreate((void*)0x50075190, &my_hook, DETOUR_TYPE_JMP, 7);
}

static void my_PostCvarInit() {
    my_cvar = orig_Cvar_Get("my_cvar", "1", 0, my_change_callback);
    // Now apply initial CVar-based settings
    my_change_callback(my_cvar);
}
```

### Real Example: lighting_blend Feature
The `lighting_blend` feature demonstrates this issue. In `RefDllLoaded`, it calls:
```cpp
lightblend_change(_sofbuddy_lightblend_src);  // CVar is NULL!
lightblend_change(_sofbuddy_lightblend_dst);  // CVar is NULL!
```

But the CVars are only created in `PostCvarInit`. The comment "This shouldn't be necessary" suggests the developer was aware of the timing issue.

## Future Architecture: Module-Based Dispatchers

Currently, shared hook dispatchers are in individual feature folders (like `hud_manager/`). A future improvement could organize dispatchers by their **target module**:

### **Proposed Structure**
```
dispatchers/
├── exe.cpp         # Dispatchers for main SoF.exe functions
├── ref_gl.cpp      # Dispatchers for ref_gl.dll functions  
├── gamex86.cpp     # Dispatchers for gamex86.dll functions
└── README.md       # Module memory mapping documentation
```

### **Benefits**
- **Clear module mapping**: Know exactly which DLL/exe each function belongs to
- **Address organization**: Group functions by memory space/module
- **Separation of concerns**: 
  - `dispatchers/` = "where to hook" (addresses & dispatching)
  - `features/` = "what to do" (business logic & callbacks)
- **Easier debugging**: Quickly find which module a function lives in

### **Example**

**`dispatchers/exe.cpp`** - Main executable dispatchers
```cpp
// Main game loop (SoF.exe @ 0x20066412)
REGISTER_HOOK_VOID(GameMainLoop, 0x20066412, void, __cdecl) {
    oGameMainLoop();
    DISPATCH_SHARED_HOOK(GameMainLoop);
}

// Player update (SoF.exe @ 0x20123456) 
REGISTER_HOOK_VOID(PlayerUpdate, 0x20123456, void, __cdecl, float deltaTime) {
    oPlayerUpdate(deltaTime);
    DISPATCH_SHARED_HOOK(PlayerUpdate);
}
```

**`dispatchers/ref_gl.cpp`** - Renderer DLL dispatchers
```cpp
// HUD rendering (ref_gl.dll @ 0x140234560)
REGISTER_HOOK_VOID(RenderHUD, 0x140234560, void, __cdecl) {
    oRenderHUD();
    DISPATCH_SHARED_HOOK(RenderHUD);
}

// Scene rendering (ref_gl.dll @ 0x140567890)
REGISTER_HOOK_VOID(RenderScene, 0x140567890, void, __cdecl) {
    DISPATCH_SHARED_HOOK(PreRenderScene);
    oRenderScene();
    DISPATCH_SHARED_HOOK(PostRenderScene);
}
```

**`features/weapon_stats/hooks.cpp`** - Pure business logic
```cpp
// Just register callbacks - no addresses needed!
REGISTER_SHARED_HOOK_CALLBACK(RenderHUD, weapon_stats, render_weapon_info, 75);
REGISTER_SHARED_HOOK_CALLBACK(PlayerUpdate, weapon_stats, update_weapon_stats, 50);
```

### **Migration Strategy**
1. Keep current system working
2. Gradually move dispatchers from feature folders to `dispatchers/`
3. Features keep only their callback registrations
4. Update documentation with module memory maps

This would create a **clean separation** between "hooking infrastructure" and "feature logic"!

Copy any folder as a template!
