#!/usr/bin/env python3
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / 'src'
OUT_TXT = ROOT / 'rsrc' / 'detours_prebuild.txt'
OUT_MD = ROOT / 'rsrc' / 'detours_prebuild.md'
OUT_HDR = ROOT / 'hdr' / 'detours_prebuild.h'
FEAT_HDR = ROOT / 'hdr' / 'features.h'

def read_defined_features():
    feats = set()
    data = FEAT_HDR.read_text(encoding='utf-8', errors='ignore').splitlines()
    in_block = False
    for line in data:
        # strip // and /* */
        if '//' in line:
            line = line.split('//', 1)[0]
        while '/*' in line:
            pre, _, rest = line.partition('/*')
            if '*/' in rest:
                _, _, after = rest.partition('*/')
                line = pre + after
            else:
                line = pre
                in_block = True
                break
        if in_block:
            if '*/' in line:
                _, _, after = line.partition('*/')
                line = after
                in_block = False
            else:
                continue
        m = re.match(r"\s*#\s*define\s+(FEATURE_[A-Za-z0-9_]+)\b", line)
        if m:
            feats.add(m.group(1))
    return feats

tok_re = re.compile(r"\s*(\|\||&&|!|\(|\)|defined|FEATURE_[A-Za-z0-9_]+|[01]|\d+)\s*")

def tokenize(expr:str):
    pos = 0
    out = []
    while pos < len(expr):
        m = tok_re.match(expr, pos)
        if not m:
            # skip unknown tokens conservatively
            pos += 1
            continue
        tok = m.group(1)
        pos = m.end()
        out.append(tok)
    return out

def parse_expr(tokens, defined):
    # recursive descent with precedence: ! > && > ||
    pos = 0

    def parse_primary():
        nonlocal pos
        if pos >= len(tokens):
            return True
        tok = tokens[pos]
        if tok == '(':
            pos += 1
            val = parse_or()
            if pos < len(tokens) and tokens[pos] == ')':
                pos += 1
            return val
        if tok == 'defined':
            pos += 1
            if pos < len(tokens) and tokens[pos] == '(':
                pos += 1
                name = tokens[pos] if pos < len(tokens) else ''
                pos += 1
                if pos < len(tokens) and tokens[pos] == ')':
                    pos += 1
                return name in defined
            else:
                name = tokens[pos] if pos < len(tokens) else ''
                pos += 1
                return name in defined
        if tok.startswith('FEATURE_'):
            pos += 1
            return tok in defined
        if tok.isdigit():
            pos += 1
            return int(tok) != 0
        pos += 1
        return True

    def parse_not():
        nonlocal pos
        neg = False
        while pos < len(tokens) and tokens[pos] == '!':
            neg = not neg
            pos += 1
        val = parse_primary()
        return (not val) if neg else val

    def parse_and():
        val = parse_not()
        nonlocal pos
        while pos < len(tokens) and tokens[pos] == '&&':
            pos += 1
            val = val and parse_not()
        return val

    def parse_or():
        val = parse_and()
        nonlocal pos
        while pos < len(tokens) and tokens[pos] == '||':
            pos += 1
            val = val or parse_and()
        return val

    return parse_or()

def extract_feats(expr:str):
    return set(re.findall(r"FEATURE_[A-Za-z0-9_]+", expr))

detour_line_re = re.compile(r"^\s*([^=]+?)\s*=\s*(.*)$")

def split_args(arg_str: str):
    args = []
    buf = []
    depth = 0
    in_s = False
    in_d = False
    esc = False
    for ch in arg_str:
        if esc:
            buf.append(ch)
            esc = False
            continue
        if ch == '\\':
            buf.append(ch)
            esc = True
            continue
        if in_s:
            buf.append(ch)
            if ch == "'":
                in_s = False
            continue
        if in_d:
            buf.append(ch)
            if ch == '"':
                in_d = False
            continue
        if ch == '"':
            buf.append(ch)
            in_d = True
            continue
        if ch == "'":
            buf.append(ch)
            in_s = True
            continue
        if ch == '(':
            depth += 1
            buf.append(ch)
            continue
        if ch == ')':
            depth = max(depth - 1, 0)
            buf.append(ch)
            continue
        if ch == ',' and depth == 0:
            args.append(''.join(buf).strip())
            buf = []
            continue
        buf.append(ch)
    if buf:
        args.append(''.join(buf).strip())
    return args

def strip_string_quotes(s: str) -> str:
    s = s.strip()
    if (len(s) >= 2 and ((s[0] == '"' and s[-1] == '"') or (s[0] == "'" and s[-1] == "'"))):
        return s[1:-1]
    return s

