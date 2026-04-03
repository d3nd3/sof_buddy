# SoF Buddy — IDA MCP bridge (HTTP, localhost only).
# Install: copy or symlink this file into IDA's plugins/ directory (see ida_plugin_mcp/README.md).
# Requires an IDB open. Starts a JSON-RPC HTTP server so the Cursor MCP bridge can query xrefs.

from __future__ import annotations

import json
import os
import queue
import threading
import time
import traceback
from http.server import BaseHTTPRequestHandler, HTTPServer
from socketserver import ThreadingMixIn

import ida_funcs
import ida_kernwin
import ida_name
import idautils
import idc

try:
    import ida_lines
except ImportError:
    ida_lines = None

try:
    import ida_hexrays
except ImportError:
    ida_hexrays = None

try:
    import idaapi
except ImportError:
    idaapi = None

try:
    import ida_search
except ImportError:
    ida_search = None

try:
    import ida_segment
except ImportError:
    ida_segment = None

try:
    import ida_struct
except ImportError:
    ida_struct = None

try:
    import ida_ida
except ImportError:
    ida_ida = None

try:
    import ida_typeinf
except ImportError:
    ida_typeinf = None

try:
    import ida_idaapi
except ImportError:
    ida_idaapi = None

DEFAULT_PORT = 31337
BIND_HOST = "127.0.0.1"

# Bump when RPC capabilities change (MCP bridge can compare `ida_ping` / `supported_ops`).
PLUGIN_VERSION = "4"
SUPPORTED_OPS = (
    "ping",
    "resolve",
    "find",
    "callers",
    "callees",
    "disasm",
    "disasm_linear",
    "decompile",
    "xrefs_to",
    "xrefs_from",
    "search_bytes",
    "search_text",
    "get_struct",
)


def _bridge_info() -> dict:
    return {
        "plugin_version": PLUGIN_VERSION,
        "supported_ops": list(SUPPORTED_OPS),
    }


_server: HTTPServer | None = None
_server_thread: threading.Thread | None = None


def _qstr(s):
    if s is None:
        return None
    t = str(s).strip()
    if not t or t == "None":
        return None
    return t


def demangled_long_at(ea):
    """Long demangled name with parameters when available (same strategy as ida_dump_main)."""
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
        s = _qstr(ida_name.demangle_name(raw, 0, ida_name.DQT_FULL))
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


def func_start(ea):
    f = ida_funcs.get_func(ea)
    return f.start_ea if f else idc.BADADDR


def name_at(ea):
    n = ida_name.get_name(ea)
    return n or f"sub_{ea:X}"


def resolve_ea(s: str) -> int:
    if not s or not str(s).strip():
        return idc.BADADDR
    t = str(s).strip()
    if t.lower().startswith("0x"):
        return int(t, 16)
    if t.isdigit():
        return int(t, 10)
    try:
        if all(c in "0123456789abcdefABCDEF" for c in t) and len(t) >= 4:
            return int(t, 16)
    except ValueError:
        pass
    ea = idc.get_name_ea_simple(t)
    if ea != idc.BADADDR:
        return func_start(ea)
    return idc.BADADDR


def _idb_bounds() -> tuple[int, int]:
    """(min_ea, max_ea) for whole IDB."""
    try:
        if ida_ida:
            lo = ida_ida.inf_get_min_ea()
            hi = ida_ida.inf_get_max_ea()
            if lo != idc.BADADDR and hi != idc.BADADDR:
                return lo, hi
    except Exception:
        pass
    try:
        lo = idc.get_inf_attr(idc.INF_MIN_EA)
        hi = idc.get_inf_attr(idc.INF_MAX_EA)
        return int(lo), int(hi)
    except Exception:
        return 0, 0xFFFFFFFFFFFFFFFF


def try_resolve_after_exact_fail(query: str) -> tuple[int, dict | None]:
    """
    When get_name_ea_simple / hex parse failed: same candidate pool as find, with safe auto-pick
    when unambiguous (single substring hit, or unique rank-0 exact base/mangled match).
    """
    q = (query or "").strip()
    if len(q) < 2:
        return idc.BADADDR, None
    sug = find_function_suggestions(q, 25)
    if len(sug) == 1:
        picked = sug[0]
        return int(picked["ea_hex"], 16), {
            "auto_picked": True,
            "via": "sole_substring_match",
            "picked": picked,
        }
    ql = q.lower()
    rank0 = []
    for rec in sug:
        mang = rec.get("mangled") or ""
        dem = rec.get("demangled_long") or mang
        if _suggestion_rank(ql, mang, dem) == 0:
            rank0.append(rec)
    if len(rank0) == 1:
        picked = rank0[0]
        return int(picked["ea_hex"], 16), {
            "auto_picked": True,
            "via": "unique_exact_name_match",
            "picked": picked,
        }
    return idc.BADADDR, None


def resolve_ea_or_auto(sym: str) -> tuple[int, dict | None]:
    """Exact resolve first; then the same auto-pick rules as find's top matches."""
    ea = resolve_ea(sym)
    if ea != idc.BADADDR:
        return ea, None
    return try_resolve_after_exact_fail(str(sym))


