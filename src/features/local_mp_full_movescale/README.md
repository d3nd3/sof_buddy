# Local MP Full Movescale

Fixes locally hosted multiplayer games running at the saved singleplayer-era
`Movescale` speed (the "Half" option, 0.65) instead of full multiplayer speed.

## Why

SoF has two different move-scale paths:

- **Singleplayer / co-op** (`deathmatch 0`) uses the game DLL's `dmnone_c`
  gamemode, which returns a fixed movescale of `0.5625` (co-op overrides it to
  `1.0`). This is intentional singleplayer pacing.
- **Multiplayer** uses `gamerules_c::clientGetMovescale`, which reads the
  `g_movescale` cvar (`gamerules_c` returns `g_movescale * (1 - ps->movescale)`).

`g_movescale` is registered by the engine as archived + serverinfo, default
`1.0`, and the game's Create Server menu exposes it as **Movescale : Half
(0.65) / Full (1.0)**. A local listen server reuses the host's saved cvar, so a
leftover Half value silently makes the whole local game run at 65% speed —
almost exactly the singleplayer 56.25%, hence "runs slow as if singleplayer".

The scale reaches both sides through the same value:

- Server: `ClientThink` (`gamex86 0x500F53A0`) scales forward/side move by
  `clientGetMovescale()` and calls `gi_pMove`.
- Client prediction: `ClientEndServerFrame` (`gamex86 0x500F9A00`) writes the
  scale into `playerState + 0x1A` (`movescale * 255`), and
  `CL_PredictMovement` scales the usercmd by `byte / 255`.
- Remote clients are unaffected: their prediction byte comes from the remote
  server, not their own config.

## Fix

- New `local_mp_full_movescale` feature (F12 → Features → Gameplay, enabled by
  default).
- `GameDllLoaded` (after `gamex86.dll` loads, before `InitGame`) checks:
  - feature and `_sofbuddy_local_mp_full_movescale` are enabled;
  - `deathmatch != 0` (multiplayer only);
  - `dedicated == 0` (listen server only);
  - `g_movescale != 1.0`.
- The current value is saved, then `g_movescale` is set to `1.0` for the life of
  that server. The saved value is restored on `SV_ShutdownGameProgs`, so the
  user's config is not permanently rewritten.
- A console line reports the override. Setting `_sofbuddy_local_mp_full_movescale
  0` keeps the saved Movescale behaviour.

## Menu

- **F12 → Gameplay → Multiplayer → Local MP movescale : Full / Saved**
  (default `Full`).
- The raw cvar is also listed on F12 → Cvars.
