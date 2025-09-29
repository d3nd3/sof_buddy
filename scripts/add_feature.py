#!/usr/bin/env python3
"""
Small idempotent generator to scaffold a new feature directory with the
minimal files required by this project: `hooks.h`, `hooks.cpp`, and
`detour_manifest.h`. It also prints the lines you may paste into feature
registration or can optionally attempt to insert them automatically.

Usage:
  ./scripts/add_feature.py <feature_name>

This script is intentionally conservative: it will not overwrite existing
files unless `--force` is passed. It aims to follow repository conventions
so generated code compiles with the pattern documented in `src/features/FEATURES.md`.
"""
from __future__ import annotations
import argparse
import os
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(__file__))

HOOKS_H_TMPL = '''#ifndef FEATURES_{UP}_HOOKS_H
#define FEATURES_{UP}_HOOKS_H

#ifdef __cplusplus
extern "C" {{
#endif

// Feature: {name}
// Exported callbacks implemented by the feature.
void refgl_on_{name}(void);

// Register this feature's callbacks with the central hook registry
void {name}_register_feature_hooks(void);

#ifdef __cplusplus
}}
#endif

#endif // FEATURES_{UP}_HOOKS_H
'''

HOOKS_CPP_TMPL = '''#include "hooks.h"
#include "../../hooks/hook_registry.h"
#include "../../feature_registration.h"

// Implement your feature callback(s) here.
void refgl_on_{name}(void) {{
    // TODO: implement feature behavior
}}

void {name}_register_feature_hooks(void) {{
    // Example registration - adjust to the hooks your feature needs:
    // register_hook_VID_CheckChanges(refgl_on_{name});
}}

// Minimal startup that registers this feature's hooks. This will be
// referenced by FEATURE_AUTO_REGISTER so the feature is queued during
// static initialization and picked up by the manifest subsystem.
void {name}_startup(void) {{
    {name}_register_feature_hooks();
}}

// Use new registration order: early_init, startup, late_init, detours
FEATURE_AUTO_REGISTER({name}, "{name}", NULL, &{name}_startup, NULL, &{name}_register_detours);
'''

DETOUR_MANIFEST_H_TMPL = '''#ifndef {UP}_DETOUR_MANIFEST_H
#define {UP}_DETOUR_MANIFEST_H

#include "../../../hdr/detour_manifest.h"

// Detour definitions for {name} feature
extern const DetourDefinition {name}_detour_manifest[];
extern const int {name}_detour_count;

// Registration function
void {name}_register_detours(void);

#endif // {UP}_DETOUR_MANIFEST_H
'''

def write_if_missing(path: str, content: str, force: bool=False) -> bool:
    if os.path.exists(path) and not force:
        return False
    with open(path, 'w', newline='\n') as f:
        f.write(content)
    return True


def generate_detour_entry(detour_name: str, address: str, signature: str, feature: str) -> str:
    """Generate a DetourDefinition initializer for a PTR detour.

    signature is expected in the form: "ret(arg1,arg2,...)" or "void(void)".
    For now we don't use the types from signature except to keep a comment;
    the generated detour_func symbol will be `exe_<detour_name>`.
    """
    desc = f"Test detour: {detour_name}" if feature == "test_it" else f"Detour: {detour_name}"
    # Use a wrapper_prefix (e.g. "exe") rather than embedding a raw pointer
    wrapper = "exe"
    entry = (
        "    {\n"
        f"        \"{detour_name}\",\n"
        "        DETOUR_TYPE_PTR,\n"
        "        NULL,  // Not used for PTR type\n"
        f"        \"{address}\",  // Address expression\n"
        f"        \"{wrapper}\",  // wrapper_prefix (exe/ref/game)\n"
        "        DETOUR_PATCH_JMP,\n"
        "        5,\n"
        f"        \"{desc}\",\n"
        f"        \"{feature}\"\n"
        "    },\n"
    )
    return entry


