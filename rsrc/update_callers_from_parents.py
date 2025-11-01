#!/usr/bin/env python3
import json
import os
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PARENTS_DIR = ROOT / 'rsrc' / 'func_parents'
SCALED_UI_H = ROOT / 'src' / 'features' / 'scaled_ui' / 'scaled_ui.h'
TEXT_CPP = ROOT / 'src' / 'features' / 'scaled_ui' / 'text.cpp'
CALLER_FROM_CPP = ROOT / 'src' / 'features' / 'scaled_ui' / 'caller_from.cpp'

# Map child name -> (file, function name, keyed by module?)
TARGETS = {
    # These are now module-aware; addresses live under Module::SofExe
    'Draw_StretchPic': (CALLER_FROM_CPP, 'getStretchPicCallerFrom', True),
    'Draw_Pic': (CALLER_FROM_CPP, 'getPicCallerFrom', True),
    'Draw_PicOptions': (CALLER_FROM_CPP, 'getPicOptionsCallerFrom', True),
    'Draw_CroppedPicOptions': (CALLER_FROM_CPP, 'getCroppedPicCallerFrom', True),
    # Already module-aware
    'GL_FindImage': (CALLER_FROM_CPP, 'getFindImageCallerFrom', True),
    'glVertex2f': (CALLER_FROM_CPP, 'getVertexCallerFromRva', True),
    'R_DrawFont': (CALLER_FROM_CPP, 'getFontCallerFrom', True),
}

MODULE_ORDER = ['SoF.exe', 'ref_gl.dll', 'player.dll', 'gamex86.dll', 'spcl.dll']
MODULE_ENUM = {
    'SoF.exe': 'Module::SofExe',
    'ref_gl.dll': 'Module::RefDll',
    'player.dll': 'Module::PlayerDll',
    'gamex86.dll': 'Module::GameDll',
    'spcl.dll': 'Module::SpclDll',
}

CASE_HEX_RE = re.compile(r"case\s+0x([0-9A-Fa-f]{1,8})\s*:")
MODULE_CASE_RE = re.compile(r"case\s+Module::(SofExe|RefDll|PlayerDll|GameDll|SpclDll)\s*:")
FUNC_DEF_RE = re.compile(r"(inline\s+)?([\w:<>]+)\s+(get\w+CallerFromRva|getFindImageCallerFrom|getFontCallerFrom|get\w+CallerFrom)\s*\(([^)]*)\)\s*\{", re.M)


def read_json_rvas(path):
    with open(path, 'r', encoding='utf-8') as f:
        data = json.load(f)
    entries = []
    for p in data.get('parents', []):
        mod = p.get('module')
        rva = int(p.get('fnStart', 0))
        entries.append((mod, rva))
    return entries


def hexlit(rva):
    return f"0x{rva:08X}"


def build_switch_non_module(rvas):
    uniq = sorted(set(r for _, r in rvas))
    lines = ["\tswitch (fnStartRva) {\n"]
    for r in uniq:
        lines.append(f"\t\tcase {hexlit(r)}: return Unknown; // new\n")
    lines.append("\t\tdefault: return Unknown;\n\t}\n")
    return ''.join(lines)


def build_switch_module(rvas):
    mod_to = {}
    for m, r in rvas:
        mod_to.setdefault(m, set()).add(r)
    lines = ["\tswitch (m) {\n"]
    for mod in MODULE_ORDER:
        if mod not in mod_to:
            continue
        lines.append(f"\t\tcase {MODULE_ENUM.get(mod, 'Module::Unknown')}:\n")
        lines.append("\t\t\tswitch (fnStartRva) {\n")
        for r in sorted(mod_to[mod]):
            lines.append(f"\t\t\t\tcase {hexlit(r)}: return Unknown; // new\n")
        lines.append("\t\t\t\tdefault: return Unknown;\n\t\t\t}\n")
    lines.append("\t\tdefault: return Unknown;\n\t}\n")
    return ''.join(lines)