def scan_file(p:Path, defined:set):
    # Track conditional stack
    stack = []  # each: {active:bool, matched:bool, feats:set}
    results = []
    in_block = False
    lines = p.read_text(encoding='utf-8', errors='ignore').splitlines()
    for i, raw in enumerate(lines, 1):
        line = raw
        # strip //
        if '//' in line:
            line = line.split('//', 1)[0]
        # strip /* */ across lines
        if in_block:
            if '*/' in line:
                _, _, after = line.partition('*/')
                line = after
                in_block = False
            else:
                continue
        while '/*' in line:
            pre, _, rest = line.partition('/*')
            if '*/' in rest:
                _, _, after = rest.partition('*/')
                line = pre + after
            else:
                line = pre
                in_block = True
                break
        s = line.lstrip()
        if s.startswith('#'):
            body = s[1:].lstrip()
            if body.startswith('ifdef'):
                name = body[5:].strip()
                name = name.split()[0] if name else ''
                cond = (name in defined)
                feats = {name} if name.startswith('FEATURE_') else set()
                parent_active = stack[-1]['active'] if stack else True
                stack.append({'active': parent_active and cond, 'matched': cond, 'feats': feats})
                continue
            if body.startswith('ifndef'):
                name = body[6:].strip()
                name = name.split()[0] if name else ''
                cond = not (name in defined)
                feats = {name} if name.startswith('FEATURE_') else set()
                parent_active = stack[-1]['active'] if stack else True
                stack.append({'active': parent_active and cond, 'matched': cond, 'feats': feats})
                continue
            if body.startswith('if') and not body.startswith('ifdef') and not body.startswith('ifndef'):
                expr = body[2:].strip()
                toks = tokenize(expr)
                cond = parse_expr(toks, defined)
                feats = extract_feats(expr)
                parent_active = stack[-1]['active'] if stack else True
                stack.append({'active': parent_active and cond, 'matched': cond, 'feats': feats})
                continue
            if body.startswith('elif'):
                expr = body[4:].strip()
                toks = tokenize(expr)
                cond = parse_expr(toks, defined)
                parent_active = stack[-2]['active'] if len(stack) >= 2 else True
                prev_matched = stack[-1]['matched'] if stack else False
                cond_active = parent_active and (not prev_matched) and cond
                feats = extract_feats(expr)
                stack[-1] = {'active': cond_active, 'matched': prev_matched or cond, 'feats': feats}
                continue
            if body.startswith('else'):
                parent_active = stack[-2]['active'] if len(stack) >= 2 else True
                prev_matched = stack[-1]['matched'] if stack else False
                cond_active = parent_active and (not prev_matched)
                stack[-1] = {'active': cond_active, 'matched': True, 'feats': set()}
                continue
            if body.startswith('endif'):
                if stack:
                    stack.pop()
                continue
        m = detour_line_re.match(line)
        if not m:
            continue
        lhs = m.group(1).strip()
        rhs = m.group(2).strip()
        mfunc = re.search(r"\b(DetourCreateModRF|DetourCreateRF|DetourCreateR|DetourCreate|DETOUR_PTR_R|DETOUR_MOD_R|DETOUR_PTR|DETOUR_MOD)\s*\(", rhs)
        if not mfunc:
            continue
        func = mfunc.group(1)
        # extract argument string inside the matched function call's parentheses
        start = mfunc.end() - 1  # position of '('
        depth = 0
        j = start
        while j < len(rhs):
            ch = rhs[j]
            if ch == '(':
                depth += 1
            elif ch == ')':
                depth -= 1
                if depth == 0:
                    break
            j += 1
        args_str = rhs[start+1:j].strip() if 0 <= start < len(rhs) and 0 <= j < len(rhs) else ''
        args_list = split_args(args_str)
        active = all(frame['active'] for frame in stack) if stack else True
        feats = set()
        if active and stack:
            for frame in stack:
                if frame['active']:
                    feats |= {f for f in frame['feats'] if f in defined}
        reason = ''
        # Determine reason location
        reason_feature = ''
        if func in ('DETOUR_PTR_R', 'DetourCreateR'):
            if len(args_list) >= 5:
                reason = strip_string_quotes(args_list[-1])
        elif func in ('DETOUR_MOD_R',):
            if len(args_list) >= 6:
                reason = strip_string_quotes(args_list[-1])
        elif func == 'DetourCreate':
            # Module-form with reason has 6 args
            if len(args_list) >= 6:
                reason = strip_string_quotes(args_list[-1])
        elif func in ('DetourCreateRF', 'DetourCreateModRF'):
            # RF variants: last two args are reason, feature
            if len(args_list) >= 6:
                reason = strip_string_quotes(args_list[-2])
                reason_feature = strip_string_quotes(args_list[-1])
        elif func in ('DETOUR_PTR', 'DETOUR_MOD'):
            reason = ''
        results.append({
            'file': str(p.relative_to(ROOT)).replace('\\', '/'),
            'line': i,
            'code': f"{lhs} = {func}({args_str})",
            'symbol': lhs,
            'features': sorted(feats),
            'active': 1 if active else 0,
            'reason': reason,
            'reason_feature': reason_feature,
        })
    return results

