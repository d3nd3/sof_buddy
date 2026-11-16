# sound_alt_tab_fix

Stops sound effects that were held while the game window was in the background
from all playing at once when the window is focused again.

## Problem

Defsnd (the default SoF sound library) releases its DirectSound buffers in
`S_Activate(0)` when the window loses focus. While it is inactive, `S_Update`
returns before painting, so queued `S_StartSound` requests stay in the 128-entry
pending list. On `S_Activate(1)` the buffers are recreated, and the next
`S_Update` sees every held request as due and issues them together. Long
Alt-Tab sessions therefore end with a burst of stacked gunfire, ambience and
other effects.

This is separate from **Quiet Alt-Tab sound messages**
(`_sofbuddy_snd_quiet_alt_tab`): that option only filters three `Com_Printf`
lines printed while DirectSound buffers are recreated.

## Fix

SoF.exe keeps the sound DLL exports in function-pointer globals. After
`LoadSoundModule` resolves them, this feature wraps `S_Activate`:

1. Call the original `S_Activate`.
2. When `active` is non-zero, call `S_StopAllSounds`.

`S_StopAllSounds` clears the pending starts, mixing channels and the output
buffer, so nothing accumulated in the background is issued. Dynamic music keeps
its own state and ambient loops are regenerated on the next frame.

The wrapper is reinstalled after every `LoadSoundModule`, so `snd_restart` and
sound-library reloads keep the fix. EAXSnd is left alone: its `S_Activate` is a
no-op and its effects keep mixing while inactive, so it has nothing queued to
flush.

## Engine locations (SoF.exe 1.06a, ImageBase 0x20000000)

| Item | RVA | Notes |
|------|-----|-------|
| `LoadSoundModule` | `0x00055C20` | Post hook resolves the new sound DLL exports |
| `S_Activate` pointer | `0x00390D6C` | Called from `MainWndProc` on `WM_ACTIVATE` |
| `S_StopAllSounds` pointer | `0x00390D4C` | Clears pending sounds, channels and DMA buffer |

## Verification

1. Enable the feature, load a level such as `arm1` and Alt-Tab out for a few
   minutes.
2. Return to the game. The background sounds no longer fire together.
3. Run `snd_restart` and repeat; the fix is reinstalled with the sound library.