def find_function_body(src: str, func_name: str):
    m = re.search(rf"(inline\s+)?[\w:<>]+\s+{re.escape(func_name)}\s*\([^)]*\)\s*\{{", src, re.M)
    if not m:
        return None, None
    start = m.end()
    depth = 1
    i = start
    while i < len(src) and depth > 0:
        if src[i] == '{':
            depth += 1
        elif src[i] == '}':
            depth -= 1
        i += 1
    return start, i - 1
def find_matching_brace(src: str, open_brace_index: int):
    depth = 1
    i = open_brace_index + 1
    while i < len(src) and depth > 0:
        c = src[i]
        if c == '{':
            depth += 1
        elif c == '}':
            depth -= 1
        i += 1
    return i - 1 if depth == 0 else None



def find_function_return_type(src: str, func_name: str):
    m = re.search(rf"(inline\s+)?([\w:<>]+)\s+{re.escape(func_name)}\s*\(", src, re.M)
    if not m:
        return None
    return m.group(2)


def extract_cases_non_module(body: str):
    return set(int(h, 16) for h in CASE_HEX_RE.findall(body))


def extract_cases_module(body: str):
    # Return dict of enum module -> set of RVAs
    out = {k: set() for k in MODULE_ENUM.values()}
    # Split by module case blocks
    for mm in MODULE_CASE_RE.finditer(body):
        enum_name = f"Module::{mm.group(1)}"
        block_start = mm.end()
        # find next module case or end of function
        next_m = MODULE_CASE_RE.search(body, block_start)
        block_end = next_m.start() if next_m else len(body)
        sub = body[block_start:block_end]
        addrs = extract_cases_non_module(sub)
        out[enum_name].update(addrs)
    return out


def compare_sets(existing, expected):
    missing = sorted(expected - existing)
    extra = sorted(existing - expected)
    return missing, extra


def validate_against_json(src: str, child: str, func: str, module_aware: bool, rvas):
    start, end = find_function_body(src, func)
    if start is None:
        return [(child, 'function_not_found')]
    body = src[start:end]
    issues = []
    if module_aware:
        existing = extract_cases_module(body)
        # build expected by module enum
        exp_by_enum = {MODULE_ENUM.get(m, 'Module::Unknown'): set() for m, _ in rvas}
        for m, r in rvas:
            exp_by_enum.setdefault(MODULE_ENUM.get(m, 'Module::Unknown'), set()).add(r)
        for enum_name, exp_set in exp_by_enum.items():
            miss, ext = compare_sets(existing.get(enum_name, set()), exp_set)
            if miss or ext:
                issues.append((child, enum_name, miss, ext))
    else:
        existing = extract_cases_non_module(body)
        exp = set(r for _, r in rvas)
        miss, ext = compare_sets(existing, exp)
        if miss or ext:
            issues.append((child, 'non_module', miss, ext))
    return issues


