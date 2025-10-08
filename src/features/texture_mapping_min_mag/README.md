# Texture Filtering Configuration

## Overview
Provides fine-grained control over OpenGL texture filtering modes (minification and magnification filters) for different texture types in Soldier of Fortune. This allows users to customize texture quality and appearance.

## Features
- Separate filter settings for mipped, unmipped, and UI textures
- Support for all OpenGL minification filter modes
- Support for all OpenGL magnification filter modes
- CVars for runtime configuration

## Configuration

### Minification Filters (GL_TEXTURE_MIN_FILTER)

Controls how textures are sampled when they appear smaller than their original size.

**Available Modes:**
- `GL_NEAREST` - Nearest neighbor (blocky, sharp)
- `GL_LINEAR` - Linear interpolation (smooth)
- `GL_NEAREST_MIPMAP_NEAREST` - Nearest mipmap, nearest texel
- `GL_LINEAR_MIPMAP_NEAREST` - Nearest mipmap, linear texel
- `GL_NEAREST_MIPMAP_LINEAR` - Linear mipmap, nearest texel (bilinear)
- `GL_LINEAR_MIPMAP_LINEAR` - Linear mipmap, linear texel (trilinear, best quality)

**CVars:**
```
_sofbuddy_minfilter_unmipped  - For sky and detail textures (default: GL_LINEAR)
_sofbuddy_minfilter_mipped    - For world textures (default: GL_LINEAR_MIPMAP_LINEAR)
_sofbuddy_minfilter_ui        - For UI elements (default: GL_NEAREST)
```

### Magnification Filters (GL_TEXTURE_MAG_FILTER)

Controls how textures are sampled when they appear larger than their original size.

**Available Modes:**
- `GL_NEAREST` - Nearest neighbor (blocky, pixelated look)
- `GL_LINEAR` - Linear interpolation (smooth)

**CVars:**
```
_sofbuddy_magfilter_unmipped  - For sky and detail textures (default: GL_LINEAR)
_sofbuddy_magfilter_mipped    - For world textures (default: GL_LINEAR)
_sofbuddy_magfilter_ui        - For UI elements (default: GL_NEAREST)
```

## Usage Examples

### Crisp, Sharp Textures (Retro Look)
```
set _sofbuddy_minfilter_mipped GL_NEAREST_MIPMAP_NEAREST
set _sofbuddy_magfilter_mipped GL_NEAREST
```

### Maximum Quality (Modern Look)
```
set _sofbuddy_minfilter_mipped GL_LINEAR_MIPMAP_LINEAR
set _sofbuddy_magfilter_mipped GL_LINEAR
```

### Sharper UI Elements
```
set _sofbuddy_minfilter_ui GL_NEAREST
set _sofbuddy_magfilter_ui GL_NEAREST
```

## Technical Details

### Texture Types

1. **Mipped Textures**: World geometry, models - use mipmaps for LOD
2. **Unmipped Textures**: Sky, detail textures - no mipmapping
3. **UI Textures**: Console, HUD elements - typically want sharp rendering

### Implementation

The feature provides CVar change callbacks that:
1. Validate user input against supported OpenGL modes
2. Update internal filter settings
3. Trigger texture mode refresh via `gl_texturemode->modified` flag
4. Provide logging of changes

### Memory Addresses

This feature works with engine CVars and doesn't require direct memory patching. The actual texture filtering is applied by the renderer based on these settings.

## Benefits

- **Customization**: Users can tune texture filtering to their preference
- **Performance**: Can reduce quality for better FPS on low-end hardware
- **Aesthetics**: Can achieve retro pixelated or modern smooth look
- **Compatibility**: Works alongside HD texture packs

## Notes

- Changes take effect immediately via the texture mode refresh system
- Invalid filter names are rejected with helpful error messages
- Default values are chosen for balanced quality/performance
- UI textures default to `GL_NEAREST` to keep fonts crisp

## Related Features

- **hd_textures**: For using high-resolution texture replacements
- **vsync_toggle**: For controlling vsync settings
