# list_match_fix

Patches `list_c` `<list ... match ... cvar ...>` behaviour so visiting a menu
no longer resets an off-list cvar value to `match[0]`.

## Problem

`list_c::GetMatchedValue` scans the cvar string against `match[]` and falls
back to index `0` when nothing matches. `Reinitialise`/`Cleanup` then call
`SetValue`, which runs `Cvar_Set(name, match[0])` — opening a page silently
rewrites the user's cvar (e.g. a custom `cl_maxfps`, FOV, or filter value).

## Fix (SoF.exe 1.06a, ImageBase 0x20000000)

Four `__thiscall` overrides (`detours.yaml`, module `SofExe`):

| Engine fn | VA | Role |
|-----------|----|------|
| `GetMatchedValue` | `0x200D3830` | return `-1` when match-backed but unmatched (disambiguates fallback `0` by comparing cvar string to `match[0]`; bitmask / no-match / null-cvar keep `0`) |
| `SetValue` | `0x200D3640` | no-op when index is `-1` (preserves cvar; width left as `Setup` left it) |
| `Handle` | `0x200D39E0` | param `1` = left, `2` = right. Matched rows run original. Unmatched rows compute next-highest (left) / next-lowest (right) by value, bias `curIndex` by ∓1, then run original so its `++/--`, `SetValue`, sound and `key` dispatch land exactly |
| `Draw` | `0x200D3AE0` | unmatched rows graft the live cvar string onto display element 0 and draw index 0, then restore — the page shows the unmatched value instead of label[0] |

`list_c` offsets used: `+0x50` cvar, `+0x9C` index (`-1` = unmatched),
`+0xA8` bitmask, `+0xB0/0xB4` display First/Last, `+0xC0/0xC4` match
First/Last, stride `0x10`, string pointer at `+4`; `cvar_t` `+0` name,
`+4` string.

## Ordering

Mode follows the cvar value: if it starts with a number (`strtod` consumes),
order numerically by leading double; otherwise case-insensitive lex order
(`_stricmp`, matching the engine's match compare). Next-highest = smallest
match strictly greater (wrap to smallest); next-lowest = largest strictly
smaller (wrap to largest). Numeric skips non-numeric matches; if none parse,
falls back to lex.

## Notes

- `cvari`-without-`match`, bitmask rows, and null-cvar rows are untouched.
- Width for unmatched rows is not recomputed (stays max-label based from
  `Setup`); it corrects itself once a listed value is picked.
- `Reinitialise` (`0x200D3940`) / `Cleanup` (`0x200D3960`) need no hook:
  `index = GetMatchedValue(); SetValue();` already preserves via the above.