def replace_function_switch(src, func_name, module_aware, new_switch_body):
    m = re.search(rf"(inline\s+)?[\w:<>]+\s+{re.escape(func_name)}\s*\([^)]*\)\s*\{{", src, re.M)
    if not m:
        return src, False
    start = m.end()
    depth = 1
    i = start
    while i < len(src) and depth > 0:
        if src[i] == '{':
            depth += 1
        elif src[i] == '}':
            depth -= 1
        i += 1
    body = src[start:i-1]
    # Incremental insert: find default case(s) and insert missing 'case 0x...:' lines before default
    def insert_cases(block: str, missing_hex: list):
        if not missing_hex:
            return block, False
        # Find position before the first 'default:' inside this block
        mdef = re.search(r"(^\s*default\s*:)", block, re.M)
        insert_pos = mdef.start() if mdef else len(block)
        lines = ''.join([f"\t\t\t\tcase {h}: return Unknown; // new\n" for h in missing_hex])
        new_block = block[:insert_pos] + lines + block[insert_pos:]
        return new_block, True

    changed = False
    if module_aware:
        # For each module sub-switch, compute missing and insert
        for enum_key, enum_name in MODULE_ENUM.items():
            # locate 'case Module::X:' region
            mcase = re.search(rf"(\t\tcase {re.escape(enum_name)}:\s*)([\s\S]*?)(?=\n\t\tcase Module::|\n\t\tdefault:|\Z)", body)
            if not mcase:
                continue
            head, sub, tail_start = mcase.group(1), mcase.group(2), mcase.end()
            # find inner switch block
            msw = re.search(r"(\t\t\tswitch \(fnStartRva\) \{)([\s\S]*?)(\t+\})", sub)
            if not msw:
                continue
            inner_head, inner_body, inner_tail = msw.group(1), msw.group(2), msw.group(3)
            existing = set(int(h,16) for h in CASE_HEX_RE.findall(inner_body))
            # expected set for this module
            # compute expected outside: caller builds full body; here we rely on earlier validation to supply missing set
            # We'll re-derive expected from JSON again for simplicity
            # Note: this function is used only by writer path and called per target; caller passes new_switch_body; we ignore it
            # So return without change; caller uses alternative writer below
            pass
    else:
        # Non-module-aware: find top-level switch and insert missing
        msw = re.search(r"(\tswitch \(fnStartRva\) \{)([\s\S]*?)(\t\})", body)
        if not msw:
            return src, False
        inner_head, inner_body, inner_tail = msw.group(1), msw.group(2), msw.group(3)
        existing = set(int(h,16) for h in CASE_HEX_RE.findall(inner_body))
        # Build expected from new_switch_body
        expected = set(int(h,16) for h in CASE_HEX_RE.findall(new_switch_body))
        missing = sorted(expected - existing)
        if not missing:
            return src, False
        missing_hex = [hexlit(r) for r in missing]
        new_inner, did = insert_cases(inner_body, missing_hex)
        if not did:
            return src, False
        new_body = body[:msw.start(2)] + new_inner + body[msw.end(2):]
        new_src = src[:start] + new_body + src[i-1:]
        return new_src, True

    # If module-aware, we don't use this path (handled separately by writer)
    return src, False


