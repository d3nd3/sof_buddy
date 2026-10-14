# GDB script to load symbols for sof_buddy.dll debugging under Wine
# Usage from sof_buddy project directory:
#   gdb -x gdb_load_symbols.gdb --args wine /path/to/SoF.exe
# Usage from SoF.exe directory:
#   gdb -x /path/to/sof_buddy/gdb_load_symbols.gdb --args wine SoF.exe

# Set up automatic symbol loading
set auto-load safe-path /
set solib-search-path bin:.

# Enable better backtrace
set print pretty on
set print elements 0
set print null-stop on
set print frame-arguments all

# Auto-find and load DLL symbols (one-step command)
define auto_load_dll_symbols
    printf "Auto-detecting and loading sof_buddy.dll symbols...\n"
    # Just call load_dll_at with no args to trigger auto-detection
    load_dll_at
    # break_asserts is automatically set by load_dll_at
end

# Helper to find and load DLL symbols from mappings (filtered output)
define find_and_load_dll
    printf "Finding sof_buddy.dll base address...\n"
    printf "Filtered process mappings for sof_buddy.dll:\n"
    printf "==========================================\n"
    python
import gdb
import re

try:
    # Get process mappings
    mappings_output = gdb.execute("info proc mappings", to_string=True)
    
    # Filter for sof_buddy.dll
    found = False
    for line in mappings_output.split('\n'):
        if 'sof_buddy.dll' in line.lower():
            found = True
            # Extract base address (first hex number)
            match = re.search(r'0x([0-9a-fA-F]+)', line)
            if match:
                base_addr = match.group(0)
                print(line)
                print(f"\nBase address found: {base_addr}")
                print(f"Use: load_dll_at {base_addr} [dll_path]")
            else:
                print(line)
    
    if not found:
        print("sof_buddy.dll not found in process mappings.")
        print("Make sure the DLL is loaded (run the process first).")
except Exception as e:
    print(f"Error: {e}")
    print("Showing full mappings instead:")
    gdb.execute("info proc mappings")
end
    printf "==========================================\n"
end

# Enhanced backtrace with automatic module identification
define bt
    dont-repeat
    python
import gdb
import re