def _suggestion_rank(ql: str, mang: str, dem: str) -> int:
    """Lower is better. ql is lowercased needle."""
    ml, dl = mang.lower(), dem.lower()
    base = dl.split("(")[0].strip().lower()
    if ml == ql or base == ql:
        return 0
    if ml.startswith(ql) or base.startswith(ql) or dl.startswith(ql):
        return 1
    if ql in ml:
        return 2
    if ql in dl:
        return 3
    return 99


def find_function_suggestions(query: str, limit: int = 15) -> list:
    """
    Grep-like substring search over every function entry (mangled + long demangled).
    Scans the whole IDB — can take a moment on huge binaries.
    """
    q = (query or "").strip()
    if len(q) < 2:
        return []
    ql = q.lower()
    lim = max(1, min(int(limit), 100))
    scored = []
    for ea in idautils.Functions():
        mang = ida_name.get_name(ea)
        if not mang:
            continue
        dem = demangled_long_at(ea) or mang
        ml, dl = mang.lower(), dem.lower()
        if ql not in ml and ql not in dl:
            continue
        rank = _suggestion_rank(ql, mang, dem)
        scored.append((rank, len(mang), ea))
    scored.sort(key=lambda x: (x[0], x[1], x[2]))
    out = []
    seen = set()
    for _, _, ea in scored:
        if ea in seen:
            continue
        seen.add(ea)
        out.append(func_record(ea))
        if len(out) >= lim:
            break
    return out


def _with_suggestions(query: str, err: str, limit: int = 12) -> dict:
    sug = find_function_suggestions(query, limit)
    out = {"ok": False, "error": err, "suggestions": sug}
    if sug:
        out["hint"] = (
            "No exact name/address match. Pick a suggestion below, use a full mangled name, "
            "or pass ea_hex (e.g. 0x8158734). You can also call ida_find_functions with the same pattern."
        )
    else:
        out["hint"] = (
            "No substring matches either; try ida_find_functions with a shorter pattern (min 2 chars) "
            "or an address."
        )
    return out


def func_record(ea: int) -> dict:
    start = func_start(ea)
    if start == idc.BADADDR:
        start = ea
    mangled = name_at(start)
    dem = demangled_long_at(start)
    return {
        "ea_hex": f"{start:#x}",
        "mangled": mangled,
        "demangled_long": dem or mangled,
    }


def op_resolve(arg: str) -> dict:
    ea, pick = resolve_ea_or_auto(str(arg))
    if ea == idc.BADADDR:
        return _with_suggestions(arg, f"could not resolve {arg!r}", limit=12)
    rec = func_record(ea)
    out: dict = {"ok": True, "function": rec}
    if pick and pick.get("auto_picked"):
        out["auto_resolved"] = True
        out["resolution"] = pick
        p = pick.get("picked") or {}
        out["hint"] = (
            f"Exact symbol not found; auto-picked {_fmt_pick_reason(pick)} — "
            f"`{p.get('mangled')}` @ `{p.get('ea_hex')}`. "
            "Prefer `ea_hex` or full mangled name when scripting; call `ida_find_functions` if unsure."
        )
    return out


def _fmt_pick_reason(pick: dict) -> str:
    via = pick.get("via") or ""
    if via == "sole_substring_match":
        return "the only substring match (same search as find)"
    if via == "unique_exact_name_match":
        return "the only exact demangled-base / mangled match among candidates"
    return "one unambiguous suggestion"


def op_find(arg: str, limit: int) -> dict:
    t0 = time.perf_counter()
    t = str(arg).strip() if arg is not None else ""
    if len(t) < 2:
        return {
            "ok": False,
            "error": "pattern must be at least 2 characters",
            "error_code": "pattern_too_short",
        }
    total_fn = None
    try:
        total_fn = int(ida_funcs.get_func_qty())
    except Exception:
        pass
    matches = find_function_suggestions(t, limit)
    elapsed_ms = int((time.perf_counter() - t0) * 1000.0)
    out = {
        "ok": True,
        "pattern": t,
        "matches": matches,
        "count": len(matches),
        "elapsed_ms": elapsed_ms,
    }
    if total_fn is not None:
        out["total_functions"] = total_fn
    return out


def _attach_resolution(out: dict, pick: dict | None) -> dict:
    if pick and pick.get("auto_picked"):
        out["auto_resolved"] = True
        out["resolution"] = pick
    return out


def op_callers(arg: str, max_results: int = 0) -> dict:
    ea, pick = resolve_ea_or_auto(str(arg))
    if ea == idc.BADADDR:
        return _with_suggestions(arg, f"could not resolve {arg!r}", limit=12)
    target = func_start(ea)
    if target == idc.BADADDR:
        return _with_suggestions(arg, f"not in a function: {arg!r}", limit=12)
    callers = set()
    for xref in idautils.XrefsTo(target, 0):
        if xref.type not in (idc.fl_CN, idc.fl_CF):
            continue
        c = func_start(xref.frm)
        if c != idc.BADADDR and c != target:
            callers.add(c)
    rows = sorted(callers)
    total = len(rows)
    truncated = False
    if max_results > 0 and total > max_results:
        rows = rows[:max_results]
        truncated = True
    return _attach_resolution(
        {
            "ok": True,
            "target": func_record(target),
            "callers": [func_record(x) for x in rows],
            "count": total,
            "returned": len(rows),
            "truncated": truncated,
        },
        pick,
    )


