# SoF Buddy Architecture: Detours & Features in Harmony

## TLDR

**SoF Buddy is a data-driven hooking framework where detours (function interceptors) and features (modular enhancements) work together through a build-time code generation system.**

- **Detours** = Function interceptors defined in `detours.yaml` (the registry)
- **Features** = Modular enhancements in `src/features/` that hook functions via `hooks.json` and lifecycle events via `callbacks.json`
- **Harmony** = Build system generates type-safe hook code only for used detours, enabling multiple features to safely hook the same functions

---

## The Two-Layer System

### Layer 1: Detours (The Foundation)

**What:** Detours are function interceptors that patch game code to redirect function calls.

**Where:** Defined in `detours.yaml` - the central registry of all hookable functions.

**Structure:**
```yaml
functions:
  - name: Draw_Pic
    module: RefDll
    identifier: "0x00001ED0"
    return_type: void
    calling_convention: __cdecl
    params: [...]
```

**Key Points:**
- `detours.yaml` is the **single source of truth** for all hookable functions
- Defines function signatures, addresses, modules, calling conventions
- Not all detours are generated - only those actually used by features
- Detours are applied at runtime when modules load (SoF.exe, ref.dll, game.dll, etc.)

### Layer 2: Features (The Enhancements)

**What:** Features are modular enhancements that hook functions and respond to lifecycle events.

**Where:** Each feature lives in `src/features/<feature_name>/`

**Structure:**
```
src/features/my_feature/
├── hooks/
│   ├── hooks.json          # Which functions to hook
│   └── draw_pic.cpp        # Hook implementation
└── callbacks/
    ├── callbacks.json      # Which lifecycle events to listen to
    └── my_feature_init.cpp # Lifecycle callback implementation
```

**Key Points:**
- Features are enabled/disabled in `features/FEATURES.txt`
- Each feature can hook multiple functions and listen to multiple lifecycle events
- Multiple features can hook the same function (they execute in priority order)
- Features are isolated - they don't know about each other

---

## How They Work Together

### Build-Time Flow

```
1. detours.yaml (registry)
   ↓
2. features/FEATURES.txt (enabled features)
   ↓
3. tools/generate_hooks.py scans:
   - All hooks.json files (which functions features want to hook)
   - All callbacks.json files (which lifecycle events features want)
   - All source files (for o* function pointer usage)
   ↓
4. Generates build/generated_detours.h:
   - Type-safe hook managers for each used detour
   - Original function pointers (oDraw_Pic, etc.)
   - Hook functions (hkDraw_Pic, etc.)
   ↓
5. Generates build/generated_registrations.h:
   - RegisterAllFeatureHooks() function
   - Registers all callbacks from hooks.json and callbacks.json
```

**Critical Insight:** Only detours that are actually used get generated. If `detours.yaml` has 100 functions but only 20 are hooked, only those 20 are generated. This keeps the binary small and startup fast.

### Runtime Flow

```
1. DllMain() → lifecycle_EarlyStartup()
   ↓
2. ProcessDeferredRegistrations()
   - Applies detours that were registered during static initialization
   ↓
3. RegisterAllFeatureHooks()
   - Registers all callbacks from hooks.json and callbacks.json
   - Callbacks are registered but not executed yet
   ↓
4. ApplyExeDetours() / ApplySystemDetours()
   - Patches SoF.exe and system DLL functions
   ↓
5. Game starts calling functions...
   ↓
6. When Draw_Pic() is called:
   - Generated hook function hkDraw_Pic() intercepts
   - Calls manager.DispatchPre() → all Pre callbacks execute
   - Calls original function oDraw_Pic()
   - Calls manager.DispatchPost() → all Post callbacks execute
   ↓
7. When ref.dll loads:
   - VID_LoadRefresh override hook calls original
   - Calls DISPATCH_SHARED_HOOK(RefDllLoaded, Post)
   - SharedHookManager dispatches all registered RefDllLoaded callbacks
```

---

## Two Types of Hooks

### 1. Function Hooks (hooks.json)

**Purpose:** Intercept specific game functions.

**Declaration:**
```json
{
  "function": "Draw_Pic",
  "callback": "my_draw_pic_callback",
  "phase": "Post",
  "priority": 50
}
```

**Execution:**
- Pre callbacks: Execute before original function, can modify parameters
- Post callbacks: Execute after original function, can modify return value
- Multiple features can hook the same function (priority determines order)