def main(write: bool):
    # Collect RVAs per child
    child_to_rvas = {}
    for p in PARENTS_DIR.glob('*.json'):
        child = p.stem
        rvas = read_json_rvas(p)
        child_to_rvas[child] = rvas

    # Validate
    src = CALLER_FROM_CPP.read_text(encoding='utf-8')
    problems = []
    for child, (file, func, module_aware) in TARGETS.items():
        rvas = child_to_rvas.get(child)
        if not rvas:
            continue
        issues = validate_against_json(src, child, func, module_aware, rvas)
        problems.extend(issues)

    if not write:
        if not problems:
            print("Up-to-date: caller_from.h matches rsrc/func_parents JSONs (addresses)")
            return 0
        print("Differences detected (by addresses only):")
        for item in problems:
            if len(item) == 2:
                child, scope = item
                print(f"  {child} [{scope}]")
            else:
                child, scope, missing, extra = item
                miss_s = ", ".join(hex(r) for r in missing) if missing else "<none>"
                extra_s = ", ".join(hex(r) for r in extra) if extra else "<none>"
                print(f"  {child} [{scope}] missing: {miss_s} extra: {extra_s}")
        return 0

    # If write, insert only missing addresses
    if not problems:
        print("No changes needed: caller_from.h already up-to-date")
        return 0

    su_src = src
    updates = []
    # Non-module-aware insertions
    for child, (file, func, module_aware) in TARGETS.items():
        if module_aware:
            continue
        rvas = child_to_rvas.get(child)
        if not rvas:
            continue
        expected = set(r for _, r in rvas)
        new_switch = build_switch_non_module(rvas)
        su_src_next, ok = replace_function_switch(su_src, func, False, new_switch)
        if ok and su_src_next != su_src:
            updates.append(f"inserted {func} cases: +{len(expected)} (existing preserved)")
            su_src = su_src_next
    # Module-aware insertions: handle each module block independently
    # We'll manually edit the text to add missing cases into each module's inner switch
    for child, (file, func, module_aware) in TARGETS.items():
        if not module_aware:
            continue
        rvas = child_to_rvas.get(child)
        if not rvas:
            continue
        start, end = find_function_body(su_src, func)
        if start is None:
            continue
        body = su_src[start:end]
        changed_any = False
        # Work strictly inside the outer 'switch (m)' block
        msw_outer_open = re.search(r"\tswitch \(m\) \{", body)
        if not msw_outer_open:
            continue
        outer_open_start = msw_outer_open.start()
        outer_open_brace = msw_outer_open.end() - 1
        outer_close_brace = find_matching_brace(body, outer_open_brace)
        if outer_close_brace is None:
            continue
        outer_head = body[outer_open_start:outer_open_brace+1]
        outer_content = body[outer_open_brace+1:outer_close_brace]
        outer_tail = body[outer_close_brace:outer_close_brace+1]
        # Find the OUTER default at exactly two tabs indent
        outer_default = re.search(r"^\t\tdefault\s*:\s*return\s+[\w:]+\s*;", outer_content, re.M)
        outer_insert_pos_rel = outer_default.start() if outer_default else len(outer_content)
        for mod in MODULE_ORDER:
            enum_name = MODULE_ENUM.get(mod, 'Module::Unknown')
            # expected RVAs for this module
            expected = set(r for m,r in rvas if m == mod)
            if not expected:
                continue
            mcase = re.search(rf"(\t\tcase {re.escape(enum_name)}:\s*)([\s\S]*?)(?=\n\t\tcase Module::|\n\t\tdefault:|\Z)", outer_content)
            if not mcase:
                # Skip creating new module blocks to avoid accidental misplacement
                continue
            # existing block: insert only missing cases using index search to avoid fragile regex
            sub = mcase.group(2)
            block_start_rel = mcase.start(2)
            msw_hdr = re.search(r"switch\s*\(fnStartRva\)\s*\{", sub)
            if not msw_hdr:
                continue
            # opening '{' is at end of header token; include it for brace match
            inner_open_brace_idx = msw_hdr.end() - 1
            inner_body_start = inner_open_brace_idx + 1
            # find matching closing brace for inner switch
            inner_close_rel = find_matching_brace(sub, inner_open_brace_idx)
            if inner_close_rel is None:
                continue
            inner_body = sub[inner_body_start:inner_close_rel]
            existing = set(int(h,16) for h in CASE_HEX_RE.findall(inner_body))
            missing = sorted(expected - existing)
            if not missing:
                continue
            # detect indentation
            m_case_indent = re.search(r"^(\s*)case\s", inner_body, re.M)
            case_indent = m_case_indent.group(1) if m_case_indent else "\t\t\t\t"
            m_def = re.search(r"^(\s*)default\s*:\s*", inner_body, re.M)
            def_idx = m_def.start() if m_def else -1
            insert_pos_rel = def_idx if def_idx >= 0 else len(inner_body)
            ret_type = find_function_return_type(su_src, func)
            unknown_tok = f"{ret_type}::Unknown" if ret_type else "Unknown"
            lines = ''.join([f"{case_indent}case {hexlit(r)}: return {unknown_tok}; // new\n" for r in missing])
            new_inner = inner_body[:insert_pos_rel] + lines + inner_body[insert_pos_rel:]
            new_sub = sub[:inner_body_start] + new_inner + sub[inner_close_rel:]
            outer_content = outer_content[:block_start_rel] + new_sub + outer_content[mcase.end(2):]
            changed_any = True
        if changed_any:
            new_body = body[:outer_open_brace+1] + outer_content + body[outer_close_brace:]
            su_src = su_src[:start] + new_body + su_src[end:]
            updates.append(f"inserted {func} module cases (existing preserved)")

    if updates:
        CALLER_FROM_CPP.write_text(su_src, encoding='utf-8')
        print("Applied updates:\n" + "\n".join(updates))
    else:
        print("No changes needed after reconciliation")
    return 0

if __name__ == '__main__':
    write = '--write' in sys.argv
    sys.exit(main(write))