try:
    # Get process mappings first (needed for module identification)
    mappings_output = gdb.execute("info proc mappings", to_string=True, from_tty=False)
    
    # Helper function to extract module name from path
    def extract_module_name(module_path):
        if not module_path:
            return None
        if '/' in module_path:
            return module_path.split('/')[-1]
        elif '\\' in module_path:
            return module_path.split('\\')[-1]
        return module_path
    
    # Parse mappings into a list of (start, end, module) tuples
    # Also track module ranges for better identification of anonymous memory
    mappings = []
    module_ranges = {}  # Track overall ranges for each module
    
    for line in mappings_output.split('\n'):
        parts = line.split()
        if len(parts) >= 2:
            try:
                start_str = parts[0]
                end_str = parts[1]
                start_addr = int(start_str, 16) if start_str.startswith('0x') else int(start_str, 16)
                end_addr = int(end_str, 16) if end_str.startswith('0x') else int(end_str, 16)
                module_name = parts[-1] if len(parts) > 0 else "unknown"
                mappings.append((start_addr, end_addr, module_name, line.strip()))
                
                # Track module ranges (extract filename from path)
                module_short = extract_module_name(module_name)
                
                # Only track actual modules, not just permissions
                if module_short and module_short not in ['rwxp', 'r-xp', 'r--p', '---p', 'rw-p', 'unknown']:
                    if module_short not in module_ranges:
                        module_ranges[module_short] = (start_addr, end_addr)
                    else:
                        # Expand range
                        old_start, old_end = module_ranges[module_short]
                        module_ranges[module_short] = (min(old_start, start_addr), max(old_end, end_addr))
            except (ValueError, IndexError):
                continue
    
    # Helper function to identify module for an address
    def identify_module(addr_str):
        try:
            addr = int(addr_str, 16) if addr_str.startswith('0x') else int(addr_str, 16)
            
            # First pass: find exact match (address within a mapped region)
            for start, end, module, full_line in mappings:
                if start <= addr < end:
                    # Extract just the filename from the path
                    module_short = extract_module_name(module)
                    # If it's not just permissions (rwxp, r-xp, etc), return the module
                    if module_short and module_short not in ['rwxp', 'r-xp', 'r--p', '---p', 'rw-p']:
                        return module_short
                    # If it's anonymous memory, continue to second pass
            
            # Second pass: check if address is within any module's overall range
            # (for anonymous memory regions that are part of a module's address space)
            for module_name, (mod_start, mod_end) in module_ranges.items():
                if mod_start <= addr < mod_end:
                    return f"{module_name} (anonymous region)"
            
            # Third pass: find nearest module (for addresses outside module ranges)
            nearest_module = None
            nearest_distance = None
            
            for module_name, (mod_start, mod_end) in module_ranges.items():
                # Check if address is near this module's range
                if addr >= mod_start and addr < mod_end + 0x1000000:  # Within 16MB after module end
                    distance = addr - mod_end if addr >= mod_end else 0
                    if nearest_distance is None or distance < nearest_distance:
                        nearest_distance = distance
                        nearest_module = module_name
                elif addr < mod_start and mod_start - addr < 0x100000:  # Within 1MB before module
                    distance = mod_start - addr
                    if nearest_distance is None or distance < nearest_distance:
                        nearest_distance = distance
                        nearest_module = module_name
            
            if nearest_module and nearest_distance and nearest_distance < 0x1000000:  # Within 16MB
                return f"{nearest_module} (nearby, +0x{nearest_distance:x})"
            
            # Check if it's a very low address (likely invalid/corrupted)
            if addr < 0x1000:
                return "invalid/corrupted (very low address)"
            return None
        except ValueError:
            return None
    
    # Get backtrace using the original GDB command
    bt_output = gdb.execute("backtrace", to_string=True, from_tty=False)
    
    # Process each line of backtrace
    lines = bt_output.split('\n')
    stack_corrupted = False
    
    for line in lines:
        if not line.strip():
            print()
            continue
        
        # Check for stack corruption messages
        if "Cannot access memory" in line or "corrupt stack" in line or "inner to this frame" in line:
            stack_corrupted = True
            print(line)
            
            # Try to extract the problematic address
            match = re.search(r'0x([0-9a-fA-F]+)', line)
            if match:
                addr_str = match.group(0)
                module_info = identify_module(addr_str)
                if module_info:
                    indent = " " * (len(line) - len(line.lstrip()) + 6)
                    print(f"{indent}-> {addr_str} is {module_info}")
            
            # Show additional info when stack is corrupted
            print("\n⚠️  Stack appears corrupted. Additional information:")
            try:
                # Show registers that might contain return addresses
                regs_output = gdb.execute("info registers", to_string=True, from_tty=False)
                # Look for EIP/RIP and EBP/RBP
                for reg_line in regs_output.split('\n'):
                    if any(reg in reg_line for reg in ['eip', 'rip', 'ebp', 'rbp', 'esp', 'rsp']):
                        print(f"  {reg_line.strip()}")
                        # Try to identify module for register values
                        match = re.search(r'0x([0-9a-fA-F]+)', reg_line)
                        if match:
                            addr_str = match.group(0)
                            module_info = identify_module(addr_str)
                            if module_info:
                                print(f"    -> {addr_str} is in {module_info}")
            except:
                pass
            continue
        
        print(line)
        
        # Look for addresses in the line (format: #N  0xADDRESS in ?? ())
        matches = re.finditer(r'0x([0-9a-fA-F]+)', line)
        for match in matches:
            addr_str = match.group(0)
            module_info = identify_module(addr_str)
            
            if module_info:
                # Only show if it's not already showing a function name (not ??)
                if '??' in line or 'in ()' in line or stack_corrupted:
                    indent = " " * (len(line) - len(line.lstrip()) + 6)
                    print(f"{indent}-> {addr_str} is in {module_info}")
    
    if stack_corrupted:
        print("\n💡 Tip: Stack corruption often indicates:")
        print("  - Buffer overflow")
        print("  - Invalid function pointer")
        print("  - Memory corruption")
        print("  - Use 'info registers' and 'x/20x $esp' to inspect stack")

except Exception as e:
    # Fallback to regular bt if there's an error
    print(f"Error in enhanced bt: {e}")
    gdb.execute("backtrace", from_tty=False)
end
end

# Enhanced backtrace with symbol resolution and module info
define bt_full
    printf "=== Enhanced Backtrace with Module Info ===\n"
    bt
    printf "\n=== Registers ===\n"
    info registers
    printf "\n=== Current Instruction ===\n"
    x/10i $pc
end

# Auto-find and load DLL symbols
# Usage: load_dll_at [base_address] [dll_path]
# If no address provided, automatically searches for sof_buddy.dll in mappings
define load_dll_at
    if $argc == 0
        printf "Auto-detecting sof_buddy.dll...\n"
        python
import gdb
import re
import os

