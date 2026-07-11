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

## Fixed

- [GitHub Issue #3](https://github.com/d3nd3/sof_buddy/issues/3) — Black loading after cinematic