**Override Hooks:**
- Set `"override": true` for full control
- Only one override per function across all features
- You control when/if original function is called

### 2. Lifecycle Hooks (callbacks.json)

**Purpose:** Respond to game lifecycle events (module loads, initialization phases).

**Declaration:**
```json
{
  "hook": "RefDllLoaded",
  "callback": "my_feature_refdll_loaded",
  "priority": 50,
  "phase": "Post"
}
```

**Execution:**
- Dispatched by core override hooks via `DISPATCH_SHARED_HOOK(hook_name, phase)`
- Available hooks: `EarlyStartup`, `PreCvarInit`, `PostCvarInit`, `RefDllLoaded`, `GameDllLoaded`
- Multiple features can listen to the same event (priority determines order)

**Key Insight:** Lifecycle hooks are not automatic - they must be dispatched by core override hooks. Core hooks (in `src/core/hooks.json`) manually call `DISPATCH_SHARED_HOOK` to trigger lifecycle callbacks.

---

## Module Loading Harmony

Detours are applied when their target modules load:

- **SoF.exe detours:** Applied in `lifecycle_EarlyStartup()` (addresses like `0x200xxxxx`)
- **System DLL detours:** Applied in `lifecycle_EarlyStartup()` (Windows API functions)
- **ref.dll detours:** Applied in `VID_LoadRefresh` override hook (when renderer loads)
- **game.dll detours:** Applied in `Sys_GetGameApi` override hook (when game DLL loads)

This ensures detours are only applied when their target modules are actually loaded and accessible.

---

## Feature Enablement

Features are controlled by `features/FEATURES.txt`:

```
# Enabled
media_timers
scaled_con

# Disabled
// scaled_hud
```

**Build System:**
- Only enabled features have their `hooks.json` and `callbacks.json` processed
- `build/feature_config.h` is generated with `#define FEATURE_XXX 1/0`
- Source files use `#if FEATURE_XXX` to conditionally compile feature code

**Runtime:**
- Disabled features are completely excluded from the binary
- No runtime overhead for disabled features

---

## Type Safety & Code Generation

The build system generates type-safe code:

**Generated Namespace Structure:**
```cpp
namespace detour_Draw_Pic {
    using tDraw_Pic = void(__cdecl*)(int x, int y, char const* imgname, int palette);
    extern tDraw_Pic oDraw_Pic;
    TypedSharedHookManager<void, int, int, char const*, int> manager;
    void hkDraw_Pic(int x, int y, char const* imgname, int palette);
}
```

**Usage in Features:**
```cpp
#include "generated_detours.h"

void my_draw_pic_callback(int x, int y, char const* imgname, int palette) {
    // Type-safe callback - compiler enforces correct signature
}
```

**Key Benefit:** Type mismatches are caught at compile time, not runtime.

---

## Priority System

Both function hooks and lifecycle hooks use priorities:

- **Higher priority = executes first**
- **Priority 100+:** Critical system initialization
- **Priority 50-99:** Normal feature initialization
- **Priority 1-49:** User configuration
- **Priority 0:** Cleanup and finalization

**Example:** If Feature A (priority 100) and Feature B (priority 50) both hook `Draw_Pic`:
1. Feature A's Pre callback executes
2. Feature B's Pre callback executes
3. Original `Draw_Pic` executes
4. Feature B's Post callback executes
5. Feature A's Post callback executes

---

## The Harmony Principle

**Detours and features work in harmony because:**

1. **Separation of Concerns:** Detours define *what* can be hooked, features define *how* to hook it
2. **No Conflicts:** Multiple features can hook the same function safely via priority system
3. **Type Safety:** Build system generates type-safe code, preventing runtime errors
4. **Efficiency:** Only used detours are generated, keeping binary size small
5. **Modularity:** Features are isolated - enable/disable without affecting others
6. **Lifecycle Coordination:** Core hooks dispatch lifecycle events, features respond to them

**The result:** A clean, maintainable system where features can be added/removed without breaking each other, and the build system ensures everything is type-safe and efficient.

---

## Base Features: scaled_ui_base

**scaled_ui_base** is a special infrastructure feature that provides shared functionality for multiple related features. It demonstrates how features can work together through shared infrastructure.

### Purpose

`scaled_ui_base` provides common infrastructure for all scaled UI features:
- `scaled_con` (console scaling)
- `scaled_hud` (HUD scaling)
- `scaled_text` (text scaling)
- `scaled_menu` (menu scaling)

### Automatic Compilation