def infer_feature_descriptions(row):
    file = row['file']
    sym = row.get('symbol','')
    code = row.get('code','')
    labels = []
    if file.endswith('features/scaled_font.cpp'):
        labels.append('Font Scaling')
    if file.endswith('features/media_timers.cpp'):
        labels.append('Media Timers')
    if file.endswith('features/ref_fixes.cpp'):
        # Map common ref fixes to friendly names
        if 'GL_BuildPolygonFromSurface' in sym or 'GL_BuildPolygonFromSurface' in code:
            labels.append('HD Textures')
        if 'drawTeamIcons' in sym or 'drawTeamIcons' in code:
            labels.append('Teamicon Fix')
        if 'R_BlendLightmaps' in sym or 'R_BlendLightmaps' in code:
            labels.append('Alt Lighting')
        if 'VID_LoadRefresh' in sym or 'VID_LoadRefresh' in code:
            labels.append('Renderer reload hook')
        if 'VID_CheckChanges' in sym or 'VID_CheckChanges' in code:
            labels.append('VSync reapply on mode change')
        if not labels:
            labels.append('Renderer fixes')
    if file.endswith('src/sof_buddy.cpp'):
        if 'FS_InitFilesystem' in sym:
            labels.append('Core filesystem init hook')
        if 'Cbuf_AddLateCommands' in sym:
            labels.append('Post-cmdline init hook')
        if 'Sys_GetGameApi' in sym:
            labels.append('Game API hook')
        if 'CinematicFreeze' in sym or 'CinematicFreeze' in code:
            labels.append('Cinematic Freeze Fix')
    return labels

def main():
    defined = read_defined_features()
    all_rows = []
    for p in sorted(SRC.rglob('*.cpp')):
        if p.name == 'detour_tracker.cpp':
            continue
        all_rows.extend(scan_file(p, defined))

    # Write TXT
    with OUT_TXT.open('w', encoding='utf-8') as f:
        from datetime import datetime, timezone
        f.write('# Detours (prebuild scan)\n')
        f.write(f"# Generated: {datetime.now(timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')}\n")
        f.write(f"# Source directory: {SRC}\n\n")
        for r in all_rows:
            f.write(f"{r['file']}:{r['line']} | {r['code']}\n")
            feats = r.get('reason_feature') or (','.join(r['features']) if r['features'] else '-')
            desc = r.get('reason') or '-'
            f.write(f"  features: {feats}\n")
            f.write(f"  description: {desc}\n")
            f.write(f"  active: {r['active']}\n\n")

    # Write MD grouped by detour name (lhs symbol), collecting reasons and features across files
    with OUT_MD.open('w', encoding='utf-8') as f:
        from datetime import datetime, timezone
        f.write('# Detours (prebuild scan)\n\n')
        f.write(f"- Generated: {datetime.now(timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')}\n")
        f.write(f"- Source: {SRC}\n")
        # Group
        groups = {}
        for r in all_rows:
            key = r['symbol']
            g = groups.setdefault(key, {'reasons': [], 'features': set(), 'codes': set(), 'active_any': 0})
            if r.get('reason'):
                g['reasons'].append(r['reason'])
            feat = r.get('reason_feature') or (','.join(r['features']) if r['features'] else '')
            if feat:
                for part in feat.split(','):
                    part = part.strip()
                    if part:
                        g['features'].add(part)
            g['codes'].add(r['code'])
            g['active_any'] = g['active_any'] or r['active']
        f.write(f"- Count: {len(groups)}\n\n")
        for name in sorted(groups.keys()):
            g = groups[name]
            ftrs = ', '.join(sorted(g['features'])) if g['features'] else '-'
            f.write(f"### {name}\n\n")
            f.write(f"- Features: {ftrs}\n")
            if g['reasons']:
                f.write(f"- Descriptions:\n")
                for idx, reason in enumerate(g['reasons']):
                    f.write(f"  - {reason}\n")
            else:
                f.write(f"- Descriptions: -\n")
            # Optionally show one representative code
            if g['codes']:
                example = sorted(g['codes'])[0]
                f.write(f"- Example: `{example}`\n")
            f.write(f"- Active: {1 if g['active_any'] else 0}\n\n")

    # Write header
    with OUT_HDR.open('w', encoding='utf-8') as f:
        f.write('#ifndef DETOURS_PREBUILD_H\n')
        f.write('#define DETOURS_PREBUILD_H\n')
        f.write(f'static const int kDetoursPrebuildCount = {len(all_rows)};\n')
        f.write('static const char* kDetoursPrebuild[] = {\n')
        for r in all_rows:
            feats = r.get('reason_feature') or (','.join(r['features']) if r['features'] else '-')
            desc = r.get('reason') or '-'
            line = f"{r['file']}:{r['line']} | {r['code']} | features={feats} | desc={desc} | active={r['active']}"
            line = line.replace('\\', '\\\\').replace('"', '\\"')
            f.write(f'    "{line}",\n')
        f.write('};\n')
        f.write('#endif\n')

if __name__ == '__main__':
    sys.exit(main())


