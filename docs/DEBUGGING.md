# Debugging sof_buddy.dll with GDB under Wine

## Build Configurations

The project supports several debug build targets:

- **`make debug`** - Standard debug build with logging enabled
  - Equivalent to: `make BUILD=debug`
  - Includes: Debug symbols, logging, file/terminal output

- **`make debug-gdb`** - Debug build with GDB breakpoint function
  - Equivalent to: `make BUILD=debug-gdb`
  - Includes: Everything from `debug` plus `sofbuddy_debug_breakpoint()` function
  - Use this when debugging with GDB - you can set breakpoint on `sofbuddy_debug_breakpoint`

- **`make debug-collect`** - Debug build with func_parents collection enabled
  - Equivalent to: `make BUILD=debug-collect`
  - Includes: Everything from `debug` plus `SOFBUDDY_ENABLE_CALLSITE_LOGGER`
  - Use this to collect function parent caller information for analysis

## Quick Start

1. Build with debug symbols (from sof_buddy project directory):
   ```bash
   cd /path/to/sof_buddy
   make debug-gdb  # For GDB debugging
   # or
   make debug      # For standard debugging
   ```
   (Note: `make debug` is equivalent to `make BUILD=debug`)

2. Launch GDB from the SoF.exe directory:

   **Option A: From SoF.exe directory (recommended)**
   ```bash
   cd /path/to/SoF/game/directory
   gdb -x /path/to/sof_buddy/gdb_load_symbols.gdb --args wine SoF.exe
   ```

   **Option B: From sof_buddy project directory**
   ```bash
   cd /path/to/sof_buddy
   gdb -x gdb_load_symbols.gdb --args wine /path/to/SoF/game/directory/SoF.exe
   ```

3. Once the process starts and sof_buddy.dll is loaded, you have two options:

   **Option 1: Auto-detect and load (easiest)**
   ```gdb
   (gdb) load_dll_at
   ```
   This automatically finds sof_buddy.dll in process mappings and loads symbols.

   **Option 2: Filtered view then load**
   ```gdb
   (gdb) find_and_load_dll
   ```
   This shows only the filtered mappings for sof_buddy.dll (no scrolling needed) and displays the base address. Then:
   ```gdb
   (gdb) load_dll_at 0x<base_address> [dll_path]
   ```
   
   **If running from SoF.exe directory:**
   ```gdb
   (gdb) load_dll_at 0x9720000 sof_buddy.dll
   ```
   
   **If running from sof_buddy project directory:**
   ```gdb
   (gdb) load_dll_at 0x9720000
   ```
   Or explicitly:
   ```gdb
   (gdb) load_dll_at 0x9720000 bin/sof_buddy.dll
   ```

## Automatic Symbol Loading

For automatic symbol loading when the DLL loads, set a breakpoint:

```gdb
(gdb) break_dllmain
(gdb) run
(gdb) # When breakpoint hits:
(gdb) load_dll_at  # Auto-detects address and loads symbols
(gdb) continue
```

Or use the one-step command:
```gdb
(gdb) auto_load_dll_symbols
```

## Helper Commands

The GDB script provides these custom commands:

- **`bt`** - Enhanced backtrace with automatic module identification. Shows which module each address belongs to automatically.

- **`find_and_load_dll`** - Shows filtered process mappings (only lines containing sof_buddy.dll). No scrolling needed! Automatically extracts and displays the base address.

- **`load_dll_at [addr] [path]`** - Load symbols at specific address. If called with no arguments, auto-detects the address from process mappings and loads symbols automatically.
  - `load_dll_at` - Auto-detect and load
  - `load_dll_at 0x9720000` - Specify address, auto-detect path
  - `load_dll_at 0x9720000 sof_buddy.dll` - Specify both address and path

- **`auto_load_dll_symbols`** - One-step command that auto-detects and loads symbols.

- **`break_asserts`** - Set a breakpoint on `SOFBUDDY_ASSERT` failures. When an assert fires, automatically shows:
  - File name
  - Line number
  - Function name
  - Failed condition
  - Backtrace
  
  **Important:** Use this after loading symbols to catch all assert failures with full context.

- **`identify_address <addr>`** - Identify which module (DLL/EXE) an address belongs to. Useful when debugging crashes in game code or Wine libraries.

- **`bt_full`** - Enhanced backtrace with registers, disassembly, and memory mappings.

- **`break_dllmain`** - Set breakpoint at DllMain for early symbol loading.

## Tips

- Use `set print pretty on` for better C++ output
- Use `set print elements 0` to see full arrays/strings
- Use `info registers` to see CPU state
- Use `x/10i $pc` to disassemble at current instruction
- Use `info proc mappings` to see all loaded modules
- After loading symbols, function names will appear in backtraces instead of `?? ()`

## Example Workflow

```gdb
# Start GDB
gdb -x /path/to/sof_buddy/gdb_load_symbols.gdb --args wine SoF.exe

# Run the process
(gdb) run

# When it stops (crash, breakpoint, or SIGUSR1):
(gdb) auto_load_dll_symbols  # Load symbols
(gdb) break_asserts           # Set breakpoint on asserts
(gdb) continue                # Continue execution

# If an assert fires, GDB will automatically show:
# - File: src/features/xxx/file.cpp
# - Line: 42
# - Function: function_name
# - Condition: pointer != nullptr
# - Full backtrace

# Now backtraces show function names and modules
(gdb) bt

# For more details
(gdb) bt_full
```

## Debugging Assert Failures

When a `SOFBUDDY_ASSERT` fails, the improved assert system provides:

1. **Automatic breakpoint** - If you set `break_asserts`, GDB will stop at the assert
2. **Global variables** - Inspect assert details:
   ```gdb
   (gdb) print g_assert_file      # File where assert failed
   (gdb) print g_assert_line     # Line number
   (gdb) print g_assert_condition # Condition that failed
   (gdb) print g_assert_function  # Function name
   ```
3. **Full context** - The assert handler shows all information automatically

**Before (old system):** SIGUSR1 with no context, had to search logs
**After (new system):** GDB breaks with full assert information immediately

## Troubleshooting

**Problem: `sof_buddy.dll not found in process mappings`**
- Make sure the process is running and the DLL has been loaded
- Check that you're running from the correct directory
- Verify the DLL is in the expected location

**Problem: `Could not find sof_buddy.dll file`**
- If running from SoF.exe directory, use: `load_dll_at 0x<address> sof_buddy.dll`
- If running from project directory, use: `load_dll_at 0x<address> bin/sof_buddy.dll`
- Or specify the full path: `load_dll_at 0x<address> /full/path/to/sof_buddy.dll`

**Problem: Symbols loaded but still seeing `?? ()` in backtraces**
- The crash might be in game code (SoF.exe) or Wine libraries, not in sof_buddy.dll
- Use `identify_address 0x<address>` to see which module an address belongs to
- sof_buddy.dll symbols only help for functions within the DLL itself

## For Game Symbols (SoF.exe)

If you have debug symbols for SoF.exe, you can load them similarly:
```gdb
(gdb) info proc mappings  # Find SoF.exe base address
(gdb) add-symbol-file SoF.exe 0x<base_address>
```

Note: The game executable typically loads at 0x00400000 or similar (under Wine it may be different, check mappings).

