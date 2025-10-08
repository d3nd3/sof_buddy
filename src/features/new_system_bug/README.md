# New System Detection Bug Fix

## Overview
Fixes a bug on Proton where new hardware detection causes the game to load extremely poor performance settings.

## Problem Solved

### Hardware Detection Performance Bug
- **Problem**: On Proton (3.7.8, GloriousEggProton), when hardware values differ from saved config
- **Trigger**: CVars `vid_card` or `cpu_memory_using` become 'modified'
- **Result**: Game automatically loads low-performance configuration files:
  - `drivers/alldefs.cfg`
  - `geforce.cfg`
  - `cpu4.cfg`
  - `memory1.cfg`
- **Impact**: These files contain extremely poor performance values (low quality, low FPS settings)

### When This Occurs
This happens when:
1. First run on a new system
2. Hardware changes detected (different CPU/GPU from saved config)
3. Running on Proton/Wine where hardware detection differs from native Windows

## Technical Details

### Implementation
Uses the module loading lifecycle system to apply optimal settings after ref.dll loads:

```cpp
// Register for RefDllLoaded lifecycle
REGISTER_SHARED_HOOK_CALLBACK(RefDllLoaded, new_system_bug, new_system_bug_RefDllLoaded, 60);

// Apply optimal defaults when ref.dll loads
static void new_system_bug_RefDllLoaded(void) {
    // Override with highest quality settings
    orig_Cmd_ExecuteString("exec drivers/highest.cfg\n");
    
    // Fix specific problematic defaults
    orig_Cmd_ExecuteString("set fx_maxdebrisonscreen 128\n");
    orig_Cmd_ExecuteString("set r_isf GL_SOLID_FORMAT\n");
    orig_Cmd_ExecuteString("set r_iaf GL_ALPHA_FORMAT\n");
}
```

### Settings Applied

| Setting | Default (Bad) | Fixed Value | Purpose |
|---------|---------------|-------------|---------|
| Quality Profile | `drivers/alldefs.cfg` | `drivers/highest.cfg` | Override low-quality cascade |
| `fx_maxdebrisonscreen` | `1024` | `128` | Reduce CPU overhead from debris |
| `r_isf` | Varies | `GL_SOLID_FORMAT` | Fix texture compression |
| `r_iaf` | Varies | `GL_ALPHA_FORMAT` | Fix alpha channel compression |

### Execution Flow

```
Game starts
  → Detects "new" hardware (differs from config.cfg)
  → Loads ref.dll
  → new_system_bug_RefDllLoaded() triggers
  → Executes drivers/highest.cfg
  → Applies performance fixes
  → Game runs with optimal settings
```

## Configuration
No CVars - fix is automatically applied when enabled.

## Enable/Disable

### Enable This Feature
Edit `features/FEATURES.txt`:
```bash
# Debug/Development Features
new_system_bug  # enabled
```

### Disable This Feature (Default)
Edit `features/FEATURES.txt`:
```bash
# Debug/Development Features
// new_system_bug  # disabled
```

Then rebuild:
```bash
make clean && make debug
```

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

## Development Notes
- Uses `RefDllLoaded` lifecycle callback (priority 60)
- Leverages `orig_Cmd_ExecuteString()` to execute console commands
- Overrides cascade of low-performance configs
- Safe to enable/disable via compile-time configuration
- Part of Proton compatibility improvements
