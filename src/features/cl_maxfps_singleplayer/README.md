# cl_maxfps Singleplayer Fix

## Purpose

Makes `cl_maxfps` work in singleplayer and fixes the black screen after mission-end cinematics.

## Root cause (black screen)

With SP frame limiting, `CL_Frame` often early-returns. Script-queued `intermission` then runs at the **next** `Qcommon_Frame` top `Cbuf_Execute` instead of inside `CL_Frame` → `CL_SendCommand` → `Cbuf_Execute` (after `CL_ReadPackets`).

`M_PushMenu` silently bails if exe `cl.ps.cinematicfreeze` is still set while `game.cinematicfreeze` is already 0.

## Callbacks

- **EarlyStartup** (Post, Priority: 70)
  - NOPs `CL_Frame` listen-server bypass so `cl_maxfps` applies in SP
  - Detours sofplus `sp_Sys_Mil` to zero `_sp_cl_frame_delay`

## Hooks

- **CinematicFreeze** (Post) — mirror freeze state to exe immediately
- **M_PushMenu** (Pre) — if game freeze is clear but exe flag is set, clear exe before menu push

## Addresses

- `CL_Frame` NOP: `0x0000D973`, `0x0000D974`
- Exe freeze: `0x001E7F5B`
- Game freeze: `0x0015D8D5`
- sofplus `sp_Sys_Mil`: `+0xFA60`; `_sp_cl_frame_delay`: `+0x331BC`

## Exact FPS cap (optional)

When **Singleplayer Max FPS** is enabled, **F12 → CPU** also offers **Exact FPS cap (try it)** (`_sofbuddy_cl_maxfps_exact`). With it on, `cl_maxfps` is a literal integer FPS (10–360, e.g. `77` caps near 77 FPS). With it off, list values use legacy semantics (e.g. `63` caps near **62.5** FPS because the budget is `ceil(1000/63)` ms). SoFPlus `_sp_cl_frame_delay` is cleared in exact mode so it does not apply a second `ceil(1000/fps)` limiter.

## Fixed

- [GitHub Issue #3](https://github.com/d3nd3/sof_buddy/issues/3) — Black loading after cinematic
