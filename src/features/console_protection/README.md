# Console Protection

## Purpose
Fixes critical console security vulnerabilities that cause crashes and buffer overflows in the original game. Prevents crashes when using resolutions > 2048px wide and when pasting large text into console.

## Callbacks
- **EarlyStartup** (Post, Priority: 80)
  - `console_protection_EarlyStartup()` - Applies memory patches to secure console drawing and clipboard handling

## Hooks
None

## OverrideHooks
None

## CustomDetours
- **Sys_GetClipboardData** (SoF.exe, via memory patch)
  - `hkSys_GetClipboardData()` - Truncates clipboard paste content to fit within `MAXCMDLINE` buffer (256 chars)
  - Patched at exe address: `0x2004BB63`
- **Con_Draw_Console** (SoF.exe, via memory patch)
  - `my_Con_Draw_Console()` - Safe console drawing with `MAXCMDLINE` limit to prevent crashes on wide resolutions
  - Patched at exe address: `0x2002111D`

## Technical Details

### Security Fixes

#### 1. Console Drawing Buffer Overflow
- **Problem**: Original `Con_DrawInput()` crashes on wide resolutions (>2048px)
- **Solution**: Implement safe drawing with `MAXCMDLINE` (256 char) limit
- **Memory Patches**:
  - Console input drawing: `0x2002111D` → `my_Con_Draw_Console`
  - Drawing bypass: `0x20020C90` → `0x20020D6C`

#### 2. Clipboard Paste Overflow
- **Problem**: Pasting large clipboard content crashes the game
- **Solution**: Truncate paste content to fit within `MAXCMDLINE` buffer
- **Memory Patches**:
  - Clipboard function: `0x2004BB63` → `hkSys_GetClipboardData`
  - Paste bypass: `0x2004BB6C` → `0x2004BBFE`

### Memory Layout
```cpp
// Console state variables
edit_line:     0x20367EA4  // Current edit line index
key_lines:     0x20365EA0  // Console input buffer  
key_linepos:   0x20365E9C  // Cursor position
con_linewidth: 0x2024AF98  // Console width in characters
con_vislines:  0x2024AFA0  // Console visible lines
cls_realtime:  0x201C1F0C  // Game time for cursor blink
cls_key_dest:  0x201C1F04  // Input destination
cls_state:     0x201C1F00  // Client state

// Functions
Z_Free:        0x200F9D32  // Memory free function
ref_draw_char: 0x204035C8  // Character drawing function
oSys_GetClipboardData: 0x20065E60  // Original clipboard function
```

## Configuration
No CVars - security features are always enabled for safety.

## Benefits
- **Prevents console crashes** on any resolution
- **Safely handles clipboard paste** of any size
- **Maintains all normal console functionality**
- **Invisible to end users** - just prevents crashes

## Usage Notes
- Automatically prevents console crashes on any resolution
- Safely handles clipboard paste of any size
- Maintains all normal console functionality
- Critical safety feature - should always be enabled
