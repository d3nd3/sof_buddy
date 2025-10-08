# Lighting Blend Mode

## Overview
Allows customization of OpenGL blend modes used for lighting in the ref.dll renderer. This feature patches memory locations that control the source and destination blend factors for `glBlendFunc()`.

## What It Does
- Modifies OpenGL blend function parameters used in the lighting pass
- Provides CVars to change blend modes at runtime
- Applies changes directly to ref.dll memory after it loads

## Technical Details

### Memory Addresses (ref.dll)
- **Source blend factor**: `0x300A4610` - Default: `GL_ZERO`
- **Destination blend factor**: `0x300A43FC` - Default: `GL_SRC_COLOR`

### Default Behavior
The SoF engine uses:
```c
glBlendFunc(GL_ZERO, GL_SRC_COLOR);
```

This is equivalent to: `Result = DST * SRC_COLOR + SRC * 0`

### Architecture
- Uses `RefDllLoaded` lifecycle callback to patch memory after ref.dll loads
- Registers with priority 60 (normal feature initialization)
- Applies settings both on startup and when CVars change

## Console Commands

### CVars
- `_sofbuddy_lightblend_src` - Source blend factor (default: `"GL_ZERO"`)
- `_sofbuddy_lightblend_dst` - Destination blend factor (default: `"GL_SRC_COLOR"`)

Both CVars are `CVAR_ARCHIVE` (saved to config.cfg).

### Valid Values
- `GL_ZERO`
- `GL_ONE`
- `GL_SRC_COLOR`
- `GL_ONE_MINUS_SRC_COLOR`
- `GL_DST_COLOR`
- `GL_ONE_MINUS_DST_COLOR`
- `GL_SRC_ALPHA`
- `GL_ONE_MINUS_SRC_ALPHA`
- `GL_DST_ALPHA`
- `GL_ONE_MINUS_DST_ALPHA`
- `GL_CONSTANT_COLOR`
- `GL_ONE_MINUS_CONSTANT_COLOR`
- `GL_CONSTANT_ALPHA`
- `GL_ONE_MINUS_CONSTANT_ALPHA`
- `GL_SRC_ALPHA_SATURATE`

### Example Usage
```
// Default (vanilla SoF lighting)
_sofbuddy_lightblend_src "GL_ZERO"
_sofbuddy_lightblend_dst "GL_SRC_COLOR"

// Additive lighting (brighter)
_sofbuddy_lightblend_src "GL_ONE"
_sofbuddy_lightblend_dst "GL_ONE"

// Alpha blending
_sofbuddy_lightblend_src "GL_SRC_ALPHA"
_sofbuddy_lightblend_dst "GL_ONE_MINUS_SRC_ALPHA"
```

## Implementation Flow

### 1. Startup (Module Loading)
```
SoF loads ref.dll
  → module_loaders.cpp detects ref.dll loading
  → InitializeRefHooks() is called
  → DISPATCH_SHARED_HOOK(RefDllLoaded) fires
  → lightblend_RefDllLoaded() callback runs
  → lightblend_ApplySettings() patches memory
  → Blend modes are active
```

### 2. Runtime (CVar Changes)
```
User types: _sofbuddy_lightblend_src "GL_ONE"
  → Engine calls lightblend_change() (registered in cvars.cpp)
  → Function parses string to GL constant
  → Updates lightblend_src variable
  → Calls lightblend_ApplySettings()
  → Patches ref.dll memory at 0x300A4610
  → New blend mode takes effect immediately
```

## OpenGL Reference

### Blend Function Equation
```
Final_Color = (Src * SrcFactor) + (Dst * DstFactor)
```

Where:
- **Src** = Incoming fragment (light)
- **Dst** = Existing framebuffer pixel (texture)
- **SrcFactor** = `_sofbuddy_lightblend_src`
- **DstFactor** = `_sofbuddy_lightblend_dst`

### Common Blend Modes

| Mode | Source | Dest | Effect |
|------|--------|------|--------|
| Default (SoF) | `GL_ZERO` | `GL_SRC_COLOR` | Modulate texture by light |
| Additive | `GL_ONE` | `GL_ONE` | Brighter lighting |
| Alpha Blend | `GL_SRC_ALPHA` | `GL_ONE_MINUS_SRC_ALPHA` | Transparency-based |
| Multiply | `GL_DST_COLOR` | `GL_ZERO` | Darker lighting |

## Files

### Feature Implementation
- `hooks.cpp` - Main implementation, RefDllLoaded callback, lightblend_change()

### Integration Points
- `src/cvars.cpp` - CVar registration and callback binding
- `hdr/features.h` - External declaration of lightblend_change()
- `features/FEATURES.txt` - Feature enablement (line 14)

## Build System

### Compilation Guard
```cpp
#include "../../../hdr/feature_config.h"
#if FEATURE_LIGHTING_BLEND
// ... feature code
#endif
```

### Enable/Disable
Edit `features/FEATURES.txt`:
```
lighting_blend      # Enabled
// lighting_blend   # Disabled
```

## Troubleshooting

### Blend modes not applying
- Check that ref.dll is loaded (only works in-game, not in menus)
- Verify CVar values with `cvarlist _sofbuddy_lightblend*`
- Check logs for "lighting_blend: Applied blend modes" message

### Invalid blend mode error
```
Bad lightblend value: GL_INVALID. Valid values:
GL_ZERO, GL_ONE, GL_SRC_COLOR, ...
```
Solution: Use one of the valid GL constants listed above (case-sensitive).

### Memory patches not working
- Addresses are specific to ref.dll version
- May need reverse engineering if using modified renderer

## Future Improvements
- Auto-detect ref.dll base address for portability
- Preset blend modes (e.g., "bright", "dark", "normal")
- Per-surface blend mode customization
- Integration with lighting quality settings

## See Also
- OpenGL `glBlendFunc()` documentation
- Quake 2 renderer source code (`SDK/Quake-2/ref_gl/gl_rsurf.c`)
- Feature documentation: `src/features/FEATURES.md`
