# ida_dump_main.py — run inside IDA (File → Script file) with the target IDB open.
# Writes a readable .md: demangler example → main → Qcommon_Frame → Qcommon_Frame's children.
# IDA 9: uses ida_nalt.get_tinfo (not ida_typeinf.get_tinfo).

import os

import ida_funcs
import ida_name
import ida_nalt
import ida_typeinf
import idautils
import idc

# demangle_name(name, mask): mask must be INF_LONG_DN / INF_SHORT_DN from idainfo,
# not MNG_* (those are a different convention and yield short names like "HandleGroundContact").

ROOT_NAME = "main"
QCOMMON_NAMES = ("Qcommon_Frame__Fi", "Qcommon_Frame")
# None → ~/Desktop/sof_calltree.md
OUTPUT_MD = None
# Under each Qcommon_Frame child, list one level of callees as bullets
LIST_GRANDCHILDREN = True
# Shown at top of .md to verify demangler output (GCC mangling)
DEMANGLE_EXAMPLE_SYMBOL = "HandleGroundContact__FP14entity_state_sPfii"


def func_start(ea):
    f = ida_funcs.get_func(ea)
    return f.start_ea if f else idc.BADADDR


def callees_of(func_ea):
    out = set()
    f = ida_funcs.get_func(func_ea)
    if not f:
        return out
    for head in idautils.Heads(f.start_ea, f.end_ea):
        for xref in idautils.XrefsFrom(head, 0):
            if xref.type not in (idc.fl_CN, idc.fl_CF):
                continue
            t = func_start(xref.to)
            if t != idc.BADADDR:
                out.add(t)
    return out


def name_at(ea):
    n = ida_name.get_name(ea)
    return n or f"sub_{ea:X}"


def _qstr(s):
    if s is None:
        return None
    t = str(s).strip()
    if not t or t == "None":
        return None
    return t


def demangled_at(ea):
    raw = ida_name.get_name(ea)
    if not raw:
        return None
    candidates = []

    try:
        ln = ida_name.get_long_name(
            ea, ida_name.GN_DEMANGLED | ida_name.GN_LONG
        )
        s = _qstr(ln)
        if s and s != raw:
            candidates.append(s)
    except Exception:
        pass

    for inf_attr in (idc.INF_LONG_DN, idc.INF_SHORT_DN):
        try:
            mask = idc.get_inf_attr(inf_attr)
            s = _qstr(idc.demangle_name(raw, mask))
            if s and s != raw:
                candidates.append(s)
        except Exception:
            pass

    try:
        s = _qstr(
            ida_name.demangle_name(raw, 0, ida_name.DQT_FULL)
        )
        if s and s != raw:
            candidates.append(s)
    except Exception:
        pass

    if not candidates:
        return None
    with_args = [c for c in candidates if "(" in c]
    if with_args:
        return max(with_args, key=len)
    return max(candidates, key=len)


def _get_tinfo_at(ea, tif):
    fn = getattr(ida_nalt, "get_tinfo", None)
    if fn:
        try:
            if fn(ea, tif):
                return True
        except TypeError:
            pass
        try:
            if fn(tif, ea):
                return True
        except TypeError:
            pass
    legacy = getattr(ida_typeinf, "get_tinfo", None)
    if legacy:
        try:
            if legacy(tif, ea):
                return True
        except TypeError:
            pass
        try:
            if legacy(ea, tif):
                return True
        except TypeError:
            pass
    return False


def _tinfo_to_cstr(tif):
    if hasattr(tif, "dstr"):
        s = tif.dstr()
        if s:
            return s
    if hasattr(tif, "_print"):
        s = tif._print()
        if s:
            return s
    name = ""
    flags = ida_typeinf.PRTYPE_SEMI
    if hasattr(ida_typeinf, "PRTYPE_1LINE"):
        flags |= ida_typeinf.PRTYPE_1LINE
    elif hasattr(ida_typeinf, "PRTYPE_1"):
        flags |= ida_typeinf.PRTYPE_1
    try:
        return ida_typeinf.print_tinfo(tif, name, flags)
    except Exception:
        return None


def prototype_at(ea):
    s = idc.get_type(ea)
    if s:
        return s
    tif = ida_typeinf.tinfo_t()
    if not _get_tinfo_at(ea, tif):
        return None
    return _tinfo_to_cstr(tif)


def describe_func(ea):
    raw = name_at(ea)
    dm = demangled_at(ea)
    proto = prototype_at(ea)
    return {
        "name": raw,
        "demangled": dm,
        "prototype": proto,
        "display": proto or dm or raw,
    }


def heading_title(info, fallback="function"):
    if info["demangled"]:
        return info["demangled"]
    return info["name"] or fallback


def md_func_lines(info, ea):
    lines = [
        f"- **EA:** `{ea:#x}`",
        f"- **Symbol:** `{info['name']}`",
    ]
    if info["demangled"]:
        lines.append(f"- **Demangled:** `{info['demangled']}`")
    if info["prototype"]:
        lines.append(f"- **Prototype:** `{info['prototype']}`")
    return lines