def op_callees(arg: str, max_results: int = 0) -> dict:
    ea, pick = resolve_ea_or_auto(str(arg))
    if ea == idc.BADADDR:
        return _with_suggestions(arg, f"could not resolve {arg!r}", limit=12)
    target = func_start(ea)
    if target == idc.BADADDR:
        return _with_suggestions(arg, f"not in a function: {arg!r}", limit=12)
    out = set()
    f = ida_funcs.get_func(target)
    if not f:
        return {"ok": False, "error": "get_func failed"}
    for head in idautils.Heads(f.start_ea, f.end_ea):
        for xref in idautils.XrefsFrom(head, 0):
            if xref.type not in (idc.fl_CN, idc.fl_CF):
                continue
            t = func_start(xref.to)
            if t != idc.BADADDR:
                out.add(t)
    rows = sorted(out)
    total = len(rows)
    truncated = False
    if max_results > 0 and total > max_results:
        rows = rows[:max_results]
        truncated = True
    return _attach_resolution(
        {
            "ok": True,
            "target": func_record(target),
            "callees": [func_record(x) for x in rows],
            "count": total,
            "returned": len(rows),
            "truncated": truncated,
        },
        pick,
    )


def _strip_disasm_markup(s: str) -> str:
    """Remove IDA color tags / markup so disassembly is plain text (grep/copy-paste friendly)."""
    if not s:
        return ""
    if ida_lines:
        try:
            return ida_lines.tag_remove(s)
        except Exception:
            pass
    return s


def _disasm_line(ea: int) -> str:
    s = ""
    if ida_lines:
        try:
            s = ida_lines.generate_disasm_line(ea, 0)
        except Exception:
            s = ""
    if not s:
        try:
            s = idc.GetDisasm(ea) or ""
        except Exception:
            s = ""
    return _strip_disasm_markup(s)


def _parse_ea_optional(v) -> int | None:
    if v is None or v == "":
        return None
    if isinstance(v, int):
        return v if v != idc.BADADDR else None
    s = str(v).strip()
    if not s:
        return None
    ea = resolve_ea(s)
    return ea if ea != idc.BADADDR else None


def op_disasm(
    arg: str,
    max_lines: int,
    start_ea=None,
    skip_instructions: int = 0,
) -> dict:
    """
    Linear disassembly for one function (IDA listing text, not graph view).
    Optional start_ea: begin at the head containing this EA (must lie inside the function).
    skip_instructions: extra instruction heads to skip after that anchor (prologue skip).
    """
    ea, pick = resolve_ea_or_auto(str(arg))
    if ea == idc.BADADDR:
        return _with_suggestions(arg, f"could not resolve {arg!r}", limit=12)
    target = func_start(ea)
    if target == idc.BADADDR:
        return _with_suggestions(arg, f"not in a function: {arg!r}", limit=12)
    f = ida_funcs.get_func(target)
    if not f:
        return {"ok": False, "error": "get_func failed"}
    first_ea = f.start_ea
    user_start = _parse_ea_optional(start_ea)
    if user_start is not None:
        ih = idc.get_item_head(user_start)
        if ih < f.start_ea or ih >= f.end_ea:
            return {
                "ok": False,
                "error": f"start_ea not inside resolved function (head {ih:#x} vs [{f.start_ea:#x},{f.end_ea:#x}))",
                "error_code": "start_ea_not_in_function",
                "function": func_record(target),
            }
        first_ea = ih
    try:
        sk = int(skip_instructions)
    except (TypeError, ValueError):
        sk = 0
    sk = max(0, min(sk, 1_000_000))
    lines_out = []
    truncated = False
    n = 0
    skipping = sk
    for head in idautils.Heads(f.start_ea, f.end_ea):
        if head < first_ea:
            continue
        if skipping > 0:
            skipping -= 1
            continue
        if n >= max_lines:
            truncated = True
            break
        dis = _disasm_line(head)
        lines_out.append(f"{head:#x}\t{dis}")
        n += 1
    out = {
        "ok": True,
        "function": func_record(target),
        "asm": "\n".join(lines_out),
        "line_count": n,
        "truncated": truncated,
        "disasm_plain_text": True,
        "disasm_start_ea": f"{first_ea:#x}",
        "skipped_instruction_heads": sk,
    }
    return _attach_resolution(out, pick)


