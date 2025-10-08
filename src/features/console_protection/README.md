# Console Protection

## What it does
Fixes critical console security vulnerabilities that cause crashes and buffer overflows in the original game.

## Features
- **Resolution Buffer Overflow Fix**: Prevents crashes when using resolutions > 2048px wide
- **Clipboard Paste Protection**: Prevents buffer overflow when pasting large text into console  
- **Console Drawing Safety**: Ensures safe console rendering at any resolution
- **Input Length Limiting**: Automatically truncates clipboard input to safe limits

## Technical Details

### Security Fixes

#### 1. Console Drawing Buffer Overflow (my_Con_Draw_Console)
- **Problem**: Original `Con_DrawInput()` crashes on wide resolutions (>2048px)
- **Solution**: Implement safe drawing with `MAXCMDLINE` (256 char) limit
- **Addresses**: 
  - Console input drawing: `0x2002111D` → `my_Con_Draw_Console`
  - Drawing bypass: `0x20020C90` → `0x20020D6C`

#### 2. Clipboard Paste Overflow (my_Sys_GetClipboardData)
- **Problem**: Pasting large clipboard content crashes the game
- **Solution**: Truncate paste content to fit within `MAXCMDLINE` buffer
- **Addresses**:
  - Clipboard function: `0x2004BB63` → `my_Sys_GetClipboardData`  
  - Paste bypass: `0x2004BB6C` → `0x2004BBFE`

### Hooks
- `Sys_GetClipboardData @ 0x20065E60` - Intercepts clipboard paste operations
- Memory patches applied during `PostCvarInit` lifecycle phase

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
```

## Configuration
No CVars - security features are always enabled for safety.

## Usage Notes
- Automatically prevents console crashes on any resolution
- Safely handles clipboard paste of any size
- Maintains all normal console functionality
- Invisible to end users - just prevents crashes

## Development Notes
- Uses lifecycle callbacks for memory patching during `PostCvarInit`
- Implements both function hooks and direct memory patches
- Critical safety feature - should always be enabled
- Part of core stability improvements

## Archived Notes
/*
			//0x48 -> 0x5C[A],0x5D[B],0x5E[G],0x5F[R]
			// 0x0C -> 0x1C [R][G][B][A] -> [R][G][B][A]
			// 16 + 8 = 24
			// White font?
			char * palette = malloc(24);
			memset(palette, 0x00, 0x24);
			*(unsigned int*)(palette + 16) = 0xFFFFFFFF;
			*(unsigned int*)(palette + 20) = 0xFFFFFFFF;
			*/