# HD Textures Support

## What it does

Enables support for 4x larger textures with correct UV mapping in Soldier of Fortune. This feature fixes texture coordinate scaling issues that occur when using higher resolution texture replacements.

## Technical Details

### Problem

When using high-resolution texture replacements (e.g., 4x the original size), the game's UV mapping system doesn't properly scale the texture coordinates, resulting in:
- Stretched or tiled textures
- Incorrect texture alignment
- Visual artifacts on surfaces

### Solution

This feature hooks into the `GL_BuildPolygonFromSurface` function and maintains a database of original texture dimensions. When the game loads a high-resolution texture, the hook:

1. Captures the texture name from the surface data
2. Looks up the original texture dimensions in the database
3. Restores the original dimensions for UV calculation
4. Allows the rendering to proceed with correct UV mapping

### Memory Addresses

- **GL_BuildPolygonFromSurface**: `0x30016390` (ref_gl.dll)

### Hook Registration

Uses the `REGISTER_HOOK` macro with `RefDllLoaded` lifecycle callback to initialize when the renderer loads.

### Data Structure

```cpp
typedef struct {
    unsigned short w;  // Original width
    unsigned short h;  // Original height
} m32size;

std::unordered_map<std::string, m32size> default_textures;
```

## Configuration

The texture database in `initDefaultTexSizes()` needs to be populated with all game textures. Currently contains a small sample set.

### Adding Textures

To add more textures to the database, add entries in the `initDefaultTexSizes()` function:

```cpp
default_textures["textures/path/texture_name"] = {width, height};
```

## Usage

1. Ensure `hd_textures` is enabled in `features/FEATURES.txt`
2. Place high-resolution textures in the game directory
3. The feature automatically applies UV fixes at runtime

## Testing

When enabled, you should see in the logs:
```
HD Textures: Initializing...
HD Textures: Loaded N textures
```

Where N is the number of textures in the database.

## Future Improvements

- [ ] Complete the texture database with all SoF textures
- [ ] Auto-detection of original texture sizes
- [ ] Optional config file for custom texture mappings
- [ ] Support for texture packs with metadata files
