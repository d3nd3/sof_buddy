# Scaled HUD

## Purpose
Scales HUD elements (health, armor, ammo, inventory, scoreboard, CTF flag) to be readable on modern high-resolution displays. Provides independent scaling for different HUD components.

## Callbacks
- **RefDllLoaded** (Post, Priority: 60)
  - `scaledHud_RefDllLoaded()` - Installs custom detours for cropped pic vertex manipulation

## Hooks
None (screen dimensions are updated by scaled_ui_base's VID_CheckChanges Post hook).

## OverrideHooks
- **Draw_PicOptions** (SoF.exe)
  - `hkDraw_PicOptions()` - Scales HUD elements drawn with Draw_PicOptions (crosshair, etc.)
- **Draw_CroppedPicOptions** (SoF.exe)
  - `hkDraw_CroppedPicOptions()` - Scales cropped HUD elements (health, armor, ammo, inventory)
- **cInventory2_And_cGunAmmo2_Draw** (SoF.exe)
  - `hkcInventory2_And_cGunAmmo2_Draw()` - Scales inventory and ammo display
- **cHealthArmor2_Draw** (SoF.exe)
  - `hkcHealthArmor2_Draw()` - Scales health and armor display
- **cDMRanking_Draw** (SoF.exe)
  - `hkcDMRanking_Draw()` - Scales deathmatch ranking display
- **cCtfFlag_Draw** (SoF.exe)
  - `hkcCtfFlag_Draw()` - Sets render type for CTF flag scaling
- **SCR_ExecuteLayoutString** (SoF.exe)
  - `hkSCR_ExecuteLayoutString()` - Scales scoreboard elements

## CustomDetours
- **CroppedPic vertex functions** (ref.dll)
  - `my_glVertex2f_CroppedPic_1/2/3/4()` - Custom vertex scaling for cropped pic elements
  - Installed at runtime in `scaledHud_RefDllLoaded()`
  - Addresses: `ref.dll + 0x0000239E, 0x000023CC, 0x000023F6, 0x0000240E`

## Configuration CVars
- `_sofbuddy_hud_scale` - HUD element scaling factor (default: 1.0)
- `_sofbuddy_crossh_scale` - Crosshair scaling factor (default: 1.0)

## Dependencies
- **scaled_ui_base** - Uses shared Draw_StretchPic hook for CTF flag scaling
- Uses shared `glVertex2f` hook for vertex scaling
- Uses shared `g_activeDrawCall` and `g_activeRenderType` state for context tracking

## Technical Details

### HUD Component Scaling
Different HUD components use different scaling mechanisms:
- **Health/Armor/Ammo/Inventory**: Cropped pic vertex manipulation via custom ref.dll detours
- **Scoreboard**: Layout string scaling via `SCR_ExecuteLayoutString` hook
- **CTF Flag**: StretchPic scaling via shared `Draw_StretchPic` hook
- **Crosshair**: PicOptions scaling via `Draw_PicOptions` hook

### Render Type Tracking
The feature sets `g_activeRenderType` to identify which HUD component is being rendered:
- `HudCtfFlag` - CTF flag display
- `HudDmRanking` - Deathmatch ranking
- `HudInventory` - Inventory display
- `HudHealthArmor` - Health/armor display
- `Scoreboard` - Scoreboard display

This allows the shared `glVertex2f` hook to apply appropriate scaling based on context.

