# Lighting Blend Mode

## Purpose
Allows customization of OpenGL blend modes used for lighting in the ref.dll renderer. This feature patches memory locations that control the source and destination blend factors for `glBlendFunc()`, enabling different lighting effects.

## Callbacks
- **PostCvarInit** (Post, Priority: 50)
  - `lightblend_PostCvarInit()` - Registers lighting blend CVars
- **RefDllLoaded** (Post, Priority: 60)
  - `lightblend_RefDllLoaded()` - Applies blend mode patches to ref.dll memory and initializes settings

## Hooks
- **R_BlendLightmaps** (Pre, Priority: 100)
  - `r_blendlightmaps_pre_callback()` - Sets blend mode targets before lightmap blending
- **R_BlendLightmaps** (Post, Priority: 100)
  - `r_blendlightmaps_post_callback()` - Resets blending state after lightmap blending

## OverrideHooks
None

## CustomDetours
- **glBlendFunc** (ref.dll, via memory patch)
  - `glBlendFunc_R_BlendLightmaps()` - Intercepts glBlendFunc calls during lightmap blending to apply custom blend modes
  - Patched at ref.dll addresses: `0x3001B9A4`, `0x3001B690`
- **AdjustTexCoords** (ref.dll, via memory patch)
  - `hkAdjustTexCoords()` - Calls the original `Builds_S_T_Shiny` for the non-spherical path and submits the resulting S/T
  - Patched at ref.dll address: `0x30014995`

## Technical Details

### Memory Addresses (ref.dll)
- **Source blend factor**: `0x300A4610` - Default: `GL_ZERO`
- **Destination blend factor**: `0x300A43FC` - Default: `GL_SRC_COLOR`
- **Lighting cutoff**: `0x3002C368` - Float value for lighting cutoff
- **Water size (double)**: `0x3002C390` - Double value for water size
- **Water size (float)**: `0x3002C398` - Float value for water size (1.0 / water_size)
- **Shiny spherical branch**: `0x30014814` - `SURF_SIMPLE_SPHERICAL` branch in `R_DrawDetailChains`
- **Shiny sphere texinfo guard**: `0x30014A66` - Null-texinfo guard in the `GL_SPHERE_MAP` pass
- **Shiny pass gate**: `0x300147DF` - "shiny surfaces collected" test before the shiny pass
- **Shiny flat threshold**: `0x3002C434` - `radiusSquared`/flat-surface cutoff, stock `0.1f`

### Default Behavior
The SoF engine uses:
```c
glBlendFunc(GL_ZERO, GL_SRC_COLOR);
```

This is equivalent to: `Result = DST * SRC_COLOR + SRC * 0`

### OpenGL Blend Function Equation
```
Final_Color = (Src * SrcFactor) + (Dst * DstFactor)
```

Where:
- **Src** = Incoming fragment (light)
- **Dst** = Existing framebuffer pixel (texture)
- **SrcFactor** = `_sofbuddy_lightblend_src`
- **DstFactor** = `_sofbuddy_lightblend_dst`

## Configuration

### CVars
- `_sofbuddy_lightblend_src` - Source blend factor (default: `"GL_ZERO"`)
- `_sofbuddy_lightblend_dst` - Destination blend factor (default: `"GL_SRC_COLOR"`)
- `_sofbuddy_lighting_overbright` - Enable overbright lighting (default: 0)
- `_sofbuddy_lighting_cutoff` - Lighting cutoff value (default: varies)
- `_sofbuddy_water_size` - Water size parameter (default: 16.0, minimum: 16.0)
- `_sofbuddy_shiny_spherical` - Shiny detail-texture mode (default: `1`)
  - `0` - Off: skip the shiny/env-map pass entirely
  - `1` - Default: stock behaviour (spherical on `SURF_SIMPLE_SPHERICAL`, plus the flat-surface heuristic)
  - `2` - No spherical: draw every shiny surface with the non-spherical/planar projection
  - `3` - All spherical: force every shiny surface through `GL_SPHERE_MAP`
  - `4` - Both (experimental): draw every shiny surface with both projections; forces the flat threshold to `10.1f` so every surface is also queued for the sphere pass

Spherical depends on **camera position and view angle**. The non-spherical/planar
projection is built from the camera position relative to the surface and a
world-space surface basis, so pure view rotation does not change it.

All persisted CVars are marked with `CVAR_SOFBUDDY_ARCHIVE` and saved to `base/sofbuddy.cfg`.

### Valid Blend Mode Values
- `GL_ZERO`, `GL_ONE`, `GL_SRC_COLOR`, `GL_ONE_MINUS_SRC_COLOR`
- `GL_DST_COLOR`, `GL_ONE_MINUS_DST_COLOR`, `GL_SRC_ALPHA`, `GL_ONE_MINUS_SRC_ALPHA`
- `GL_DST_ALPHA`, `GL_ONE_MINUS_DST_ALPHA`, `GL_CONSTANT_COLOR`, `GL_ONE_MINUS_CONSTANT_COLOR`
- `GL_CONSTANT_ALPHA`, `GL_ONE_MINUS_CONSTANT_ALPHA`, `GL_SRC_ALPHA_SATURATE`

### Common Blend Modes
| Mode | Source | Dest | Effect |
|------|--------|------|--------|
| Default (SoF) | `GL_ZERO` | `GL_SRC_COLOR` | Modulate texture by light |
| Additive | `GL_ONE` | `GL_ONE` | Brighter lighting |
| Alpha Blend | `GL_SRC_ALPHA` | `GL_ONE_MINUS_SRC_ALPHA` | Transparency-based |
| Multiply | `GL_DST_COLOR` | `GL_ZERO` | Darker lighting |

## Implementation Flow

### 1. Startup (Module Loading)
```
SoF loads ref.dll
  → lightblend_RefDllLoaded() callback runs
  → Patches memory addresses in ref.dll
  → Applies initial blend mode settings
  → Blend modes are active
```

### 2. Runtime (CVar Changes)
```
User changes CVar
  → CVar change callback runs
  → Updates blend mode variables
  → Patches ref.dll memory
  → New blend mode takes effect immediately
```

## Requirements
- **IMPORTANT**: Requires `gl_ext_multitexture 0` for proper operation
- Feature automatically sets `gl_ext_multitexture` to 0 if enabled
