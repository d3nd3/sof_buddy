# Texture Filtering Configuration

## Purpose
Provides fine-grained control over OpenGL texture filtering modes (minification and magnification filters) for different texture types in Soldier of Fortune. This allows users to customize texture quality and appearance.

## Callbacks
- **PostCvarInit** (Post, Priority: 50)
  - `texturemapping_PostCvarInit()` - Registers texture filtering CVars
- **RefDllLoaded** (Post, Priority: 70)
  - `setup_minmag_filters()` - Applies memory patches to intercept glTexParameterf calls for different texture types

## Hooks
None

## OverrideHooks
None

## CustomDetours
- **glTexParameterf** (ref.dll, via memory patches)
  - Multiple intercepts for different texture types:
    - `orig_glTexParameterf_min_mipped()` - Minification filter for mipped textures
    - `orig_glTexParameterf_mag_mipped()` - Magnification filter for mipped textures
    - `orig_glTexParameterf_min_unmipped()` - Minification filter for unmipped textures
    - `orig_glTexParameterf_mag_unmipped()` - Magnification filter for unmipped textures
    - `orig_glTexParameterf_min_ui()` - Minification filter for UI textures
    - `orig_glTexParameterf_mag_ui()` - Magnification filter for UI textures
  - Patched at ref.dll addresses: `0x30006636`, `0x30006660`, `0x300065DF`, `0x30006609`, `0x3000659C`, `0x300065B1`

## Technical Details

### Texture Types

1. **Mipped Textures**: World geometry, models - use mipmaps for LOD
2. **Unmipped Textures**: Sky, detail textures - no mipmapping
3. **UI Textures**: Console, HUD elements - typically want sharp rendering

### Minification Filters (GL_TEXTURE_MIN_FILTER)

Controls how textures are sampled when they appear smaller than their original size.

**Available Modes:**
- `GL_NEAREST` - Nearest neighbor (blocky, sharp)
- `GL_LINEAR` - Linear interpolation (smooth)
- `GL_NEAREST_MIPMAP_NEAREST` - Nearest mipmap, nearest texel
- `GL_LINEAR_MIPMAP_NEAREST` - Nearest mipmap, linear texel
- `GL_NEAREST_MIPMAP_LINEAR` - Linear mipmap, nearest texel (bilinear)
- `GL_LINEAR_MIPMAP_LINEAR` - Linear mipmap, linear texel (trilinear, best quality)

### Magnification Filters (GL_TEXTURE_MAG_FILTER)

Controls how textures are sampled when they appear larger than their original size.

**Available Modes:**
- `GL_NEAREST` - Nearest neighbor (blocky, pixelated look)
- `GL_LINEAR` - Linear interpolation (smooth)

## Configuration

### CVars
```
_sofbuddy_minfilter_unmipped  - For sky and detail textures (default: GL_LINEAR)
_sofbuddy_minfilter_mipped    - For world textures (default: GL_LINEAR_MIPMAP_LINEAR)
_sofbuddy_minfilter_ui        - For UI elements (default: GL_NEAREST)
_sofbuddy_magfilter_unmipped  - For sky and detail textures (default: GL_LINEAR)
_sofbuddy_magfilter_mipped    - For world textures (default: GL_LINEAR)
_sofbuddy_magfilter_ui        - For UI elements (default: GL_NEAREST)
```

### Usage Examples

#### Crisp, Sharp Textures (Retro Look)
```
set _sofbuddy_minfilter_mipped GL_NEAREST_MIPMAP_NEAREST
set _sofbuddy_magfilter_mipped GL_NEAREST
```

#### Maximum Quality (Modern Look)
```
set _sofbuddy_minfilter_mipped GL_LINEAR_MIPMAP_LINEAR
set _sofbuddy_magfilter_mipped GL_LINEAR
```

#### Sharper UI Elements
```
set _sofbuddy_minfilter_ui GL_NEAREST
set _sofbuddy_magfilter_ui GL_NEAREST
```

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