def op_disasm_linear(ea_start, ea_end=None, max_lines: int = 400) -> dict:
    """Linear disassembly from an arbitrary EA (not snapped to function entry)."""
    start = _parse_ea_optional(ea_start)
    if start is None:
        t = str(ea_start or "").strip()
        return {"ok": False, "error": f"bad ea_start: {t!r}", "error_code": "bad_ea"}
    end_bound = None
    if ea_end is not None and str(ea_end).strip() != "":
        end_bound = _parse_ea_optional(ea_end)
        if end_bound is None:
            return {"ok": False, "error": f"bad ea_end: {ea_end!r}", "error_code": "bad_ea"}
    if end_bound is None:
        seg = ida_segment.getseg(start) if ida_segment else None
        if seg:
            end_bound = int(seg.end_ea)
        else:
            _, hi = _idb_bounds()
            end_bound = min(start + 0x100000, hi)
    if end_bound <= start:
        return {"ok": False, "error": "ea_end must be > ea_start", "error_code": "bad_range"}
    max_span = 0x200000
    if end_bound - start > max_span:
        end_bound = start + max_span
        clipped = True
    else:
        clipped = False
    lines_out = []
    truncated = False
    n = 0
    for head in idautils.Heads(start, end_bound):
        if n >= max_lines:
            truncated = True
            break
        dis = _disasm_line(head)
        lines_out.append(f"{head:#x}\t{dis}")
        n += 1
    fn = ida_funcs.get_func(start)
    func_info = func_record(func_start(start)) if fn else None
    return {
        "ok": True,
        "ea_start": f"{start:#x}",
        "ea_end": f"{end_bound:#x}",
        "range_clipped": clipped,
        "containing_function": func_info,
        "asm": "\n".join(lines_out),
        "line_count": n,
        "truncated": truncated,
        "disasm_plain_text": True,
    }


def _xref_kind(x) -> str:
    try:
        t = int(x.type)
    except Exception:
        t = x.type
    m = {}
    for name in ("fl_CN", "fl_CF", "fl_JN", "fl_JF", "dr_O", "dr_W", "dr_R"):
        if hasattr(idc, name):
            try:
                m[int(getattr(idc, name))] = name
            except Exception:
                pass
    if hasattr(idc, "fl_F"):
        try:
            m[int(idc.fl_F)] = "fl_F"
        except Exception:
            pass
    return m.get(t, f"type_{t}")


def _xref_record(frm: int, to: int, x) -> dict:
    fc = func_start(frm)
    fr = func_record(fc) if fc != idc.BADADDR else None
    tc = func_start(to)
    tr = func_record(tc) if tc != idc.BADADDR else None
    return {
        "from_ea": f"{frm:#x}",
        "to_ea": f"{to:#x}",
        "type": _xref_kind(x),
        "iscode": bool(getattr(x, "iscode", True)),
        "from_function": fr,
        "to_function": tr,
    }


def op_xrefs_to(ea_arg, max_results: int = 300) -> dict:
    """All xrefs to EA (instruction or data head); not limited to call xrefs."""
    ea = _parse_ea_optional(ea_arg)
    if ea is None:
        ea2, _pick = resolve_ea_or_auto(str(ea_arg))
        ea = ea2 if ea2 != idc.BADADDR else None
    if ea is None or ea == idc.BADADDR:
        return {
            "ok": False,
            "error": f"could not resolve EA: {ea_arg!r}",
            "error_code": "bad_ea",
        }
    head = idc.get_item_head(ea)
    rows = []
    for x in idautils.XrefsTo(head, 0):
        rows.append(_xref_record(int(x.frm), int(x.to), x))
    total = len(rows)
    truncated = False
    if max_results > 0 and total > max_results:
        rows = rows[:max_results]
        truncated = True
    return {
        "ok": True,
        "target_ea": f"{head:#x}",
        "xrefs": rows,
        "count": total,
        "returned": len(rows),
        "truncated": truncated,
    }


def op_xrefs_from(ea_arg, max_results: int = 300) -> dict:
    """Xrefs from this instruction head (outgoing)."""
    ea = _parse_ea_optional(ea_arg)
    if ea is None:
        ea2, _pick = resolve_ea_or_auto(str(ea_arg))
        ea = ea2 if ea2 != idc.BADADDR else None
    if ea is None or ea == idc.BADADDR:
        return {
            "ok": False,
            "error": f"could not resolve EA: {ea_arg!r}",
            "error_code": "bad_ea",
        }
    head = idc.get_item_head(ea)
    rows = []
    for x in idautils.XrefsFrom(head, 0):
        rows.append(_xref_record(int(x.frm), int(x.to), x))
    total = len(rows)
    truncated = False
    if max_results > 0 and total > max_results:
        rows = rows[:max_results]
        truncated = True
    return {
        "ok": True,
        "from_ea": f"{head:#x}",
        "xrefs": rows,
        "count": total,
        "returned": len(rows),
        "truncated": truncated,
    }


def _find_binary_next(cur: int, end: int, pat: str) -> int:
    if ida_search:
        try:
            fl = getattr(ida_search, "SEARCH_DOWN", getattr(ida_search, "SEARCH_NEXT", 0))
            return int(ida_search.find_binary(cur, end, pat, 16, fl))
        except Exception:
            pass
    try:
        return int(idc.find_binary(cur, end, pat, 16, idc.SEARCH_DOWN))
    except Exception:
        return idc.BADADDR


