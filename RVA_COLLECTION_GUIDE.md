# RVA Collection Guide

## Overview

The codebase now has **7 separate caller enum systems** for collecting function caller addresses (RVAs) across multiple game sessions. Data persists and merges automatically.

## Enum Systems

### 1. **VertexCaller** (glVertex2f)
- **File**: `hooks.cpp`
- **Contexts**: Console, Scoreboard, DrawPicCenter, DrawPicTiled, MenuLoadbox, etc.

### 2. **StretchPicCaller** (Draw_StretchPic)
- **File**: `hooks.cpp`
- **Contexts**: CtfFlagDraw, ConDrawConsole, LoadboxDraw, VbarDraw, etc.

### 3. **PicCaller** (Draw_Pic)
- **File**: `hooks.cpp`
- **Contexts**: ExecuteLayoutString, Crosshair, UpdateScreenPics, BackdropDraw

### 4. **PicOptionsCaller** (Draw_PicOptions)
- **File**: `hud.cpp`
- **Contexts**: InterfaceDrawNum, DMRankingDraw, CrosshairDraw

### 5. **CroppedPicCaller** (Draw_CroppedPicOptions)
- **File**: `hud.cpp`
- **Contexts**: Inventory2Draw, HealthArmor2Draw

### 6. **FindImageCaller** (GL_FindImage)
- **File**: `hooks.cpp`
- **Contexts**: CrosshairLoad, MenuBackdrop, HudElement, Other

### 7. **FontCaller** (R_DrawFont)
- **File**: `text.cpp`
- **Already has RVAs populated**
- **Contexts**: ScopeCalcXY, DMRankingCalcXY, Inventory2, SCRDrawPause, etc.

## Quick Start

### Exercise All UI Features

Run the game multiple times and:
- **Session 1**: Focus on console and basic menus
- **Session 2**: Play multiplayer, view scoreboard
- **Session 3**: Test all HUD elements (health, ammo, inventory)
- **Session 4**: Navigate all menu screens
- **Session 5**: Test special features (CTF flag, scope, etc.)

### Check Results

Look in `sof_buddy/func_parents/*.json` for collected data. Each file contains all unique callers discovered across ALL sessions.

Example: `sof_buddy/func_parents/glVertex2f.json`

```json
{
  "child": "glVertex2f",
  "parents": [
    { "module": "SoF.exe", "fnStart": 82928, "fnStartHex": "0x00014410", ... },
    { "module": "ref_gl.dll", "fnStart": 18512, "fnStartHex": "0x00004850", ... }
  ]
}
```

### Add RVAs to Code

Open the appropriate file and add to the switch statement:

**Example: hooks.cpp (line ~67)**
```cpp
static VertexCaller getVertexCallerFromRva(uint32_t fnStartRva) {
    switch (fnStartRva) {
        case 0x00020F90: return VertexCaller::Console;
        case 0x00014510: return VertexCaller::Scoreboard;
        // Add more from JSON files...
        default:
            return VertexCaller::Unknown;
    }
}
```

## Persistent Data Feature

### What Changed

The `ParentRecorder` now:
1. **Loads existing JSON files** on startup
2. **Merges** new discoveries with historical data
3. **Preserves** all unique callers across sessions
4. **Accumulates** a complete picture over time

### Implementation Details

**Modified Files:**
- `parent_recorder.h` - Added `loadExistingData()` and `moduleFromString()`
- `parent_recorder.cpp` - Implemented JSON loading and parsing

**Key Features:**
- Automatic on initialization
- Only in debug builds (`#ifndef NDEBUG`)
- No data loss between sessions
- Set-based deduplication (no duplicates)

### Benefits

- **Comprehensive coverage**: Run different gameplay scenarios across sessions
- **No data loss**: Previous discoveries preserved
- **Incremental**: Add more data without re-testing everything
- **Reliable**: Set data structure ensures no duplicates

## Tips

1. **Run multiple short sessions** focusing on different features
2. **Check JSON files** after each session to see new discoveries
3. **Backup the func_parents directory** before major testing
4. **Use different game modes** (single player, multiplayer, different maps)
5. **Test all menu paths** (settings, multiplayer browser, load/save, etc.)

## Example Workflow

```bash
# Session 1: Console testing
# Open/close console repeatedly
# Exit game
# Check glVertex2f.json - should have console-related RVAs

# Session 2: Multiplayer
# Join server, view scoreboard, play
# Exit game  
# Check JSON files - now has console + scoreboard + HUD RVAs

# Session 3: Menu exploration
# Navigate all menus
# Exit game
# Check JSON files - comprehensive caller list!
```

## Location of Switch Statements

Add collected RVAs to these functions:

| Function | File | Approx Line |
|----------|------|-------------|
| `getVertexCallerFromRva()` | hooks.cpp | 67 |
| `getStretchPicCallerFromRva()` | hooks.cpp | 75 |
| `getPicCallerFromRva()` | hooks.cpp | 83 |
| `getPicOptionsCallerFromRva()` | hooks.cpp | 91 |
| `getCroppedPicCallerFromRva()` | hooks.cpp | 99 |
| `getFindImageCallerFromRva()` | hooks.cpp | 107 |
| `getFontCallerFromRva()` | text.cpp | 49 |

## Data Collection

RVAs are automatically collected via the **ParentRecorder** system:
- Data is written to `sof_buddy/func_parents/*.json`
- Data persists and merges across sessions
- No configuration needed - just run the game!

## Summary

You now have a powerful system to collect comprehensive caller information across multiple game sessions with automatic persistence and merging. Perfect for building complete enum mappings!