try:
    # Get process mappings
    mappings_output = gdb.execute("info proc mappings", to_string=True)
    
    # Find sof_buddy.dll and extract base address
    base_addr = None
    for line in mappings_output.split('\n'):
        if 'sof_buddy.dll' in line.lower():
            match = re.search(r'0x([0-9a-fA-F]+)', line)
            if match:
                base_addr = match.group(0)
                break
    
    if base_addr:
        # Try to find DLL file
        dll_path = None
        if os.path.exists("bin/sof_buddy.dll"):
            dll_path = "bin/sof_buddy.dll"
        elif os.path.exists("sof_buddy.dll"):
            dll_path = "sof_buddy.dll"
        
        if dll_path:
            print(f"Found sof_buddy.dll at {base_addr}")
            print(f"Loading symbols from {dll_path}...")
            gdb.execute(f"add-symbol-file {dll_path} {base_addr}")
            print("Symbols loaded! Function names should now be visible in backtraces.")
            # Automatically set breakpoint on asserts
            try:
                gdb.execute("break sofbuddy_assert_failed", to_string=True, from_tty=False)
                gdb.execute("commands", to_string=True, from_tty=False)
                gdb.execute("printf \"\\n=== ASSERT FAILED ===\\n\"", to_string=True, from_tty=False)
                gdb.execute("printf \"File: %%s\\n\", g_assert_file", to_string=True, from_tty=False)
                gdb.execute("printf \"Line: %%d\\n\", g_assert_line", to_string=True, from_tty=False)
                gdb.execute("printf \"Function: %%s\\n\", g_assert_function", to_string=True, from_tty=False)
                gdb.execute("printf \"Condition: %%s\\n\", g_assert_condition", to_string=True, from_tty=False)
                gdb.execute("printf \"\\nShow backtrace: (gdb) bt\\n\\n\"", to_string=True, from_tty=False)
                gdb.execute("end", to_string=True, from_tty=False)
                print("✓ Auto-set breakpoint on sofbuddy_assert_failed")
            except:
                pass
        else:
            print(f"Found base address: {base_addr}")
            print("But could not find sof_buddy.dll file.")
            print("Please specify path: load_dll_at <address> <path>")
    else:
        print("sof_buddy.dll not found in process mappings.")
        print("Make sure the DLL is loaded (run the process first).")
        print("Usage: load_dll_at <base_address_in_hex> [dll_path]")
        print("Example: load_dll_at 0x9720000 sof_buddy.dll")
except Exception as e:
    print(f"Error during auto-detection: {e}")
    print("Usage: load_dll_at <base_address_in_hex> [dll_path]")
    print("Example: load_dll_at 0x9720000 sof_buddy.dll")
end
    else
        set $dll_path = "bin/sof_buddy.dll"
        if $argc >= 2
            set $dll_path = $arg1
        end
        printf "Loading sof_buddy.dll symbols at address %s from %s\n", $arg0, $dll_path
        add-symbol-file $dll_path $arg0
        printf "Symbols loaded! Function names should now be visible in backtraces.\n"
        # Automatically set breakpoint on asserts
        _auto_break_asserts
    end
end

# Set breakpoint on assert failures (can be called manually or is auto-set when symbols load)
define break_asserts
    # Remove existing breakpoint if any
    python
import gdb
try:
    # Try to delete existing breakpoint
    gdb.execute("delete breakpoints sofbuddy_assert_failed", to_string=True, from_tty=False)
except:
    pass
end
    break sofbuddy_assert_failed
    commands
        printf "\n=== ASSERT FAILED ===\n"
        printf "File: %s\n", g_assert_file
        printf "Line: %d\n", g_assert_line
        printf "Function: %s\n", g_assert_function
        printf "Condition: %s\n", g_assert_condition
        printf "\nInspect variables:\n"
        printf "  (gdb) print g_assert_file\n"
        printf "  (gdb) print g_assert_line\n"
        printf "  (gdb) print g_assert_condition\n"
        printf "  (gdb) print g_assert_function\n"
        printf "\nShow backtrace:\n"
        printf "  (gdb) bt\n"
        printf "\n"
    end
    printf "Breakpoint set on sofbuddy_assert_failed - will show assert details when hit\n"
end

document break_asserts
Set a breakpoint on assert failures that automatically shows assert information.
Use this after loading symbols to catch all SOFBUDDY_ASSERT failures.
end

# Auto-set breakpoint on DLL load (debug builds only)
# This breakpoint is automatically triggered by sof_buddy.dll when it loads
define break_on_dll_load
    break sofbuddy_debug_breakpoint
    commands
        printf "\n=== sof_buddy.dll loaded - Auto-loading symbols ===\n"
        # Automatically load symbols
        auto_load_dll_symbols
        printf "\nSymbols loaded! You can now:\n"
        printf "  - Set breakpoints: break function_name\n"
        printf "  - Set assert breakpoint: break_asserts\n"
        printf "  - Continue: continue\n"
        printf "\n"
    end
    printf "Breakpoint set on sofbuddy_debug_breakpoint - will auto-load symbols when DLL loads\n"
end