def append_detours_to_manifest(feature_dir: str, feature: str, detours: list[tuple[str,str,str]], force: bool=False) -> list[str]:
    """Append detour entries to src/features/<feature>/detour_manifest.cpp.

    detours: list of (name,address,signature)
    Returns list of files created/modified.
    """
    out_path = os.path.join(feature_dir, 'detour_manifest.cpp')
    created_or_modified = []

    if not os.path.exists(out_path):
        # create a new manifest file with header and array
        header = (
            '#include "../../../hdr/detour_manifest.h"\n\n'
            f'// Detour definitions for {feature} feature\n'
            f'const DetourDefinition {feature}_detour_manifest[] = {{\n'
        )
        body = ""
        for name, addr, sig in detours:
            body += generate_detour_entry(name, addr, sig, feature)
        footer = (
            '};\n\n'
            f'const int {feature}_detour_count = sizeof({feature}_detour_manifest) / sizeof(DetourDefinition);\n\n'
            '// Queue these definitions for import during detour_manifest_init().\n'
            'extern void detour_manifest_queue_definitions(const DetourDefinition *defs, int count);\n'
            f'static int {feature}_manifest_queued = (detour_manifest_queue_definitions({feature}_detour_manifest, {feature}_detour_count), 0);\n'
        )
        with open(out_path, 'w', newline='\n') as f:
            f.write(header)
            f.write(body)
            f.write(footer)
        created_or_modified.append(out_path)
        return created_or_modified

    # If file exists, try to find the array and insert before its closing brace
    with open(out_path, 'r', newline='\n') as f:
        content = f.read()

    marker = f'const DetourDefinition {feature}_detour_manifest[] = {{'
    idx = content.find(marker)
    if idx == -1:
        # Can't find canonical array; append a new array at end (safe fallback)
        with open(out_path, 'a', newline='\n') as f:
            f.write('\n')
            f.write(f'// Appended detours for {feature}\n')
            f.write(f'const DetourDefinition {feature}_detour_manifest[] = {{\n')
            for name, addr, sig in detours:
                f.write(generate_detour_entry(name, addr, sig, feature))
            f.write('};\n')
            f.write(f'const int {feature}_detour_count = sizeof({feature}_detour_manifest) / sizeof(DetourDefinition);\n')
            f.write('\n')
            f.write('// Queue these definitions for import during detour_manifest_init().\n')
            f.write('extern void detour_manifest_queue_definitions(const DetourDefinition *defs, int count);\n')
            f.write(f'static int {feature}_manifest_queued = (detour_manifest_queue_definitions({feature}_detour_manifest, {feature}_detour_count), 0);\n')
        created_or_modified.append(out_path)
        return created_or_modified

    # Find end of array via brace matching starting at '{' after marker
    start_brace = content.find('{', idx)
    pos = start_brace + 1
    depth = 1
    while pos < len(content) and depth > 0:
        if content[pos] == '{':
            depth += 1
        elif content[pos] == '}':
            depth -= 1
        pos += 1

    # pos now points just after the matching '}'
    insert_pos = pos - 1  # before the closing '}'
    new_entries = ''
    for name, addr, sig in detours:
        new_entries += generate_detour_entry(name, addr, sig, feature)

    new_content = content[:insert_pos] + new_entries + content[insert_pos:]
    with open(out_path, 'w', newline='\n') as f:
        f.write(new_content)
    created_or_modified.append(out_path)

    # Ensure the file queues the manifest array for import. If the canonical
    # queue call isn't present, append it so detour_manifest_init() will pick
    # up the new entries automatically.
    if 'detour_manifest_queue_definitions' not in new_content:
        with open(out_path, 'a', newline='\n') as f:
            f.write('\n')
            f.write('// Queue these definitions for import during detour_manifest_init().\n')
            f.write('extern void detour_manifest_queue_definitions(const DetourDefinition *defs, int count);\n')
            f.write(f'static int {feature}_manifest_queued = (detour_manifest_queue_definitions({feature}_detour_manifest, {feature}_detour_count), 0);\n')

    # For each generated detour, also register the implementation pointer with
    # the manifest (useful for runtime lookup/validation). We append static
    # initializers that call the registration helper so no manual edits are
    # required by the user when --apply is used.
    for name, addr, sig in detours:
        # Register via wrapper_prefix convention (generated wrapper symbol: <prefix>_<Name>)
        reg_line = (
            f'\nextern void detour_manifest_register_detour_function(const char *name, void *func_ptr);\n'
            f'static void *_gen_impl_{feature}_{name} = NULL; /* placeholder for runtime resolution */\n'
            f'static int _reg_{feature}_{name} = (detour_manifest_register_detour_function("{name}", (void*)0), 0);\n'
        )
        if reg_line not in new_content:
            with open(out_path, 'a', newline='\n') as f:
                f.write(reg_line)

    return created_or_modified

