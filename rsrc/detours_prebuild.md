# Detours (prebuild scan)

- Generated: 2025-09-23T15:14:54Z
- Source: /home/dinda/git-projects/d3nd3/public/sof/sof_buddy/src
- Count: 24

### Sys_Mil_AfterHookThunk

- Features: Media Timers
- Descriptions:
  - Media Timers
  - Media Timers
- Example: `Sys_Mil_AfterHookThunk = DetourCreateRF(reinterpret_cast<void*>(0x20055930),(void*)&core_Sys_Milliseconds,DETOUR_TYPE_JMP,5,"Media Timers","Media Timers")`
- Active: 1

### orig_Cbuf_AddLateCommands

- Features: Media Timers
- Descriptions:
  - Post-cmdline init hook
- Example: `orig_Cbuf_AddLateCommands = DetourCreateRF(reinterpret_cast<void*>(0x20018740),&my_Cbuf_AddLateCommands,DETOUR_TYPE_JMP,5,"Post-cmdline init hook","Media Timers")`
- Active: 1

### orig_CinematicFreeze

- Features: Cinematic Freeze Fix
- Descriptions:
  - Cinematic Freeze Fix
- Example: `orig_CinematicFreeze = DetourCreateRF(reinterpret_cast<void*>(0x50075190),&core_CinematicFreeze,DETOUR_TYPE_JMP,7,"Cinematic Freeze Fix","Cinematic Freeze Fix")`
- Active: 1

### orig_Con_CheckResize

- Features: FEATURE_FONT_SCALING
- Descriptions:
  - Font Scaling
- Example: `orig_Con_CheckResize = DetourCreateRF((void * ) 0x20020880, (void * ) & my_Con_CheckResize, DETOUR_TYPE_JMP, 5, "Font Scaling", "FEATURE_FONT_SCALING")`
- Active: 1

### orig_Con_DrawConsole

- Features: FEATURE_FONT_SCALING
- Descriptions:
  - Font Scaling
- Example: `orig_Con_DrawConsole = DetourCreateRF((void * ) 0x20020F90, (void * ) & my_Con_DrawConsole, DETOUR_TYPE_JMP, 5, "Font Scaling", "FEATURE_FONT_SCALING")`
- Active: 1

### orig_Con_DrawNotify

- Features: FEATURE_FONT_SCALING
- Descriptions:
  - Font Scaling
- Example: `orig_Con_DrawNotify = DetourCreateRF((void * ) 0x20020D70, (void * ) & my_Con_DrawNotify, DETOUR_TYPE_JMP, 5, "Font Scaling", "FEATURE_FONT_SCALING")`
- Active: 1

### orig_Con_Init

- Features: FEATURE_FONT_SCALING
- Descriptions:
  - Font Scaling
- Example: `orig_Con_Init = DetourCreateRF((void * ) 0x200208E0, (void * ) & my_Con_Init, DETOUR_TYPE_JMP, 9, "Font Scaling", "FEATURE_FONT_SCALING")`
- Active: 1

### orig_DrawCroppedPicOptions

- Features: FEATURE_FONT_SCALING
- Descriptions:
  - Font Scaling
- Example: `orig_DrawCroppedPicOptions = DetourCreateRF((void * ) 0x30002240, (void * ) & my_DrawCroppedPicOptions, DETOUR_TYPE_JMP, 5, "Font Scaling", "FEATURE_FONT_SCALING")`
- Active: 1

### orig_DrawPicOptions

- Features: FEATURE_FONT_SCALING
- Descriptions:
  - Font Scaling
- Example: `orig_DrawPicOptions = DetourCreateRF((void * ) 0x30002080, (void * ) & my_DrawPicOptions, DETOUR_TYPE_JMP, 6, "Font Scaling", "FEATURE_FONT_SCALING")`
- Active: 1

### orig_DrawStretchPic

- Features: FEATURE_FONT_SCALING
- Descriptions:
  - Font Scaling
- Example: `orig_DrawStretchPic = DetourCreateRF((void * ) 0x30001D10, (void * ) & my_DrawStretchPic, DETOUR_TYPE_JMP, 5, "Font Scaling", "FEATURE_FONT_SCALING")`
- Active: 1

### orig_Draw_String_Color

- Features: FEATURE_FONT_SCALING
- Descriptions:
  - Font Scaling
- Example: `orig_Draw_String_Color = DetourCreateRF((void * ) 0x30001A40, (void * ) & my_Draw_String_Color, DETOUR_TYPE_JMP, 5, "Font Scaling", "FEATURE_FONT_SCALING")`
- Active: 1

### orig_FS_InitFilesystem

- Features: Core
- Descriptions:
  - Core filesystem init hook
- Example: `orig_FS_InitFilesystem = DetourCreateRF(reinterpret_cast<void*>(0x20026980), &my_FS_InitFilesystem,DETOUR_TYPE_JMP,6,"Core filesystem init hook","Core")`
- Active: 1