def op_search_bytes(pattern: str, max_hits: int = 40) -> dict:
    """IDA-style hex pattern with ?? wildcards (e.g. `8B 4D ?? E8`)."""
    pat = (pattern or "").strip()
    if len(pat) < 2:
        return {
            "ok": False,
            "error": "pattern too short",
            "error_code": "pattern_too_short",
        }
    lo, hi = _idb_bounds()
    hits = []
    cur = lo
    mh = max(1, min(int(max_hits), 500))
    while cur != idc.BADADDR and cur < hi and len(hits) < mh:
        found = _find_binary_next(cur, hi, pat)
        if found == idc.BADADDR:
            break
        hits.append(f"{found:#x}")
        try:
            cur = idc.next_head(found, hi)
            if cur == idc.BADADDR:
                cur = found + 1
        except Exception:
            cur = found + 1
    return {
        "ok": True,
        "pattern": pat,
        "hits": hits,
        "count": len(hits),
        "capped": len(hits) >= mh,
    }


def _find_text_ida_search(cur: int, end: int, text: str) -> int:
    """Prefer ida_search.find_text(start, end, ustr, flags) when available."""
    if not ida_search:
        return idc.BADADDR
    try:
        fn = getattr(ida_search, "find_text", None)
        if not fn:
            return idc.BADADDR
        fl = getattr(ida_search, "SEARCH_DOWN", getattr(idc, "SEARCH_DOWN", 1))
        r = int(fn(cur, end, text, fl))
        if r == idc.BADADDR or r >= end:
            return idc.BADADDR
        return r
    except Exception:
        return idc.BADADDR


def op_search_text(substring: str, max_hits: int = 40) -> dict:
    """Search disassembly text / listing string (substring, not regex)."""
    sub = (substring or "").strip()
    if len(sub) < 2:
        return {
            "ok": False,
            "error": "substring must be at least 2 characters",
            "error_code": "pattern_too_short",
        }
    lo, hi = _idb_bounds()
    hits = []
    mh = max(1, min(int(max_hits), 500))
    # 1) Fast path: ida_search linear range API
    cur = lo
    while cur != idc.BADADDR and cur < hi and len(hits) < mh:
        found = _find_text_ida_search(cur, hi, sub)
        if found == idc.BADADDR:
            break
        line = _disasm_line(found)
        hits.append({"ea": f"{found:#x}", "line": line[:500]})
        try:
            cur = idc.next_head(found, hi)
            if cur == idc.BADADDR:
                cur = found + 1
        except Exception:
            cur = found + 1
    if hits:
        return {
            "ok": True,
            "substring": sub,
            "hits": hits,
            "count": len(hits),
            "capped": len(hits) >= mh,
        }
    # 2) IDC find_text(ea, flags, y, x, str): first hit SEARCH_DOWN, then SEARCH_DOWN|SEARCH_NEXT
    try:
        sdown = int(getattr(idc, "SEARCH_DOWN", 1))
        snext = int(getattr(idc, "SEARCH_NEXT", 2))
    except Exception:
        sdown, snext = 1, 2
    try:
        ea = int(idc.find_text(lo, sdown, 0, 0, sub))
    except Exception:
        ea = idc.BADADDR
    while ea != idc.BADADDR and ea < hi and len(hits) < mh:
        line = _disasm_line(ea)
        hits.append({"ea": f"{ea:#x}", "line": line[:500]})
        try:
            ea = int(idc.find_text(ea, sdown | snext, 0, 0, sub))
        except Exception:
            break
    return {
        "ok": True,
        "substring": sub,
        "hits": hits,
        "count": len(hits),
        "capped": len(hits) >= mh,
    }


def _bad_node() -> int:
    if ida_idaapi:
        try:
            return int(ida_idaapi.BADNODE)
        except Exception:
            pass
    return -1


def op_get_struct(name: str) -> dict:
    """Read-only struct layout: members with offset and type string when available."""
    n = (name or "").strip()
    if not n:
        return {"ok": False, "error": "missing struct name", "error_code": "missing_name"}
    if not ida_struct:
        return {
            "ok": False,
            "error": "ida_struct not available in this IDA build",
            "error_code": "struct_module_missing",
        }
    sid = idc.get_struc_id(n)
    if sid in (idc.BADADDR, None):
        return {
            "ok": False,
            "error": f"struct not found: {n!r}",
            "error_code": "struct_not_found",
        }
    try:
        sptr = ida_struct.get_struc(sid)
    except Exception:
        sptr = None
    if not sptr:
        return {
            "ok": False,
            "error": f"get_struc failed for {n!r}",
            "error_code": "struct_load_failed",
        }
    members = []
    badn = _bad_node()
    try:
        off = int(ida_struct.get_struc_first_offset(sptr))
    except Exception:
        off = badn
    guard = 0
    while off not in (badn, idc.BADADDR) and guard < 4096:
        guard += 1
        mname = None
        try:
            mname = idc.get_member_name(sid, off)
        except Exception:
            mname = None
        tif_s = ""
        try:
            mptr = ida_struct.get_member(sptr, off)
            if mptr and ida_typeinf:
                tif = ida_typeinf.tinfo_t()
                if ida_struct.get_member_tinfo(tif, mptr):
                    tif_s = str(tif)
        except Exception:
            pass
        members.append(
            {
                "offset": int(off),
                "offset_hex": f"{int(off):#x}",
                "name": mname or f"field_{int(off):x}",
                "type": tif_s,
            }
        )
        try:
            off = int(ida_struct.get_struc_next_offset(sptr, off))
        except Exception:
            break
    try:
        sz = int(ida_struct.get_struc_size(sptr))
    except Exception:
        sz = None
    return {
        "ok": True,
        "name": n,
        "struc_id": int(sid) if sid is not None else None,
        "size": sz,
        "members": members,
        "member_count": len(members),
    }


