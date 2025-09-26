# Cvar notes (summary)

This document summarizes key behavior and gotchas for the cvar API used in the codebase.

- Purpose
  - `Cvar_Get(name, default, flags, callback)`
    - Used to declare/create a cvar and install flags/callback pointers.
    - On creation: the cvar's `modified` is set and the provided `callback` (if non-NULL) is executed immediately.
    - If the cvar already exists, `Cvar_Get` will not replace the stored `string`/`value`; it merges flags and installs/overrides the callback pointer. In this case a callback is invoked only if `cvar->modified == true` and a callback is present.
  - `Cvar_Set` / `Cvar_Set2(name, value, force)`
    - Used to change a cvar's value at runtime.
    - `Cvar_Set2` will call `Cvar_Get` if the cvar does not yet exist (so a Set may create then return).
    - Callbacks on Set are fired only when the new value differs from the old value (and a callback exists).

- CVAR_INTERNAL and `force` semantics
  - `CVAR_INTERNAL` marks variables that are not user-visible and can prevent updating the stored numeric `value` in `Cvar_Set2`.
  - Using `force > 1` can override internal protection, but should be used sparingly (may bypass latched handling or leak).

- `modified` behavior
  - `modified` is set when a cvar is created and is sticky; there is no single global place that always clears it. Repeated calls to `Cvar_Get` (or creation followed by immediate operations in the same frame) can cause callbacks to run if `modified` remains true.

- Callback and NULL-callback quirks
  - Passing `NULL` for the callback argument to `Cvar_Get` does NOT remove a previously installed callback; a later non-NULL callback call will overwrite it.
  - A cvar's modified callback should not rely on the caller's global pointer to the cvar being assigned — the callback can run before that pointer is written.

- Archive/race guidance
  - Cvars that change other cvars should not both be `CVAR_ARCHIVE`, because the order of entries in config files matters and archived cvars may conflict.

- Startup / config execution order (important)
  1) `default.cfg`
  2) `config.cfg`
  3) command-line `+set` entries
  4) `autoexec.cfg`
  5) other command-line `+cmd` entries

Use this file as a quick reference; the original detailed notes are preserved inline in `src/cvars.cpp` for more verbose historical context.