def main(argv: list[str]) -> int:
    prog = os.path.basename(__file__)
    description = (
        "Small idempotent generator to scaffold a new feature directory with the "
        "minimal files required by this project: hooks.h, hooks.cpp and "
        "detour_manifest.h. It will not overwrite existing files unless "
        "--force is specified."
    )
    epilog = (
        "Examples:\n"
        f"  {prog} my_feature\n"
        f"  {prog} my_feature --apply\n"
        "See src/features/FEATURES.md for repository conventions."
    )

    parser = argparse.ArgumentParser(prog=prog, description=description, epilog=epilog,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('feature', help='feature name (snake_case)')
    parser.add_argument('--force', action='store_true', help='overwrite existing files')
    parser.add_argument('--apply', action='store_true', help='Auto-apply generated changes to shared files (hook_registry/src/funcs)')
    parser.add_argument('--version', action='version', version='add_feature.py 1.0')
    args = parser.parse_args(argv)

    name = args.feature
    up = name.upper()
    feature_dir = os.path.join(REPO_ROOT, 'src', 'features', name)
    os.makedirs(feature_dir, exist_ok=True)

    hooks_h = os.path.join(feature_dir, 'hooks.h')
    hooks_cpp = os.path.join(feature_dir, 'hooks.cpp')
    detour_h = os.path.join(feature_dir, 'detour_manifest.h')
    detour_cpp = os.path.join(feature_dir, 'detour_manifest.cpp')

    changed = []
    if write_if_missing(hooks_h, HOOKS_H_TMPL.format(UP=up, name=name), force=args.force):
        changed.append(hooks_h)
    if write_if_missing(hooks_cpp, HOOKS_CPP_TMPL.format(name=name), force=args.force):
        changed.append(hooks_cpp)
    if write_if_missing(detour_h, DETOUR_MANIFEST_H_TMPL.format(UP=up, name=name), force=args.force):
        changed.append(detour_h)
    # Ensure a minimal detour_manifest.cpp scaffold exists so users have a
    # place to define DetourDefinition arrays even if they didn't pass
    # --detour. This file is intentionally minimal and idempotent. The
    # DetourDefinition no longer encodes a raw function pointer; instead
    # features should register their implementation pointers at static init
    # using `detour_manifest_register_detour_function(name, func_ptr)` or
    # rely on the wrapper_prefix convention (e.g. "exe").
    detour_cpp_content = (
        '#include "../../../hdr/detour_manifest.h"\n\n'
        f'// Detour definitions for {name} feature (empty scaffold)\n'
        '\n'
        '/* Example detour (commented out - does not apply)\n'
        'const DetourDefinition example_detour_manifest[] = {\n'
        '    {\n'
        '        "V_DrawScreen",               // name\n'
        '        DETOUR_TYPE_PTR,               // type\n'
        '        NULL,                          // target_module (NULL for PTR)\n'
        '        "0x13371337",                 // target_proc / address expression\n'
        '        "exe",                         // wrapper_prefix (exe/ref/game)\n'
        '        DETOUR_PATCH_JMP,              // patch_type\n'
        '        5,                             // detour_len (number of patched bytes)\n'
        '        "void(void)",                 // signature (compact prototype)\n'
        '        "' + name + '"                  // feature_name\n'
        '    }\n'
        '};\n'
        '*/\n\n'
        f'const DetourDefinition {name}_detour_manifest[] = {{}};\n\n'
        f'const int {name}_detour_count = sizeof({name}_detour_manifest) / sizeof(DetourDefinition);\n\n'
        '// Queue these definitions for import during detour_manifest_init().\n'
        'extern void detour_manifest_queue_definitions(const DetourDefinition *defs, int count);\n'
        f'static int {name}_manifest_queued = (detour_manifest_queue_definitions({name}_detour_manifest, {name}_detour_count), 0);\n'
    )
    if write_if_missing(detour_cpp, detour_cpp_content, force=args.force):
        changed.append(detour_cpp)

    print('Feature scaffold:', name)
    if changed:
        print('\nCreated/updated files:')
        for p in changed:
            print(' -', os.path.relpath(p, REPO_ROOT))
    else:
        print('\nNo files changed (use --force to overwrite).')

    # Note: detour entries are authored directly in src/features/<feature>/detour_manifest.cpp.
    # The two-step workflow expects you to edit that file and then re-run this script with
    # `--apply` to generate wrappers and registry entries. The --detour CLI option has been
    # removed to avoid duplicating the authoritative source of detours.

    # If --apply requested, try to parse existing detour_manifest.cpp for entries
    if args.apply:
        detours_parsed = []
        try:
            if os.path.exists(detour_cpp):
                with open(detour_cpp, 'r', newline='\n') as f:
                    content = f.read()

                # Find the array block
                marker = f'const DetourDefinition {name}_detour_manifest[] = '
                idx = content.find(marker)
                if idx != -1:
                    start = content.find('{', idx)
                    end = content.find('};', start)
                    if start != -1 and end != -1 and end > start:
                        array_block = content[start:end]
                        # Extract individual initializer blocks { ... }
                        entries = []
                        depth = 0
                        cur = ''
                        for ch in array_block:
                            cur += ch
                            if ch == '{':
                                depth += 1
                            elif ch == '}':
                                depth -= 1
                                if depth == 0:
                                    entries.append(cur)
                                    cur = ''

                        for e in entries:
                            # Normalize lines and strip outer braces
                            lines = [ln.strip().rstrip(',') for ln in e.splitlines()]
                            # Remove the first line if it's just '{'
                            if lines and lines[0].startswith('{'):
                                lines = lines[1:]
                            # Collect field values in order
                            fields = []
                            for ln in lines:
                                # skip empty or comment-only lines
                                if not ln or ln.startswith('//'):
                                    continue
                                # take the token before any // comment
                                if '//' in ln:
                                    ln = ln.split('//', 1)[0].strip()
                                # strip trailing commas/whitespace
                                token = ln.strip().rstrip(',').strip()
                                if token:
                                    fields.append(token)
                            # Expect fields: name, type, target_module, target_proc, wrapper_prefix, patch_type, detour_len, signature, feature_name
                            if len(fields) >= 9:
                                det_name = fields[0].strip().strip('"')
                                addr = fields[3].strip().strip('"')
                                wrapper_pref = fields[4].strip().strip('"')
                                sig = fields[7].strip().strip('"')
                                detours_parsed.append((det_name, addr, sig, wrapper_pref))
        except Exception as e:
            print('\nFailed to parse detour_manifest.cpp:', e)

        if detours_parsed:
            # Generate hook registry declarations and implementations based on signatures,
            # and then generate wrapper stubs using the parsed wrapper_prefix.
            hook_h_path = os.path.join(REPO_ROOT, 'src', 'hooks', 'hook_registry.h')
            hook_cpp_path = os.path.join(REPO_ROOT, 'src', 'hooks', 'hook_registry.cpp')

            def parse_signature(sig: str):
                sig = sig.strip()
                if '(' not in sig or not sig.endswith(')'):
                    return ('void', 'void')
                ret, args = sig.split('(', 1)
                args = args[:-1]
                ret = ret.strip()
                args = args.strip()
                if args == '':
                    args = 'void'
                return (ret, args)

            def make_header_decl(name: str, sig: str) -> str:
                ret, args = parse_signature(sig)
                return (
                    f'typedef {ret} (*hook_{name}_t)({args});\n'
                    f'void register_hook_{name}(hook_{name}_t cb);\n'
                    f'{ret} call_hook_{name}({args});\n'
                )

            def make_cpp_impl(name: str, sig: str) -> str:
                ret, args = parse_signature(sig)
                size = 8
                lines = []
                lines.append(f'static void *g_hooks_{name}[{size}];')
                lines.append(f'static int g_hooks_{name}_count = 0;')
                lines.append(f'void register_hook_{name}(hook_{name}_t cb) {{ if (g_hooks_{name}_count < {size}) g_hooks_{name}[g_hooks_{name}_count++] = (void*)cb; }}')
                if ret == 'void':
                    if args == 'void':
                        lines.append(f'void call_hook_{name}(void) {{ for (int i = 0; i < g_hooks_{name}_count; ++i) ((hook_{name}_t)g_hooks_{name}[i])(); }}')
                    else:
                        lines.append(f'void call_hook_{name}({args}) {{ for (int i = 0; i < g_hooks_{name}_count; ++i) ((hook_{name}_t)g_hooks_{name}[i])({args}); }}')
                else:
                    # return first non-zero/null result
                    if args == 'void':
                        lines.append(f'{ret} call_hook_{name}(void) {{ for (int i = 0; i < g_hooks_{name}_count; ++i) {{ {ret} res = ((hook_{name}_t)g_hooks_{name}[i])(); if (res) return res; }} return ({ret})0; }}')
                    else:
                        lines.append(f'{ret} call_hook_{name}({args}) {{ for (int i = 0; i < g_hooks_{name}_count; ++i) {{ {ret} res = ((hook_{name}_t)g_hooks_{name}[i])({args}); if (res) return res; }} return ({ret})0; }}')
                return '\n'.join(lines) + '\n'

            # read existing hook header/cpp if present
            hook_h_content = ''
            if os.path.exists(hook_h_path):
                with open(hook_h_path, 'r', newline='\n') as f:
                    hook_h_content = f.read()
            hook_cpp_content = ''
            if os.path.exists(hook_cpp_path):
                with open(hook_cpp_path, 'r', newline='\n') as f:
                    hook_cpp_content = f.read()

            # add missing hook typedefs/declarations and implementations
            for det_name, det_addr, det_sig, wrapper_pref in detours_parsed:
                if f'hook_{det_name}_t' not in hook_h_content:
                    decl = make_header_decl(det_name, det_sig)
                    # insert before closing extern "C" block if present
                    closing = '\n#ifdef __cplusplus\n}\n#endif\n'
                    if closing in hook_h_content:
                        hook_h_content = hook_h_content.replace(closing, decl + '\n' + closing)
                    else:
                        hook_h_content += '\n' + decl
                    with open(hook_h_path, 'w', newline='\n') as fh:
                        fh.write(hook_h_content)
                if f'g_hooks_{det_name}_' not in hook_cpp_content:
                    impl = make_cpp_impl(det_name, det_sig)
                    hook_cpp_content += '\n' + impl
                    with open(hook_cpp_path, 'w', newline='\n') as fc:
                        fc.write(hook_cpp_content)

            # Ensure detour_manifest registers an implementation pointer for each parsed detour.
            # This writes small static initializers into the feature's detour_manifest.cpp so
            # the manifest's g_detour_functions map is populated during static init.
            try:
                if os.path.exists(detour_cpp):
                    with open(detour_cpp, 'r', newline='\n') as df:
                        dcontent = df.read()
                    reg_lines = ''
                    for det_name, det_addr, det_sig, wrapper_pref in detours_parsed:
                        reg_line = (
                            f'\nextern void detour_manifest_register_detour_function(const char *name, void *func_ptr);\n'
                            f'static void *_gen_impl_{name}_{det_name} = NULL; /* placeholder for runtime resolution */\n'
                            f'static int _reg_{name}_{det_name} = (detour_manifest_register_detour_function("{det_name}", (void*)0), 0);\n'
                        )
                        if reg_line.strip() not in dcontent:
                            reg_lines += reg_line
                    if reg_lines:
                        with open(detour_cpp, 'a', newline='\n') as df:
                            df.write(reg_lines)
                        print('\nAppended detour registration lines to:', os.path.relpath(detour_cpp, REPO_ROOT))
            except Exception as e:
                print('\nFailed to append detour registration lines:', e)

            # now generate wrappers into funcs file
            exe_funcs_path = os.path.join(REPO_ROOT, 'src', 'funcs', 'exe_funcs.cpp')
            try:
                if os.path.exists(exe_funcs_path):
                    with open(exe_funcs_path, 'r', newline='\n') as f:
                        exe_content = f.read()
                    stubs_to_add = ''
                    for det_name, det_addr, det_sig, wrapper_pref in detours_parsed:
                        stub_name = f"{wrapper_pref}_{det_name}"
                        if stub_name + '(' in exe_content:
                            continue

                        # Detect whether a call_hook_<Name>() exists anywhere in the repo
                        call_hook_exists = False
                        for root, dirs, files in os.walk(REPO_ROOT):
                            for fn in files:
                                if not fn.endswith(('.cpp', '.h', '.c', '.hpp')):
                                    continue
                                p = os.path.join(root, fn)
                                try:
                                    with open(p, 'r', newline='\n') as fh:
                                        if f"call_hook_{det_name}(" in fh.read():
                                            call_hook_exists = True
                                            raise StopIteration
                                except StopIteration:
                                    break
                                except Exception:
                                    pass
                            if call_hook_exists:
                                break

                        stub = []
                        stub.append('\n// Generated wrapper for detour: %s' % det_name)
                        stub.append('void %s(void) {' % stub_name)
                        if call_hook_exists:
                            stub.append('    // Invoke registered feature callbacks (if any)')
                            stub.append('    call_hook_%s();' % det_name)
                        stub.append('    // Optionally call the original implementation via the manifest trampoline')
                        stub.append('    void *orig_tr = detour_manifest_get_internal_trampoline("%s");' % det_name)
                        stub.append('    if (orig_tr) { /* cast and call if signature known */ }')
                        stub.append('}')
                        stubs_to_add += '\n'.join(stub) + '\n'

                    if stubs_to_add:
                        with open(exe_funcs_path, 'a', newline='\n') as f:
                            f.write('\n')
                            f.write('// --- auto-generated detour wrappers ---\n')
                            f.write(stubs_to_add)
                        print('\nApplied wrappers to:', os.path.relpath(exe_funcs_path, REPO_ROOT))
                else:
                    print('\n--apply requested but %s not found' % exe_funcs_path)
            except Exception as e:
                print('\nFailed to apply wrappers:', e)

    print('\nNext steps:')
    print(' - Implement callbacks in', os.path.relpath(hooks_cpp, REPO_ROOT))
    print(' - Define DetourDefinition array and', name + '_detour_count in a new .cpp in the feature dir')
    print(' - Call', name + '_register_feature_hooks() from your feature startup (or add to existing startup)')
    print(' - Add FEATURE_AUTO_REGISTER(...) if not present')

    return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))


