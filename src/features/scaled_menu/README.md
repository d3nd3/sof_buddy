# Scaled Menu

## Purpose
Scales menu elements to be readable and properly sized on modern high-resolution displays. Handles menu background tiling, menu item scaling, and layout adjustments.

## Callbacks
- **EarlyStartup** (Post, Priority: 70)
  - `scaledMenu_EarlyStartup()` - Initializes menu scaling system

## Hooks
- **VID_CheckChanges** (Post, Priority: 100)
  - `vid_checkchanges_post()` - Updates screen dimensions when video mode changes

## OverrideHooks
None (uses shared hooks from scaled_ui_base)

## CustomDetours
None

## Configuration CVars
None (uses screen scaling factors from scaled_ui_base)

## Dependencies
- **scaled_ui_base** - Heavily depends on shared infrastructure:
  - Uses `Draw_Pic` hook for menu background tiling detection
  - Uses `GL_FindImage` Post hook for menu image dimension extraction
  - Uses `glVertex2f` hook for menu vertex scaling
  - Uses `mainMenuBgTiled` flag for background tiling mode

## Technical Details

### Menu Background Tiling
The shared `Draw_Pic` hook in `scaled_ui_base` detects menu background images (paths containing `backdope/web_bg`) and sets the `mainMenuBgTiled` flag. This flag is used by the shared `glVertex2f` hook to apply tiling logic instead of scaling.

### Image Dimension Extraction
The shared `GL_FindImage` Post hook extracts image dimensions from loaded menu images. When `mainMenuBgTiled` is true, dimensions are scaled by `screen_y_scale` for proper tiling.

### Menu Item Scaling
Menu items are scaled through the shared `glVertex2f` hook based on the `DrawRoutineType::Pic` context and `mainMenuBgTiled` state.

## Note
This feature requires the `UI_MENU` compile-time flag to be enabled. Menu scaling functionality is conditionally compiled based on this flag.

