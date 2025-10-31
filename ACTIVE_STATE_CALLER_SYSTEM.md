# Active State Caller System

## Overview

The caller detection system now uses an **active state pattern** where each hook automatically sets its caller context to "active" before invoking the original function, then clears it afterwards. This allows downstream hooks to check the active caller with a **single conditional**.

## Key Concept: Active State Flow

```
High-level function (e.g., CON_DrawConsole)
  ↓
hkDraw_StretchPic:
  - Detect caller RVA
  - Set g_currentStretchPicCaller = ConDrawConsole  ← ACTIVE
  - Call original Draw_StretchPic()
    ↓
    hkglVertex2f:
      - Check: if (g_currentStretchPicCaller == ConDrawConsole)  ← Single check!
      - Apply console-specific scaling
    ↓
  - Clear g_currentStretchPicCaller = Unknown  ← INACTIVE
```

## Benefits

### 1. **Single Conditional Check**
```cpp
// ✅ Clean and simple
if (g_currentStretchPicCaller == StretchPicCaller::ConDrawConsole) {
    orig_glVertex2f(x * fontScale, y * fontScale);
    return;
}
```

Instead of:
```cpp
// ❌ Complex logic
StretchPicCaller caller = (g_currentStretchPicCaller != Unknown) 
    ? g_currentStretchPicCaller 
    : g_detectedStretchPicCaller;
if (caller == ConDrawConsole) { ... }
```

### 2. **Automatic State Management**
The state is automatically:
- Set when entering the hook
- Available to all downstream calls
- Cleared when exiting the hook

### 3. **Natural Call Stack Flow**
The active state naturally flows down the call stack and is automatically cleaned up on return.

## Implementation Pattern

### Hook Pattern
```cpp
void hkSomeFunction(...) {
    // 1. Detect caller via RVA
    uint32_t fnStart = HookCallsite::recordAndGetFnStartExternal("FunctionName");
    if (fnStart) {
        g_detectedCaller = getCallerFromRva(fnStart);
        
        // Optional: Debug logging for unknown callers
        if (_debug_cvar->value > 0 && g_detectedCaller == Unknown) {
            PrintOut(PRINT_LOG, "[FunctionName] Unknown caller RVA: 0x%08x\n", fnStart);
        }
    }
    
    // 2. Activate state if detected
    if (g_currentCaller == Unknown && g_detectedCaller != Unknown) {
        g_currentCaller = g_detectedCaller;
    }
    
    // 3. Apply function-specific logic based on active caller
    if (g_currentCaller == SpecificContext) {
        // Handle this specific case
        oOriginalFunction(...);
        g_currentCaller = Unknown;  // Clear before return
        return;
    }
    
    // 4. Call original function (state is active for downstream hooks)
    oOriginalFunction(...);
    
    // 5. Deactivate state (only if we activated it)
    if (g_detectedCaller != Unknown) {
        g_currentCaller = Unknown;
    }
}
```

### Downstream Hook Pattern
```cpp
void hkDownstreamFunction(...) {
    // Just check the active state - no complex logic needed!
    
    if (g_currentStretchPicCaller == StretchPicCaller::ConDrawConsole) {
        // Console rendering
        return;
    }
    
    if (g_currentPicCaller == PicCaller::Crosshair) {
        // Crosshair rendering
        return;
    }
    
    // Default behavior
    oOriginalFunction(...);
}
```

## State Variables

Each layer maintains two state variables:

```cpp
static XxxCaller g_currentXxxCaller = Unknown;   // Currently active caller
static XxxCaller g_detectedXxxCaller = Unknown;  // Detected via RVA this frame
```

### State Flow

1. **Detection Phase**: `g_detectedXxxCaller = getXxxCallerFromRva(fnStart)`
2. **Activation Phase**: `if (g_currentXxxCaller == Unknown) g_currentXxxCaller = g_detectedXxxCaller`
3. **Active Phase**: Downstream hooks check `g_currentXxxCaller`
4. **Deactivation Phase**: `g_currentXxxCaller = Unknown`

## Example: Console Rendering

### Call Stack
```
CON_DrawConsole()                    [RVA: 0x00020F90]
  ↓
hkDraw_StretchPic()
  - Detects RVA 0x00020F90
  - Maps to StretchPicCaller::ConDrawConsole
  - Sets g_currentStretchPicCaller = ConDrawConsole
  ↓
  oDraw_StretchPic()  ← Original function
    ↓
    hkglVertex2f()
      - Checks: g_currentStretchPicCaller == ConDrawConsole ✓
      - Applies: orig_glVertex2f(x * fontScale, y * fontScale)
    ↓
  ← Returns from oDraw_StretchPic()
  ↓
  - Clears g_currentStretchPicCaller = Unknown
  ↓
← Returns from hkDraw_StretchPic()
```

