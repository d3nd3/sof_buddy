# cbuf_limit_increase

## Deprecation
This feature is deprecated and disabled by default.

Reason:
- SoF Buddy now persists buddy cvars into `SoFDirRoot/{basedir}/base/sofbuddy.cfg`.
- `sofbuddy.cfg` is executed automatically after `FS_InitFilesystem`.
- Buddy cvars no longer rely on `CVAR_ARCHIVE`/`config.cfg`, so the original `exec config.cfg` Cbuf overflow workaround is no longer needed.

## Purpose
Mitigates command-buffer overflows (`Cbuf_AddText: overflow`) that prevent large script files (for example `exec config.cfg`) from being executed.

This does **not** literally resize the engine's fixed Cbuf. Instead it bypasses the overflow path by:
- Intercepting `exec config.cfg` (`Cmd_Exec_f`) and executing the file contents without using the engine Cbuf.
- Using a small "virtual Cbuf" that dispatches each command via `Cmd_ExecuteString`.

## Callbacks
- **EarlyStartup** (Post, Priority: 72)
  - `cbuf_limit_increase_EarlyStartup()`
    - Detours `Cmd_Exec_f` (bypass/stabilize only `exec config.cfg` via `FS_LoadFile` + virtual execution)
    - Detours `Cbuf_AddText` / `Cbuf_InsertText` (only special-cased while executing `config.cfg`)
    - Detours `Com_Printf` in debug builds only (diagnostic: detect/report overflow print callsite)

## Addresses (IDA)
Hardcode these RVAs in `src/features/cbuf_limit_increase/callbacks/cbuf_limit_increase_early.cpp`:
- `kCbufAddTextRva`
- `kCbufInsertTextRva`
- `kCmdExecfRva`
- `kCmdExecuteStringRva`
- `kComPrintfRva`
- `kFsLoadFileRva`
- `kFsFreeFileRva`

They are RVAs relative to `SoF.exe` base (commonly `0x20000000`).

Example values that worked on one build:
```text
Cbuf_AddText       0x00018180
Cbuf_InsertText    0x000181D0
Cmd_Exec_f         0x00018930
Cmd_ExecuteString  0x000194F0
Com_Printf         0x0001C6E0
FS_LoadFile        0x00025370
FS_FreeFile        0x00025420
```

## Behavior

### 1) `exec` bypass (`Cmd_Exec_f` hook)
When the player runs `exec <file>`:
- This feature intentionally only applies special handling when `<file>` is `config.cfg` (case-insensitive basename match).
- For `exec config.cfg`:
  - The hook loads the file via the engine filesystem (`FS_LoadFile`), so the search path matches the game (paks, gamedirs, etc).
  - It **does not call the original `Cmd_Exec_f`** if the load succeeds.
  - It pushes the contents into an internal string buffer (`s_virtual_cmd_text`) and drains it by splitting commands and dispatching each one via `Cmd_ExecuteString(cmd)`.
- If `FS_LoadFile` fails, it falls back to the original engine `Cmd_Exec_f` (which may still overflow on very large configs).
- For any other `exec <file>`, it calls the original engine `Cmd_Exec_f` unchanged.

### 2) Virtual Cbuf draining
The virtual buffer drain logic is meant to approximate the engine's `Cbuf_Execute` splitting rules:
- Commands are split on `\n` always.
- Commands are split on `;` only when not inside quotes (`"` toggles quote state).
- Each extracted command is whitespace-trimmed and dispatched via `Cmd_ExecuteString`.
- A safety guard aborts the drain after 50,000 commands to avoid pathological infinite execution.

### 3) Streaming `Cbuf_AddText` / `Cbuf_InsertText`
Outside of `exec config.cfg` (and outside of virtual-buffer draining), the original engine functions are used.

For config-related exec content (while inside `exec config.cfg`, or while draining the virtual buffer):
- `Cbuf_AddText` appends text to the virtual buffer and drains what it can.
- `Cbuf_InsertText` inserts text at the front of the virtual buffer (preserving "insert before queued text" semantics) and drains what it can.

This exists because some SoF builds appear to inline Cbuf writes inside `Cmd_Exec_f`, so detouring only `Cbuf_AddText` may not catch all overflow paths.

### 4) Overflow diagnostics (`Com_Printf` hook)
If the engine prints `Cbuf_AddText: overflow`, the `Com_Printf` hook logs:
- the callsite return address (and RVA)
- whether we were inside an exec (`exec_depth`)
- current virtual buffer length

This was used to confirm the overflow came from an inlined callsite inside `Cmd_Exec_f` in one build (caller RVA `0x18A59`).

## Notes
- The feature name is historical: it avoids the overflow without patching the engine Cbuf size.
- Streaming only triggers while executing `config.cfg` (or while draining the virtual buffer). Outside of that, `Cbuf_AddText` / `Cbuf_InsertText` behave like vanilla.
- If you still see `Cbuf_AddText: overflow` after enabling this feature, the most common cause is that `FS_LoadFile` failed and we fell back to the original engine `Cmd_Exec_f`.

## Risks / Side Effects
This feature detours core engine functions. It is intentionally conservative most of the time, but there are real behavior differences to be aware of.

Likely/realistic risks:
- **Execution semantics differences:** when the bypass path is taken (`exec config.cfg`), commands are executed via `Cmd_ExecuteString` in a tight loop, not via the engine Cbuf. If your `config.cfg` relies on subtle Cbuf behavior (for example any "yield to next frame" / `wait`-style behavior, if present in your build), results may differ.
- **Parser edge cases:** command splitting is based on `\n` and `;` outside quotes. If your scripts rely on unusual quoting/escaping/comment rules that differ from the engine's `Cbuf_Execute`, they may execute differently.
- **Frame hitching:** very large configs can execute all at once, which can cause a short freeze while thousands of commands are processed.
- **Detour conflicts:** other injected DLLs that also detour `Cmd_Exec_f`, `Cbuf_*`, or `Com_Printf` may conflict depending on patch order.
- **Safety guard truncation:** the 50,000-command guard will abort draining if hit, leaving the script partially executed.

## Reducing Risk (Options)
If you want to reduce the chance of side effects, these are the most practical next steps:
- **Keep it config.cfg-only:** the current implementation intentionally limits behavior changes to `exec config.cfg` to minimize stability risk.
- **Keep `Com_Printf` debug-only:** the overflow callsite logger is extremely useful for diagnosis but it detours a very hot function. Compiling it only in debug builds reduces the surface area for conflicts/perf overhead.
- **Only bypass when necessary:** keep original `Cmd_Exec_f` for small scripts and only use the bypass when the file is large (or only for `config.cfg`). That minimizes semantic differences for typical `exec` usage.
- **Drain over multiple frames:** cap how many commands are executed per drain and continue draining from `Qcommon_Frame` (or a similar per-frame hook). This reduces hitching and can better match any engine "wait/yield" semantics.