def _func_thunk_flags(target: int) -> tuple[bool, str]:
    f = ida_funcs.get_func(target)
    if not f:
        return False, ""
    try:
        fl = int(f.flags)
        tf = int(ida_funcs.FUNC_THUNK) if hasattr(ida_funcs, "FUNC_THUNK") else 0x80
        if fl & tf:
            return True, "target is a thunk (jump stub); decompile the destination or use disasm"
    except Exception:
        pass
    return False, ""


def _decompile_failure_detail(target: int, exc: Exception | None) -> dict:
    is_thunk, thunk_hint = _func_thunk_flags(target)
    parts = {
        "likely_thunk": is_thunk,
        "thunk_hint": thunk_hint or None,
        "hexrays_exception": repr(exc) if exc else None,
    }
    return parts


def _asm_snippet_for_function(target: int, max_lines: int) -> str:
    f = ida_funcs.get_func(target)
    if not f:
        return ""
    lines_out = []
    n = 0
    for head in idautils.Heads(f.start_ea, f.end_ea):
        if n >= max_lines:
            break
        lines_out.append(f"{head:#x}\t{_disasm_line(head)}")
        n += 1
    return "\n".join(lines_out)


def op_decompile(arg: str, fallback_asm: bool = False, fallback_asm_max_lines: int = 128) -> dict:
    """Hex-Rays pseudocode (F5) for one function; optional asm fallback when decompile fails."""
    if ida_hexrays is None:
        return {
            "ok": False,
            "error": "ida_hexrays not available (Hex-Rays decompiler not installed)",
            "error_code": "no_hexrays",
            "decompile_detail": {"reason": "hexrays_module_missing"},
        }
    if not ida_hexrays.init_hexrays_plugin():
        return {
            "ok": False,
            "error": "Hex-Rays failed to initialize (not licensed or plugin unavailable)",
            "error_code": "hexrays_init_failed",
            "decompile_detail": {"reason": "hexrays_init_failed"},
        }
    ea, pick = resolve_ea_or_auto(str(arg))
    if ea == idc.BADADDR:
        r = _with_suggestions(arg, f"could not resolve {arg!r}", limit=12)
        r["error_code"] = "resolve_failed"
        return r
    target = func_start(ea)
    if target == idc.BADADDR:
        r = _with_suggestions(arg, f"not in a function: {arg!r}", limit=12)
        r["error_code"] = "not_in_function"
        return r
    is_thunk, thunk_hint = _func_thunk_flags(target)
    if is_thunk:
        out = {
            "ok": False,
            "error": "function is a thunk; Hex-Rays often returns no pseudocode for thunks",
            "error_code": "decompile_thunk",
            "function": func_record(target),
            "decompile_detail": {
                "reason": "thunk",
                "hint": thunk_hint,
            },
        }
        if fallback_asm:
            ml = max(1, min(int(fallback_asm_max_lines), 2000))
            out["fallback_asm"] = _asm_snippet_for_function(target, ml)
            out["fallback_asm_note"] = f"first {ml} instruction heads from function entry"
        return _attach_resolution(out, pick)
    cfunc = None
    exc: Exception | None = None
    try:
        cfunc = ida_hexrays.decompile(target)
    except Exception as e:
        exc = e
    DecompFail = getattr(ida_hexrays, "DecompilationFailure", None)
    if exc is not None:
        code = "hexrays_decompilation_failure" if DecompFail and isinstance(exc, DecompFail) else "decompile_failed"
        out = {
            "ok": False,
            "error": f"decompilation failed: {exc}",
            "error_code": code,
            "function": func_record(target),
            "decompile_detail": _decompile_failure_detail(target, exc),
        }
        if fallback_asm:
            ml = max(1, min(int(fallback_asm_max_lines), 2000))
            out["fallback_asm"] = _asm_snippet_for_function(target, ml)
            out["fallback_asm_note"] = f"first {ml} instruction heads from function entry"
        return _attach_resolution(out, pick)
    if not cfunc:
        detail = _decompile_failure_detail(target, None)
        detail["reason"] = "hexrays_returned_none"
        detail["hint"] = (
            "Hex-Rays returned no cfunc (library/no-lumina, opaque code, or internal limit). "
            "Use disasm with start_ea inside the function, or xref/callees to narrow logic."
        )
        out = {
            "ok": False,
            "error": "decompile returned None (see decompile_detail)",
            "error_code": "decompile_none",
            "function": func_record(target),
            "decompile_detail": detail,
        }
        if fallback_asm:
            ml = max(1, min(int(fallback_asm_max_lines), 2000))
            out["fallback_asm"] = _asm_snippet_for_function(target, ml)
            out["fallback_asm_note"] = f"first {ml} instruction heads from function entry"
        return _attach_resolution(out, pick)
    sv = cfunc.get_pseudocode()
    parts = []
    for sl in sv:
        try:
            if ida_lines:
                line = ida_lines.tag_remove(sl.line)
            else:
                line = str(sl.line)
        except Exception:
            line = str(getattr(sl, "line", sl))
        parts.append(line)
    text = "\n".join(parts)
    out = {
        "ok": True,
        "function": func_record(target),
        "pseudocode": text,
    }
    return _attach_resolution(out, pick)