The base feature automatically compiles when **any** of its sub-features are enabled:

```cpp
#if FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_TEXT || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE
// Base infrastructure code
#endif
```

This ensures the base infrastructure is always available when needed, without requiring explicit enablement.

### Shared Infrastructure

**Global State:**
- `g_activeDrawCall` - Tracks current drawing routine (StretchPic, Pic, PicOptions, etc.)
- `g_currentPicCaller` - Identifies which function called `Draw_Pic`
- `g_currentStretchPicCaller` - Identifies which function called `Draw_StretchPic`
- `DrawPicWidth`, `DrawPicHeight` - Image dimensions extracted from `GL_FindImage`
- `screen_y_scale`, `current_vid_w`, `current_vid_h` - Screen dimensions and scaling factors

**Shared Hooks:**
- `Draw_StretchPic` (override) - Shared hook for console background and HUD CTF flag scaling
- `Draw_Pic` (override) - Shared hook for menu background tiling and pic caller detection
- `glVertex2f` (custom detour) - Central vertex scaling hook used by all scaled UI features
- `GL_FindImage` (Post, priority 100) - Extracts image dimensions for menu scaling
- `VID_CheckChanges` (Post, priority 100) - Updates screen dimensions when resolution changes

**Custom Detours:**
- `glVertex2f` is installed at runtime in `scaledUIBase_RefDllLoaded()` callback
- Address: `ref.dll + 0x000A4670`
- Handles vertex coordinate scaling for console, HUD, text, and menu elements

### Architecture Pattern

Base features follow this pattern:
1. **Shared headers** (`shared.h`) - Common declarations, enums, global variables
2. **Shared hooks** - Override hooks that multiple sub-features use
3. **Caller detection** - Uses RVA parent tracing to identify call sites (see below)
4. **CVars** - Shared configuration variables (only compiled when sub-features are enabled)

**Key Insight:** Base features enable code reuse and shared state management across related features, while maintaining feature isolation at the build system level.

---

## RVA Function Parent Tracing

**RVA (Relative Virtual Address) Function Parent Tracing** is a sophisticated system that identifies which function called a hook, enabling context-aware behavior.

### The Problem

When a hook intercepts a function like `Draw_Pic()`, it needs to know **which function called it** to adapt its behavior:
- If called from `SCR_DrawCrosshair`, scale the crosshair
- If called from `ExecuteLayoutString`, scale menu elements
- If called from `NetworkDisconnectIcon`, don't scale

### The Solution: Three-Layer System

#### Layer 1: CallsiteClassifier

**Purpose:** Converts return addresses into function start RVAs.

**How it works:**
1. Hook captures return address (RVA within calling module)
2. Looks up return address in funcmap JSON files (`rsrc/funcmaps/*.json`)
3. Finds containing function's start RVA
4. Returns `CallerInfo` with module, RVA, and function name

**Funcmaps:**
- JSON files containing function start RVAs for each module
- Located in `rsrc/funcmaps/` (SoF.exe.json, ref_gl.dll.json, etc.)
- Generated by `tools/pe_funcmap_gen` from PE binaries
- Required at runtime for caller identification

**Example:**
```cpp
CallerInfo info;
if (CallsiteClassifier::classify(returnAddress, info)) {
    // info.module = Module::SofExe
    // info.functionStartRva = 0x00014510
    // info.name = "ExecuteLayoutString" (if available)
}
```

#### Layer 2: ParentRecorder

**Purpose:** Records which parent functions call each hooked function (debug builds only).

**How it works:**
1. Hook calls `HookCallsite::recordAndGetFnStartExternal("Draw_Pic")`
2. Walks up stack to find first external caller (outside sof_buddy.dll)
3. Records (module, fnStartRva) pair for the child hook
4. Writes JSON files to `sof_buddy/func_parents/*.json` on shutdown

**Output Format:**
```json
{
  "child": "Draw_Pic",
  "parents": [
    { "module": "SoF.exe", "fnStart": 83216, "fnStartHex": "0x00014510" },
    { "module": "SoF.exe", "fnStart": 89504, "fnStartHex": "0x00015DA0" }
  ]
}
```

**Usage:**
- Debug builds only (compiled out in release)
- Helps developers discover all call sites for a hook
- Files can be manually copied to `rsrc/func_parents/` for reference
- Used by `rsrc/update_callers_from_parents.py` to update caller detection code

#### Layer 3: Caller Detection (caller_from.cpp)