def _demangled_vs_ida_prototype_note(demangled, prototype):
    if not demangled or "(" not in demangled or not prototype:
        return None
    sd = demangled.count("*")
    sp = prototype.count("*")
    if sd == sp:
        return None
    return (
        f"- **Note:** demangled parameters include `{sd}` `*` (pointers); IDA prototype shows "
        f"`{sp}`. For ABI truth, trust **demangled** until you set the function type in IDA "
        f"(keyboard **Y** on the function)."
    )


def md_demangler_example_section():
    lines = [
        "# Demangler sanity check",
        "",
        "Example symbol: **demangled** line uses `idc.demangle_name(..., get_inf_attr(INF_LONG_DN))` "
        "and `get_long_name(..., GN_DEMANGLED|GN_LONG)` — not `MNG_*` as the mask to "
        "`demangle_name` (that yields short names without `()`). **IDA prototype** is the "
        "stored type only; it can lag the mangled name.",
        "",
    ]
    ea = idc.get_name_ea_simple(DEMANGLE_EXAMPLE_SYMBOL)
    if ea == idc.BADADDR:
        lines.append(f"*Symbol `{DEMANGLE_EXAMPLE_SYMBOL}` not found — rename in IDA or adjust `DEMANGLE_EXAMPLE_SYMBOL`.*")
        lines.append("")
        return lines
    info = describe_func(ea)
    lines.append(f"- **Mangled (IDA name):** `{info['name']}`")
    if info["demangled"]:
        lines.append(f"- **Demangled (long / with parameters when available):** `{info['demangled']}`")
    else:
        lines.append("- **Demangled:** *(none — demangler returned same as symbol or failed)*")
    if info["prototype"]:
        lines.append(f"- **IDA prototype (stored type, editable with Y):** `{info['prototype']}`")
    note = _demangled_vs_ida_prototype_note(info.get("demangled"), info.get("prototype"))
    if note:
        lines.append(note)
    lines.append(f"- **EA:** `{ea:#x}`")
    lines.append("")
    lines.append("---")
    lines.append("")
    return lines


def find_qcommon_frame_ea():
    for n in QCOMMON_NAMES:
        ea = idc.get_name_ea_simple(n)
        if ea != idc.BADADDR:
            return ea
    for _, n in idautils.Names():
        if "Qcommon_Frame" in n:
            return idc.get_name_ea_simple(n)
    return idc.BADADDR


def build_markdown(main_ea, qcf_ea):
    parts = []
    parts.extend(md_demangler_example_section())
    mi = describe_func(main_ea)
    parts.append(f"# `{ROOT_NAME}`")
    parts.append("")
    parts.extend(md_func_lines(mi, main_ea))
    parts.append("")
    parts.append("## Direct callees of `" + ROOT_NAME + "`")
    parts.append("")
    parts.append("| EA | Symbol | Signature |")
    parts.append("| --- | --- | --- |")
    for c in sorted(callees_of(main_ea)):
        ci = describe_func(c)
        sig = (ci["prototype"] or ci["demangled"] or "").replace("|", "\\|")
        parts.append(f"| `{c:#x}` | `{ci['name']}` | `{sig}` |")
    parts.append("")
    parts.append("---")
    parts.append("")

    if qcf_ea == idc.BADADDR:
        parts.append("# Qcommon_Frame")
        parts.append("")
        parts.append("*Not found in IDB (rename / adjust `QCOMMON_NAMES`).*")
        return "\n".join(parts)

    qi = describe_func(qcf_ea)
    title = heading_title(qi, "Qcommon_Frame")
    parts.append(f"# Qcommon_Frame — `{title}`")
    parts.append("")
    parts.extend(md_func_lines(qi, qcf_ea))
    parts.append("")
    parts.append("## Direct callees of Qcommon_Frame")
    parts.append("")

    for c in sorted(callees_of(qcf_ea)):
        ci = describe_func(c)
        child_title = heading_title(ci, ci["name"])
        parts.append(f"### `{child_title}`")
        parts.append("")
        parts.extend(md_func_lines(ci, c))
        if LIST_GRANDCHILDREN:
            gc = sorted(callees_of(c))
            if gc:
                parts.append("")
                parts.append("#### Callees")
                parts.append("")
                for g in gc:
                    gi = describe_func(g)
                    gsig = gi["display"].replace("|", "\\|")
                    parts.append(f"- `{gi['name']}` @ `{g:#x}` — `{gsig}`")
        parts.append("")

    return "\n".join(parts)


def main():
    out_path = OUTPUT_MD or os.path.expanduser("~/Desktop/sof_calltree.md")
    main_ea = idc.get_name_ea_simple(ROOT_NAME)
    if main_ea == idc.BADADDR:
        print("Root not found:", ROOT_NAME)
        return
    qcf_ea = find_qcommon_frame_ea()
    text = build_markdown(main_ea, qcf_ea)
    with open(out_path, "w", encoding="utf-8") as f:
        f.write(text)
    print("Wrote", out_path)


main()
