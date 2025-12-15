# New System Detection Bug Fix

## Purpose
Fixes a bug on Proton where new hardware detection causes the game to load extremely poor performance settings. Ensures optimal performance settings are applied when hardware detection differs from saved configuration.

## Callbacks
- **EarlyStartup** (Post, Priority: 60)
  - `new_system_bug_EarlyStartup()` - Patches LoadLibraryA to intercept ref.dll loading and apply optimal settings

## Hooks
None

## OverrideHooks
None

## CustomDetours
- **LoadLibraryA** (SoF.exe, via memory patch)
  - `new_sys_bug_LoadLibraryRef()` - Intercepts ref.dll loading to apply optimal settings
  - Patched at exe address: `0x20066E75`
- **InitDefaults** (ref.dll, via memory patch)
  - `new_system_bug_InitDefaults()` - Overrides default settings initialization
  - Patched at ref.dll address: `0x3000FA26`

## Technical Details

### Problem
On Proton (3.7.8, GloriousEggProton), when hardware values differ from saved config:
- CVars `vid_card` or `cpu_memory_using` become 'modified'
- Game automatically loads low-performance configuration files:
  - `drivers/alldefs.cfg`
  - `geforce.cfg`
  - `cpu4.cfg`
  - `memory1.cfg`
- These files contain extremely poor performance values (low quality, low FPS settings)

### When This Occurs
This happens when:
1. First run on a new system
2. Hardware changes detected (different CPU/GPU from saved config)
3. Running on Proton/Wine where hardware detection differs from native Windows

### Settings Applied

| Setting | Default (Bad) | Fixed Value | Purpose |
|---------|---------------|-------------|---------|
| Quality Profile | `drivers/alldefs.cfg` | `drivers/highest.cfg` | Override low-quality cascade |
| `fx_maxdebrisonscreen` | `1024` | `128` | Reduce CPU overhead from debris |
| `r_isf` | Varies | `GL_SOLID_FORMAT` | Fix texture compression |
| `r_iaf` | Varies | `GL_ALPHA_FORMAT` | Fix alpha channel compression |

### Implementation Flow
```
Game starts
  → Detects "new" hardware (differs from config.cfg)
  → Loads ref.dll
  → new_sys_bug_LoadLibraryRef() intercepts
  → Executes drivers/highest.cfg
  → Applies performance fixes
  → Game runs with optimal settings
```

## Configuration
No CVars - fix is automatically applied when enabled.

## Benefits
- **Optimal Performance**: Game always uses highest quality settings
- **Proton Compatibility**: Fixes poor performance on Proton/Wine
- **CPU Efficiency**: Reduces fx_maxdebrisonscreen from 1024 to 128
- **Texture Quality**: Ensures proper texture compression formats

## Why Disabled by Default?
This feature is disabled by default because:
1. It may override user-configured settings
2. It forces highest quality settings which may not be suitable for all systems
3. Modern systems typically don't need this workaround
4. Primarily useful for Proton/Wine compatibility testing

## Use Cases
Enable this feature if:
- Running SoF on Proton/Wine with poor performance
- Experiencing low quality graphics after fresh install
- Testing compatibility on Linux/Proton
- Debugging hardware detection issues
