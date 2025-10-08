# CVar Refactoring - Migration Complete

## Overview
Successfully migrated from a centralized `cvars.cpp` file to feature-local cvar management. Each feature now manages its own cvars in a dedicated `cvars.cpp` file within its feature directory.

## Changes Made

### 1. Created Feature-Local CVar Files

#### `src/features/media_timers/cvars.cpp`
- `_sofbuddy_high_priority`
- `_sofbuddy_sleep`
- `_sofbuddy_sleep_jitter`
- `_sofbuddy_sleep_busyticks`

#### `src/features/lighting_blend/cvars.cpp`
- `_sofbuddy_lightblend_src`
- `_sofbuddy_lightblend_dst`

#### `src/features/texture_mapping_min_mag/cvars.cpp`
- `_sofbuddy_minfilter_unmipped`
- `_sofbuddy_magfilter_unmipped`
- `_sofbuddy_minfilter_mipped`
- `_sofbuddy_magfilter_mipped`
- `_sofbuddy_minfilter_ui`
- `_sofbuddy_magfilter_ui`
- `_gl_texturemode` (engine cvar reference)

### 2. Updated Feature Hooks

Each feature now registers its cvars via a `PostCvarInit` lifecycle callback:

- **media_timers/hooks.cpp**: Already had cvar initialization via `mediaTimers_PostCvarInit()`
- **lighting_blend/hooks.cpp**: Added `lightblend_PostCvarInit()` callback
- **texture_mapping_min_mag/hooks.cpp**: Added `texturemapping_PostCvarInit()` callback

### 3. Updated Headers

**hdr/features.h**:
- Removed obsolete cvar externs (scaled_font, whiteraven_lighting, classic_timers)
- Streamlined to only include cvars for active features
- Updated function declarations to match new naming convention

### 4. Updated Initialization

**src/simple_init.cpp**:
- Removed `create_sofbuddy_cvars()` call
- Features now self-register via `PostCvarInit` lifecycle hook

### 5. Cleaned Up Old File

**src/cvars.cpp**:
- Replaced with documentation file explaining the migration
- Listed all obsolete cvars from removed features for reference

## Benefits

1. **Better Organization**: Each feature's cvars are co-located with the feature code
2. **Self-Contained Features**: Features manage their own initialization
3. **Clearer Ownership**: Easy to see which cvars belong to which feature
4. **Easier Maintenance**: Adding/removing features no longer requires editing centralized files

## Obsolete CVars Removed

The following cvars were from removed or refactored features and are no longer in use:

- `_sofbuddy_classic_timers` - from removed core feature
- `_sofbuddy_whiteraven_lighting` - from removed ref_fixes feature
- `_sofbuddy_font_scale` - from removed scaled_font feature
- `_sofbuddy_console_size` - from removed scaled_font feature
- `_sofbuddy_hud_scale` - from removed scaled_font feature
- `con_maxlines` - from removed scaled_font feature
- `con_buffersize` - from removed scaled_font feature
- `test` / `test2` - testing cvars

## Build System

No changes required to the makefile - it automatically discovers all `.cpp` files including the new feature-local `cvars.cpp` files.

## Verification

Build completed successfully:
- All features compile with their local cvars
- Lifecycle system properly initializes each feature's cvars
- DLL size: 3.7MB
- No linker errors or warnings (except pre-existing inlining warning)

## Migration Date

September 30, 2025