document break_on_dll_load
Set breakpoint on sofbuddy_debug_breakpoint which is triggered when DLL loads.
Automatically loads symbols when the breakpoint is hit.
Only works in debug builds (NDEBUG not defined).
Note: This breakpoint is now automatically set on startup, so this command is optional.
end

# Breakpoint helper that loads symbols when DllMain is hit
define break_dllmain
    break DllMain
    commands
        printf "DllMain hit! Loading symbols...\n"
        printf "Get base address from: info proc mappings\n"
        printf "Then: load_dll_at 0x<address>\n"
    end
end

document auto_load_dll_symbols
Attempt to automatically find and load sof_buddy.dll symbols.
end

document find_and_load_dll
Show filtered process mappings (only lines containing sof_buddy.dll).
Automatically extracts and displays the base address.
end

document bt
Enhanced backtrace that automatically identifies which module each address belongs to.
Shows module names (e.g., SoF.exe, sof_buddy.dll, Wine libraries) for each frame.
When stack corruption is detected, shows additional register information and helpful tips.
end

document bt_full
Enhanced backtrace with registers, disassembly, and module identification.
end

document load_dll_at
Load sof_buddy.dll symbols at the specified base address.
If called with no arguments, auto-detects address from process mappings.
Usage: load_dll_at [base_address] [dll_path]
Examples:
  load_dll_at                        # Auto-detect address and load
  load_dll_at 0x9720000              # Specify address, auto-detect path
  load_dll_at 0x9720000 sof_buddy.dll # Specify both address and path
end

document break_dllmain
Set breakpoint at DllMain to help load symbols early.
end

# Identify which module an address belongs to (filtered output)
define identify_address
    if $argc != 1
        printf "Usage: identify_address <address_in_hex>\n"
        printf "Example: identify_address 0x0bf8ecbf\n"
    else
        printf "Identifying module for address %s...\n", $arg0
        python
import gdb

try:
    addr_str = gdb.string_to_argv("$arg0")[0]
    # Convert to integer for comparison
    addr = int(addr_str, 16) if addr_str.startswith('0x') else int(addr_str, 16)
    
    # Get process mappings
    mappings_output = gdb.execute("info proc mappings", to_string=True)
    
    found = False
    for line in mappings_output.split('\n'):
        # Extract address range from line (format: start_addr end_addr ...)
        parts = line.split()
        if len(parts) >= 2:
            try:
                start_addr = int(parts[0], 16) if parts[0].startswith('0x') else int(parts[0], 16)
                end_addr = int(parts[1], 16) if parts[1].startswith('0x') else int(parts[1], 16)
                
                if start_addr <= addr < end_addr:
                    found = True
                    # Extract module name (usually at the end of the line)
                    module_name = parts[-1] if len(parts) > 0 else "unknown"
                    print(f"Address {addr_str} belongs to:")
                    print(f"  Module: {module_name}")
                    print(f"  Range: {parts[0]} - {parts[1]}")
                    print(f"  Full line: {line.strip()}")
                    break
            except (ValueError, IndexError):
                continue
    
    if not found:
        print(f"Address {addr_str} not found in any mapped region.")
        print("This might indicate:")
        print("  - Invalid/corrupted address")
        print("  - Address in unmapped memory")
        print("  - Stack corruption")
        print("\nFull mappings:")
        gdb.execute("info proc mappings")
except Exception as e:
    print(f"Error: {e}")
    print("Showing full mappings instead:")
    gdb.execute("info proc mappings")
end
    end
end

document identify_address
Identify which module (DLL/EXE) an address belongs to.
Automatically filters mappings to show only the relevant module.
Usage: identify_address 0x<address>
Example: identify_address 0x0042fc50
end

# Note: SIGTRAP catchpoint removed - we use breakpoints on sofbuddy_debug_breakpoint
# and sofbuddy_assert_failed instead, which work better with symbols loaded

# Catch SIGUSR1 (triggered by debug-gdb build)
catch signal SIGUSR1
commands
    printf "\n=== Caught SIGUSR1 ===\n"
    printf "Load symbols with: auto_load_dll_symbols\n"
    printf "\n"
end

# Print instructions on startup
printf "\n=== GDB Symbol Loading Helper for sof_buddy.dll ===\n"
printf "SIGUSR1 catchpoint set - when caught, use: auto_load_dll_symbols\n"
printf "\nAvailable commands:\n"
printf "  auto_load_dll_symbols - Auto-detect and load symbols\n"
printf "  load_dll_at [addr] [path] - Load symbols (auto-detects if no address)\n"
printf "  break_asserts - Set breakpoint on assert failures\n"
printf "  bt - Enhanced backtrace with module identification\n"
printf "  bt_full - Enhanced backtrace with registers and disassembly\n"
printf "  find_and_load_dll - Show filtered mappings\n"
printf "  identify_address <addr> - Identify which module an address belongs to\n"
printf "\n"