**Purpose:** Maps function start RVAs to semantic caller types.

**How it works:**
1. Hook gets function start RVA via `recordAndGetFnStartExternal()`
2. Looks up RVA in hardcoded switch statements
3. Returns semantic enum (e.g., `PicCaller::SCR_DrawCrosshair`)

**Example:**
```cpp
uint32_t fnStart = HookCallsite::recordAndGetFnStartExternal("Draw_Pic");
if (fnStart) {
    PicCaller caller = getPicCallerFromRva(fnStart);
    switch (caller) {
        case PicCaller::SCR_DrawCrosshair:
            // Scale crosshair
            break;
        case PicCaller::ExecuteLayoutString:
            // Scale menu elements
            break;
    }
}
```

**Caller Types:**
- `PicCaller` - Who called `Draw_Pic`
- `StretchPicCaller` - Who called `Draw_StretchPic`
- `PicOptionsCaller` - Who called `Draw_PicOptions`
- `FontCaller` - Who called `R_DrawFont`
- `VertexCaller` - Who called `glVertex2f`

### Complete Flow Example

**When `Draw_Pic()` is called from `SCR_DrawCrosshair`:**

```
1. hkDraw_Pic() intercepts
   ↓
2. HookCallsite::recordAndGetFnStartExternal("Draw_Pic")
   - Captures return address: 0x20015DA0
   - CallsiteClassifier identifies: SoF.exe, fnStart = 0x00015DA0
   - ParentRecorder records: (SoF.exe, 0x00015DA0) → Draw_Pic
   ↓
3. getPicCallerFromRva(0x00015DA0)
   - Looks up in switch statement
   - Returns: PicCaller::SCR_DrawCrosshair
   ↓
4. Hook adapts behavior:
   - Sets g_currentPicCaller = PicCaller::SCR_DrawCrosshair
   - Scales crosshair appropriately
   ↓
5. Calls original oDraw_Pic()
```

### External Caller Detection

**Why `recordAndGetFnStartExternal()`?**

When sof_buddy.dll functions call each other internally, you need the **first external caller** (from SoF.exe, ref.dll, etc.), not the immediate caller within sof_buddy.dll.

**Example:**
```
SoF.exe::SCR_DrawCrosshair()
  → sof_buddy.dll::helper_function()
    → sof_buddy.dll::hkDraw_Pic()
```

`recordAndGetFnStartExternal()` walks up the stack to find `SCR_DrawCrosshair`, not `helper_function`.

### Integration with scaled_ui_base

The scaled_ui_base feature uses RVA parent tracing extensively:

1. **Draw_Pic hook** - Identifies callers to determine scaling behavior
2. **Draw_StretchPic hook** - Identifies console vs HUD rendering
3. **glVertex2f hook** - Identifies drawing context (char, stretchpic, line)
4. **R_DrawFont hook** - Identifies font rendering context

**Key Benefit:** Context-aware scaling - the same hook can behave differently based on who called it, enabling precise UI scaling without breaking game functionality.

---

## Quick Reference

**To add a new feature:**
1. Add to `features/FEATURES.txt`
2. Create `src/features/my_feature/hooks/hooks.json` (if hooking functions)
3. Create `src/features/my_feature/callbacks/callbacks.json` (if listening to lifecycle)
4. Implement hook/callback files
5. Rebuild: `make clean && make BUILD=debug`

**To hook a new function:**
1. Add function to `detours.yaml`
2. Add hook declaration to feature's `hooks/hooks.json`
3. Implement hook callback in `hooks/<function_name>.cpp`
4. Rebuild

**To listen to a lifecycle event:**
1. Add callback declaration to feature's `callbacks/callbacks.json`
2. Implement callback in `callbacks/<callback_name>.cpp`
3. Rebuild

**Key Files:**
- `detours.yaml` - Function registry
- `features/FEATURES.txt` - Feature enablement
- `src/features/*/hooks/hooks.json` - Function hook declarations
- `src/features/*/callbacks/callbacks.json` - Lifecycle hook declarations
- `build/generated_detours.h` - Generated hook code
- `build/generated_registrations.h` - Generated registration code
- `rsrc/funcmaps/*.json` - Function maps for RVA parent tracing
- `rsrc/func_parents/*.json` - Recorded parent callers (debug builds)

**Advanced Topics:**
- See **Base Features: scaled_ui_base** section for shared infrastructure patterns
- See **RVA Function Parent Tracing** section for context-aware hooking