### Code
```cpp
void hkDraw_StretchPic(int x, int y, int w, int h, int palette, char * name, int flags) {
    uint32_t fnStart = HookCallsite::recordAndGetFnStartExternal("Draw_StretchPic");
    if (fnStart) {
        g_detectedStretchPicCaller = getStretchPicCallerFromRva(fnStart);
    }
    
    // Activate state
    if (g_currentStretchPicCaller == Unknown && g_detectedStretchPicCaller != Unknown) {
        g_currentStretchPicCaller = g_detectedStretchPicCaller;
    }
    
    StretchPicCaller caller = g_currentStretchPicCaller;
    
    if (caller == StretchPicCaller::ConDrawConsole) {
        // Console-specific adjustments
        extern float draw_con_frac;
        int consoleHeight = draw_con_frac * current_vid_h;
        y = -1 * current_vid_h + consoleHeight;
        oDraw_StretchPic(x, y, w, h, palette, name, flags);
        orig_SRC_AddDirtyPoint(0, 0);
        orig_SRC_AddDirtyPoint(w - 1, consoleHeight - 1);
        g_currentStretchPicCaller = Unknown;  // Deactivate
        return;
    }
    
    // Default path
    oDraw_StretchPic(x, y, w, h, palette, name, flags);
    
    // Deactivate state
    if (g_detectedStretchPicCaller != Unknown) {
        g_currentStretchPicCaller = Unknown;
    }
}

void __stdcall hkglVertex2f(float x, float y) {
    // Single conditional check on active state!
    if (g_currentStretchPicCaller == StretchPicCaller::ConDrawConsole) {
        orig_glVertex2f(x * fontScale, y * fontScale);
        return;
    }
    
    // Other checks...
    orig_glVertex2f(x, y);
}
```

## All Active State Variables

| Variable | Type | Purpose |
|----------|------|---------|
| `g_currentStretchPicCaller` | StretchPicCaller | Active Draw_StretchPic context |
| `g_detectedStretchPicCaller` | StretchPicCaller | RVA-detected StretchPic caller |
| `g_currentPicCaller` | PicCaller | Active Draw_Pic context |
| `g_detectedPicCaller` | PicCaller | RVA-detected Pic caller |
| `g_currentVertexCaller` | VertexCaller | Active glVertex2f context |
| `g_detectedVertexCaller` | VertexCaller | RVA-detected vertex caller |

## Layered Checking

You can check multiple layers simultaneously:

```cpp
void __stdcall hkglVertex2f(float x, float y) {
    // Layer 1: High-level StretchPic context
    if (g_currentStretchPicCaller == StretchPicCaller::ConDrawConsole) {
        orig_glVertex2f(x * fontScale, y * fontScale);
        return;
    }
    
    // Layer 2: High-level Pic context
    if (g_currentPicCaller == PicCaller::Crosshair) {
        // Crosshair scaling...
        return;
    }
    
    // Layer 3: Combined check (if needed)
    if (g_currentStretchPicCaller == StretchPicCaller::ExecuteLayoutString &&
        g_currentVertexCaller == VertexCaller::DrawChar) {
        // Very specific scoreboard text rendering
        return;
    }
    
    // Default
    orig_glVertex2f(x, y);
}
```

## No Manual State Management Needed

Unlike previous approaches, you **never** need to manually call `setXxxCaller()` or manage state:

**Before (Manual):**
```cpp
void hkCon_DrawConsole(float frac) {
    setVertexCaller(VertexCaller::Console);  // ❌ Manual
    oCon_DrawConsole(frac);
    setVertexCaller(VertexCaller::Unknown);  // ❌ Manual cleanup
}
```

**After (Automatic):**
```cpp
void hkCon_DrawConsole(float frac) {
    oCon_DrawConsole(frac);  // ✅ Automatic via RVA detection
}
```

The RVA detection in `hkDraw_StretchPic` automatically handles state management!

## Performance

- **Minimal overhead**: Single enum comparison per check
- **No function calls**: Inline state checks, no function call overhead
- **Cache-friendly**: Static variables stay in cache
- **Short-lived state**: Only active during the call, no persistent state

## RVA Collection

RVAs are automatically collected via the **ParentRecorder** system:
- Data is written to `sof_buddy/func_parents/*.json`
- Data persists and merges across sessions
- No manual logging needed - just run the game!

Add collected RVAs from JSON files to the switch statements in `get*CallerFromRva()` functions.

## Summary

✅ **Single conditional checks** - `if (g_currentCaller == X)`  
✅ **Automatic state management** - Set before call, clear after  
✅ **Natural call stack flow** - State flows downstream automatically  
✅ **No manual tracking** - RVA detection handles everything  
✅ **Layered checking** - Can check multiple contexts simultaneously  
✅ **Persistent data collection** - RVAs accumulate across sessions  
✅ **Clean code** - Removed all manual `setVertexCaller()` calls

This is an elegant, efficient, and maintainable approach to context-aware hook management!

