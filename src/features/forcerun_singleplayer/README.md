# Force Run Singleplayer

Makes the singleplayer player run by default instead of walking. The stock
`cl_run 0` default means the player walks unless the run key is held; SoF Plus's
Community Edition sets `cl_run 1` for its first-run defaults, so players without
that config move slowly in the campaign.

## Callbacks

- **PostCvarInit** (Post, Priority: 50)
  - `forcerun_PostCvarInit()` - registers `_sofbuddy_forcerun_singleplayer`
    (archived, default `0`)

## Hooks

- **CL_BaseMove** (`SoF.exe`, `0x00004B20`, override)
  - `hkCL_BaseMove()` - sets `cl_run` to `1` for the duration of the command when
    the feature and the F12 option are enabled in singleplayer. The engine reads
    `cl_run` at the end of `CL_BaseMove` to set the +speed button bit, so this is
    exactly the SoF Plus CE `cl_run 1` behaviour without changing the user's
    saved cvar. The engine's walk key still slows the player down.

## Behaviour

- **F12 → Gameplay → Singleplayer → Force movespeed** (`Off`/`On`, default
  `Off`).
- Applies immediately; no restart or map change is needed.
- Multiplayer is never touched: the hook requires both a live local server
  (`SoF.exe` `sv.state` = `ss_loading`/`ss_game`, `0x203A1F20`) and
  `deathmatch == 0`. A remote client leaves `sv.state` at `ss_dead`, so changing
  the `deathmatch` cvar from the console cannot enable the feature in
  multiplayer.
- The walk key still works, so the option behaves like `cl_run 1` rather than
  removing the ability to walk.