def _dispatch(body: dict) -> dict:
    op = body.get("op") or body.get("method")
    arg = body.get("arg") or body.get("params") or body.get("name")
    try:
        lim = int(body.get("limit", 15))
    except (TypeError, ValueError):
        lim = 15
    lim = max(1, min(lim, 100))

    if op in ("ping", "health"):
        out = {
            "ok": True,
            "idb": idc.get_idb_path(),
            "input_file": idc.get_input_file_path(),
        }
        out.update(_bridge_info())
        return out
    if op in ("resolve", "resolve_function"):
        if not arg:
            return {"ok": False, "error": "missing arg (symbol or 0x address)"}
        return op_resolve(str(arg))
    if op in ("find", "find_functions", "search_functions"):
        return op_find(str(arg) if arg is not None else "", lim)
    if op in ("callers", "get_callers"):
        if not arg:
            return {"ok": False, "error": "missing arg (symbol or 0x address)"}
        try:
            mr = int(body.get("max_results", 0))
        except (TypeError, ValueError):
            mr = 0
        mr = max(0, min(mr, 50_000))
        return op_callers(str(arg), mr)
    if op in ("callees", "get_callees"):
        if not arg:
            return {"ok": False, "error": "missing arg (symbol or 0x address)"}
        try:
            mr = int(body.get("max_results", 0))
        except (TypeError, ValueError):
            mr = 0
        mr = max(0, min(mr, 50_000))
        return op_callees(str(arg), mr)
    if op in ("disasm", "asm", "get_asm", "function_asm"):
        if not arg:
            return {"ok": False, "error": "missing arg (symbol or 0x address)"}
        try:
            ml = int(body.get("max_lines", 2000))
        except (TypeError, ValueError):
            ml = 2000
        ml = max(1, min(ml, 100_000))
        try:
            sk = int(body.get("skip_instructions", body.get("skip_lines", 0)))
        except (TypeError, ValueError):
            sk = 0
        start_ea = body.get("start_ea") or body.get("start_from_ea")
        return op_disasm(str(arg), ml, start_ea=start_ea, skip_instructions=sk)
    if op in ("disasm_linear", "linear_disasm", "asm_linear"):
        try:
            ml = int(body.get("max_lines", 400))
        except (TypeError, ValueError):
            ml = 400
        ml = max(1, min(ml, 50_000))
        ea_s = body.get("ea_start") or body.get("start_ea") or arg
        if ea_s is None or str(ea_s).strip() == "":
            return {"ok": False, "error": "missing ea_start (hex EA string)"}
        ea_e = body.get("ea_end") or body.get("end_ea")
        return op_disasm_linear(ea_s, ea_e, ml)
    if op in ("decompile", "pseudocode", "hexrays", "f5"):
        if not arg:
            return {"ok": False, "error": "missing arg (symbol or 0x address)"}
        fb = body.get("fallback_asm", body.get("include_asm_on_failure", False))
        if isinstance(fb, str):
            fb = fb.strip().lower() in ("1", "true", "yes", "on")
        try:
            fbl = int(body.get("fallback_asm_max_lines", body.get("fallback_max_lines", 128)))
        except (TypeError, ValueError):
            fbl = 128
        return op_decompile(str(arg), fallback_asm=bool(fb), fallback_asm_max_lines=fbl)
    if op in ("xrefs_to", "xrefsto", "xrefs2"):
        ea_s = body.get("ea") or body.get("address") or arg
        if ea_s is None or str(ea_s).strip() == "":
            return {"ok": False, "error": "missing ea (hex or symbol)"}
        try:
            mr = int(body.get("max_results", 300))
        except (TypeError, ValueError):
            mr = 300
        mr = max(0, min(mr, 10_000))
        return op_xrefs_to(ea_s, mr)
    if op in ("xrefs_from", "xrefsfrom", "xrefsfromea"):
        ea_s = body.get("ea") or body.get("address") or arg
        if ea_s is None or str(ea_s).strip() == "":
            return {"ok": False, "error": "missing ea (hex or symbol)"}
        try:
            mr = int(body.get("max_results", 300))
        except (TypeError, ValueError):
            mr = 300
        mr = max(0, min(mr, 10_000))
        return op_xrefs_from(ea_s, mr)
    if op in ("search_bytes", "find_bytes", "bin_search"):
        pat = body.get("pattern") or body.get("arg") or arg
        if pat is None or str(pat).strip() == "":
            return {"ok": False, "error": "missing pattern (hex bytes with ?? wildcards)"}
        try:
            mh = int(body.get("max_hits", 40))
        except (TypeError, ValueError):
            mh = 40
        return op_search_bytes(str(pat), mh)
    if op in ("search_text", "find_text", "text_search"):
        sub = body.get("substring") or body.get("pattern") or body.get("arg") or arg
        if sub is None or str(sub).strip() == "":
            return {"ok": False, "error": "missing substring"}
        try:
            mh = int(body.get("max_hits", 40))
        except (TypeError, ValueError):
            mh = 40
        return op_search_text(str(sub), mh)
    if op in ("get_struct", "struct", "struct_layout"):
        nm = body.get("name") or body.get("struct") or arg
        if nm is None or str(nm).strip() == "":
            return {"ok": False, "error": "missing struct name"}
        return op_get_struct(str(nm))
    err = {"ok": False, "error": f"unknown op: {op!r}"}
    err.update(_bridge_info())
    return err