### orig_GL_BuildPolygonFromSurface

- Features: HD Textures
- Descriptions:
  - HD Textures
  - HD Textures
- Example: `orig_GL_BuildPolygonFromSurface = DetourCreateRF(reinterpret_cast<void*>(0x30016390),(void*)&my_GL_BuildPolygonFromSurface,DETOUR_TYPE_JMP,6,"HD Textures","HD Textures")`
- Active: 1

### orig_R_BlendLightmaps

- Features: FEATURE_ALT_LIGHTING
- Descriptions:
  - Alt Lighting
  - Alt Lighting
- Example: `orig_R_BlendLightmaps = DetourCreateRF(reinterpret_cast<void*>(0x30015440),(void*)&my_R_BlendLightmaps,DETOUR_TYPE_JMP,6,"Alt Lighting","FEATURE_ALT_LIGHTING")`
- Active: 1

### orig_R_DrawFont

- Features: FEATURE_FONT_SCALING
- Descriptions:
  - Font Scaling
- Example: `orig_R_DrawFont = DetourCreateRF((void * ) 0x300045B0, (void * ) & my_R_DrawFont, DETOUR_TYPE_JMP, 6, "Font Scaling", "FEATURE_FONT_SCALING")`
- Active: 1

### orig_SCR_ExecuteLayoutString

- Features: FEATURE_FONT_SCALING
- Descriptions:
  - Font Scaling
- Example: `orig_SCR_ExecuteLayoutString = DetourCreateRF((void * ) 0x20014510, (void * ) my_SCR_ExecuteLayoutString, DETOUR_TYPE_JMP, 8, "Font Scaling", "FEATURE_FONT_SCALING")`
- Active: 1

### orig_Sys_GetGameApi

- Features: Core
- Descriptions:
  - Game API hook
- Example: `orig_Sys_GetGameApi = DetourCreateRF(reinterpret_cast<void*>(0x20065F20),(void*)&my_Sys_GetGameApi,DETOUR_TYPE_JMP,5,"Game API hook","Core")`
- Active: 1

### orig_VID_CheckChanges

- Features: Renderer
- Descriptions:
  - VSync reapply on mode change
  - VSync reapply on mode change
- Example: `orig_VID_CheckChanges = DetourCreateRF(reinterpret_cast<void*>(0x200670C0),(void*)&my_VID_CheckChanges,DETOUR_TYPE_JMP,5,"VSync reapply on mode change","Renderer")`
- Active: 1

### orig_VID_LoadRefresh

- Features: Renderer
- Descriptions:
  - Renderer reload hook
  - Renderer reload hook
- Example: `orig_VID_LoadRefresh = DetourCreateRF(reinterpret_cast<void*>(0x20066E10),(void*)&my_VID_LoadRefresh,DETOUR_TYPE_JMP,5,"Renderer reload hook","Renderer")`
- Active: 1

### orig_cDMRanking_Draw

- Features: FEATURE_FONT_SCALING
- Descriptions:
  - Font Scaling
- Example: `orig_cDMRanking_Draw = DetourCreateRF((void * ) 0x20007B30, (void * ) & my_cDMRanking_Draw, DETOUR_TYPE_JMP, 6, "Font Scaling", "FEATURE_FONT_SCALING")`
- Active: 1

### orig_cHealthArmor2_Draw

- Features: FEATURE_FONT_SCALING
- Descriptions:
  - Font Scaling
- Example: `orig_cHealthArmor2_Draw = DetourCreateRF((void * ) 0x20008C60, (void * ) & my_cHealthArmor2_Draw, DETOUR_TYPE_JMP, 5, "Font Scaling", "FEATURE_FONT_SCALING")`
- Active: 1

### orig_cInventory2_And_cGunAmmo2_Draw

- Features: FEATURE_FONT_SCALING
- Descriptions:
  - Font Scaling
- Example: `orig_cInventory2_And_cGunAmmo2_Draw = DetourCreateRF((void * ) 0x20008430, (void * ) & my_cInventory2_And_cGunAmmo2_Draw, DETOUR_TYPE_JMP, 5, "Font Scaling", "FEATURE_FONT_SCALING")`
- Active: 1

### orig_drawTeamIcons

- Features: Teamicon Fix
- Descriptions:
  - Teamicon Fix
  - Teamicon Fix
- Example: `orig_drawTeamIcons = DetourCreateRF(reinterpret_cast<void*>(0x30003040),(void*)&my_drawTeamIcons,DETOUR_TYPE_JMP,6,"Teamicon Fix","Teamicon Fix")`
- Active: 1

### orig_glVertex2f

- Features: FEATURE_FONT_SCALING
- Descriptions:
  - Font Scaling
- Example: `orig_glVertex2f = DetourCreateRF((void * ) glVertex2f, (void * ) & my_glVertex2f, DETOUR_TYPE_JMP, DETOUR_LEN_AUTO, "Font Scaling", "FEATURE_FONT_SCALING")`
- Active: 1

