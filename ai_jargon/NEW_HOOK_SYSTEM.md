# New Simple Hook System

This project has been refactored to use a much simpler, more elegant hook system that eliminates the need for Python scripts and complex registration flows.

## How It Works

Instead of the previous complex system with Python scripts, detour manifests, and registration callbacks, everything now works with simple macros that auto-register hooks at compile time.

### Folder Structure

Each feature gets its own folder with clean separation:

```
src/features/
├── godmode/
│   ├── hooks.cpp          # Clean code using REGISTER_HOOK macros
│   └── README.md          # Detailed documentation
├── enhanced_hud/
│   ├── hooks.cpp          # HUD hooks
│   └── README.md          # Feature docs
└── console_protection/
    ├── hooks.cpp          # Security hooks
    └── README.md          # Protection details
```

### Basic Usage

Create a feature folder and add clean hook code:

**`src/features/my_feature/hooks.cpp`**
```cpp
#include "../../../hdr/feature_macro.h"

// Hook a function with parameters
REGISTER_HOOK(UpdatePlayerHealth, 0x140123450, void, __fastcall, int playerId, int delta) {
    if (delta < 0) delta = 0; // Prevent damage
    oUpdatePlayerHealth(playerId, delta);
}

// Hook without parameters  
REGISTER_HOOK_VOID(RenderHud, 0x140234560, void, __cdecl) {
    oRenderHud();
    // Add custom rendering
}

// Hook module export
REGISTER_MODULE_HOOK(GetClipboardData, "user32.dll", "GetClipboardDataA", HANDLE, __stdcall, UINT uFormat) {
    return oGetClipboardData(uFormat);
}
```

**`src/features/my_feature/README.md`**
```markdown
# My Feature

## Overview
Description of what this feature does...

## Technical Details
- Memory addresses, CVars, configuration
- Hook explanations and purposes
- Usage examples and troubleshooting
```

### What the Macros Do

1. **Define the function typedef** (e.g., `tUpdatePlayerHealth`)
2. **Create original function pointer** (e.g., `oUpdatePlayerHealth`)
3. **Auto-register the hook** via static constructor
4. **Provide hook function signature** for you to implement

### Available Macros

- `REGISTER_HOOK(name, addr, ret, conv, ...)` - Hook with parameters
- `REGISTER_HOOK_VOID(name, addr, ret, conv)` - Hook without parameters  
- `REGISTER_MODULE_HOOK(name, module, proc, ret, conv, ...)` - Hook DLL export

### Parameters

- **name**: Unique identifier for the hook
- **addr**: Memory address to hook (for PTR hooks)
- **ret**: Return type (void, int, etc.)
- **conv**: Calling convention (__cdecl, __stdcall, __fastcall)
- **module**: DLL name (for module hooks)
- **proc**: Export name (for module hooks)
- **...**: Function parameters

## Benefits

1. **Zero Python dependencies** - No more build scripts
2. **Folder per feature** - Clean organization + documentation
3. **Auto-registration** - Hooks register themselves at startup
4. **Type safety** - Compiler catches mistakes
5. **Simple debugging** - Easy to see what hooks are active
6. **Rich documentation** - Separate README.md files
7. **Clean code files** - Only hook logic, no embedded docs

## Migration from Old System

The old system required:
- Python script execution
- Detour manifest definitions
- Registration callbacks
- Multiple files per feature

The new system only requires:
- Feature folder: `mkdir src/features/my_feature`
- Hook file: `hooks.cpp` with `#include "../../../hdr/feature_macro.h"`
- `REGISTER_HOOK` macro calls
- Documentation: `README.md` with feature details

## Examples

See the example features:
- `src/features/godmode/` - Player invincibility
- `src/features/enhanced_hud/` - Custom HUD elements  
- `src/features/console_protection/` - Security features

Each folder contains:
- **`hooks.cpp`** - Clean hook implementation
- **`README.md`** - Comprehensive documentation

Copy any folder as a template for new features!