def ida_sync_dispatch(body: dict) -> dict:
    out: dict = {}
    err: list[str] = []

    def work():
        try:
            out.clear()
            out.update(_dispatch(body))
        except Exception:
            err.append(traceback.format_exc())
            out.clear()
            out.update({"ok": False, "error": err[0] if err else "exception"})
        return 0

    ida_kernwin.execute_sync(work, ida_kernwin.MFF_READ)
    return out


class _Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, fmt, *args):
        pass

    def _send_json(self, code: int, obj: dict):
        data = json.dumps(obj).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(data)

    def do_POST(self):
        if self.path not in ("/rpc", "/"):
            self._send_json(404, {"ok": False, "error": "not found"})
            return
        try:
            n = int(self.headers.get("Content-Length", "0"))
            raw = self.rfile.read(n) if n else b"{}"
            body = json.loads(raw.decode("utf-8") or "{}")
        except Exception as e:
            self._send_json(400, {"ok": False, "error": f"bad json: {e}"})
            return
        result = ida_sync_dispatch(body)
        self._send_json(200, result)

    def do_GET(self):
        if self.path == "/health":
            q: queue.Queue = queue.Queue()

            def work():
                h = {
                    "ok": True,
                    "idb": idc.get_idb_path(),
                    "input_file": idc.get_input_file_path(),
                }
                h.update(_bridge_info())
                q.put(h)
                return 0

            ida_kernwin.execute_sync(work, ida_kernwin.MFF_READ)
            self._send_json(200, q.get())
            return
        self._send_json(
            404,
            {
                "ok": False,
                "error": "POST /rpc with JSON {op, arg} — ops: ping, resolve, find, callers, callees, disasm, disasm_linear, decompile, xrefs_to, xrefs_from, search_bytes, search_text, get_struct",
            },
        )


class ThreadingHTTPServer(ThreadingMixIn, HTTPServer):
    daemon_threads = True


def _start_server(port: int):
    global _server, _server_thread
    if _server is not None:
        return
    _server = ThreadingHTTPServer((BIND_HOST, port), _Handler)
    _server.allow_reuse_address = True

    def run():
        _server.serve_forever()

    _server_thread = threading.Thread(target=run, name="sof_buddy_ida_mcp", daemon=True)
    _server_thread.start()
    print(
        f"[SoF Buddy IDA MCP] v{PLUGIN_VERSION} listening on http://{BIND_HOST}:{port}/rpc (POST JSON)"
    )


def _stop_server():
    global _server, _server_thread
    if _server:
        _server.shutdown()
        _server.server_close()
        _server = None
    _server_thread = None


class sof_buddy_ida_mcp_plugin_t(idaapi.plugin_t if idaapi else object):
    flags = idaapi.PLUGIN_KEEP if idaapi else 0
    comment = "SoF Buddy: HTTP MCP bridge (callers/callees, disasm, Hex-Rays)"
    help = "Exposes POST /rpc for Cursor MCP bridge (disasm, decompile, xrefs). Binds 127.0.0.1 only."
    wanted_name = "SoF Buddy IDA MCP"
    wanted_hotkey = ""

    def init(self):
        if not idaapi:
            return 0
        port = int(os.environ.get("IDA_MCP_PORT", str(DEFAULT_PORT)))
        try:
            _start_server(port)
        except OSError as e:
            print(f"[SoF Buddy IDA MCP] failed to bind {BIND_HOST}:{port}: {e}")
            return idaapi.PLUGIN_SKIP
        print(f"[SoF Buddy IDA MCP] IDB: {idc.get_idb_path()}")
        return idaapi.PLUGIN_KEEP

    def run(self, arg):
        if idaapi:
            port = int(os.environ.get("IDA_MCP_PORT", str(DEFAULT_PORT)))
            idaapi.info(f"SoF Buddy IDA MCP: http://127.0.0.1:{port}/rpc\n")

    def term(self):
        _stop_server()


def PLUGIN_ENTRY():
    if not idaapi:
        return None
    return sof_buddy_ida_mcp_plugin_t()
