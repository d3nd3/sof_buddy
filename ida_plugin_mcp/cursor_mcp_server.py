#!/usr/bin/env python3
"""
Cursor / MCP stdio server: forwards tool calls to the IDA plugin HTTP RPC.

Prerequisite: IDA running with `sof_buddy_ida_mcp.py` loaded and an IDB open.

  pip install mcp
  export IDA_MCP_BASE=http://127.0.0.1:31337   # default

Add to Cursor MCP settings (see README.md).
"""

from __future__ import annotations

import json
import os
import sys
import urllib.error
import urllib.request
from pathlib import Path

IDA_MCP_BASE = os.environ.get("IDA_MCP_BASE", "http://127.0.0.1:31337").rstrip("/")
RPC_TIMEOUT = float(os.environ.get("IDA_MCP_TIMEOUT", "120"))

# Expected plugin version from `ida_ping` when the in-IDA file matches this repo (informational).
# v4: linear disasm from EA, xrefs, byte/text search, struct export, smarter resolve/decompile.
BRIDGE_EXPECTED_PLUGIN_VERSION = "4"

PLUGIN_UPGRADE_HINT = (
    "If you see `unknown op` for `disasm` or `decompile`, the IDA plugin is older than this MCP bridge. "
    "Copy or symlink the current `ida_plugin_mcp/sof_buddy_ida_mcp.py` into IDA's `plugins/` folder and restart IDA."
)

LEGACY_PLUGIN_WARNING = (
    "**Warning:** IDA plugin looks outdated (missing `plugin_version` or non-empty `supported_ops` in `ida_ping`). "
    "Install the current `ida_plugin_mcp/sof_buddy_ida_mcp.py` into IDA's `plugins/` folder and restart IDA."
)

# Agent-friendly default cap; use `max_results=0` for unlimited (requires plugin v2+ honoring `max_results`).
DEFAULT_MAX_CALLERS_CALLEES = 80

_bridge_meta_cache: dict | None = None


def _rpc(op: str, arg: str = "", **extra) -> dict:
    url = f"{IDA_MCP_BASE}/rpc"
    payload_obj = {"op": op, "arg": arg}
    for k, v in extra.items():
        if v is not None:
            payload_obj[k] = v
    payload = json.dumps(payload_obj).encode("utf-8")
    req = urllib.request.Request(
        url,
        data=payload,
        method="POST",
        headers={"Content-Type": "application/json"},
    )
    try:
        with urllib.request.urlopen(req, timeout=RPC_TIMEOUT) as resp:
            return json.loads(resp.read().decode("utf-8"))
    except urllib.error.URLError as e:
        return {
            "ok": False,
            "error": str(e),
            "hint": "Start IDA with the SoF Buddy IDA MCP plugin and an IDB open.",
        }


def _fmt_fn(row: dict) -> str:
    dem = row.get("demangled_long") or row.get("mangled") or "?"
    mang = row.get("mangled") or ""
    eh = row.get("ea_hex") or ""
    if mang and mang != dem:
        return f"- `{dem}` @ `{eh}` _(mangled: `{mang}`)_"
    return f"- `{dem}` @ `{eh}`"


def _get_bridge_meta() -> dict:
    global _bridge_meta_cache
    if _bridge_meta_cache is None:
        _bridge_meta_cache = _rpc("ping", "")
    return _bridge_meta_cache


def _invalidate_bridge_meta() -> None:
    global _bridge_meta_cache
    _bridge_meta_cache = None


def _maybe_invalidate_on_unknown_op(data: dict) -> None:
    err = str(data.get("error", "")).lower()
    if "unknown op" in err:
        _invalidate_bridge_meta()


def _is_legacy_plugin(meta: dict) -> bool:
    """True when ping succeeded but the in-IDA plugin predates v2 metadata."""
    if not meta.get("ok"):
        return False
    pv = meta.get("plugin_version")
    so = meta.get("supported_ops")
    if (
        isinstance(pv, str)
        and pv.strip()
        and isinstance(so, list)
        and len(so) > 0
    ):
        return False
    return True


def _enriched_ping_payload(raw: dict, include_paths: bool = True) -> dict:
    """
    Normalize ping JSON for agents: explicit null / [] when legacy, plus bridge_capabilities.
    Raw IDA HTTP GET /health returns only what the plugin sends; this enrichment applies to ida_ping output.
    """
    out = dict(raw)
    if not out.get("ok"):
        return out
    legacy = _is_legacy_plugin(raw)
    if legacy:
        out["plugin_version"] = None
        out["supported_ops"] = []
    elif not isinstance(out.get("supported_ops"), list):
        out["supported_ops"] = []
    so = out.get("supported_ops") or []
    out["bridge_capabilities"] = {
        "disasm": "disasm" in so,
        "decompile": "decompile" in so,
        "disasm_linear": "disasm_linear" in so,
        "xrefs_to": "xrefs_to" in so,
        "xrefs_from": "xrefs_from" in so,
        "search_bytes": "search_bytes" in so,
        "search_text": "search_text" in so,
        "get_struct": "get_struct" in so,
        "max_results_honored": not legacy,
    }
    out["stdio_mcp_enrichment"] = True
    if not include_paths:
        out["idb"] = "<redacted>"
        out["input_file"] = "<redacted>"
    return out


def _fail_if_legacy_plugin_for_disasm_decompile(which: str) -> str | None:
    """Avoid a wasted RPC: legacy plugins always return unknown op for disasm/decompile. Short error; full upgrade text is in ida_ping."""
    meta = _get_bridge_meta()
    if not meta.get("ok"):
        return None
    if not _is_legacy_plugin(meta):
        return None
    cap = "disasm" if which == "disasm" else "decompile"
    return (
        f"**Error:** `{cap}` unavailable — IDA plugin v2+ required. "
        f"See **`ida_ping`** (`bridge_capabilities.{cap}`) and upgrade `sof_buddy_ida_mcp.py` (README)."
    )


def _maybe_note_legacy_max_results(text: str, requested_cap: bool) -> str:
    if not requested_cap:
        return text
    if not _is_legacy_plugin(_get_bridge_meta()):
        return text
    return (
        text
        + "\n\n_**Note:** This IDA plugin did not report `supported_ops`; `max_results` may be ignored until you upgrade._\n"
    )


def _check_required_ops(*ops: str) -> str | None:
    """
    If the plugin reports supported_ops and omits a required op, return a markdown error.
    Legacy plugins (no supported_ops) return None so the real RPC can run.
    """
    meta = _get_bridge_meta()
    if not meta.get("ok"):
        return None
    supported = meta.get("supported_ops")
    if not isinstance(supported, list) or not supported:
        return None
    missing = [o for o in ops if o not in supported]
    if not missing:
        return None
    return "\n".join(
        [
            "**Error:** IDA plugin does not list these ops: "
            + ", ".join(f"`{m}`" for m in missing)
            + ". See **`ida_ping`** / `supported_ops` and upgrade the in-IDA plugin (README).",
        ]
    )


def _fmt_suggestions_block(data: dict) -> str:
    sug = data.get("suggestions") or []
    if not sug:
        return ""
    lines = [
        "",
        "### Nearest matches (substring search on mangled + demangled names)",
        "",
    ]
    for row in sug:
        lines.append(_fmt_fn(row))
    return "\n".join(lines)


def _fmt_error(data: dict) -> str:
    err = data.get("error", data)
    parts = [f"**Error:** {err}"]
    ec = data.get("error_code")
    if ec:
        parts.append(f"**error_code:** `{ec}`")
    es = str(err).lower()
    if "unknown op" in es:
        parts.append("")
        parts.append(f"_{PLUGIN_UPGRADE_HINT}_")
        if isinstance(data.get("supported_ops"), list) and data["supported_ops"]:
            parts.append("")
            parts.append(
                "**Plugin reports `supported_ops`:** `"
                + "`, `".join(str(x) for x in data["supported_ops"])
                + "`"
            )
        if isinstance(data.get("plugin_version"), str):
            parts.append("")
            parts.append(
                f"**Plugin version (in IDA):** `{data['plugin_version']}` "
                f"(bridge expects `{BRIDGE_EXPECTED_PLUGIN_VERSION}` or compatible)."
            )
    if data.get("hint"):
        parts.append(f"_{data['hint']}_")
    parts.append(_fmt_suggestions_block(data))
    return "\n".join(parts)


def _fmt_result(data: dict, list_key: str, heading: str) -> str:
    if not data.get("ok"):
        return _fmt_error(data)
    tgt = data.get("target") or {}
    lines = [
        f"**Target:** {_fmt_fn(tgt).lstrip('- ')}",
        f"**Count (total):** {data.get('count', len(data.get(list_key) or []))}",
    ]
    if data.get("truncated"):
        lines.append(
            f"**Returned (after cap):** {data.get('returned', len(data.get(list_key) or []))} "
            "_(use `max_results=0` for full list when the plugin supports it)_"
        )
    lines.extend(
        [
            "",
            f"### {heading}",
            "",
        ]
    )
    for row in data.get(list_key) or []:
        lines.append(_fmt_fn(row))
    if len(data.get(list_key) or []) == 0:
        lines.append("_(none)_")
    return "\n".join(lines)


try:
    from mcp.server.fastmcp import FastMCP
except ImportError:
    print(
        "Install the MCP SDK:  python3 -m pip install mcp",
        file=sys.stderr,
    )
    sys.exit(1)

mcp = FastMCP(
    "ida-sof-buddy",
    instructions=(
        "Tools talk to a running IDA instance via HTTP (SoF Buddy IDA MCP plugin). "
        "Requires IDB open and plugin loaded. "
        "Most reliable address form: hex EA (0x…). Resolve/decompile/disasm/callers/callees auto-pick "
        "when there is exactly one substring match or one exact short-name match (same pool as ida_find_functions). "
        "Otherwise use mangled names from ida_find_functions or ida_resolve_function suggestions. "
        "ida_find_functions: case-insensitive substring, not regex; limit capped at 100. "
        "ida_get_function_asm: linear disassembly; optional start_ea inside the function to skip prologue; skip_instructions for extra heads. "
        "ida_get_disasm_linear: disassemble from any EA (not function entry). "
        "ida_xrefs_to / ida_xrefs_from: all xref types at an EA (not only calls). "
        "ida_search_bytes: hex pattern with ??; ida_search_text: listing substring search; both capped. "
        "ida_get_struct: read-only member offsets/types. "
        "ida_decompile_function: Hex-Rays when available; on failure can include fallback_asm (default on) plus decompile_detail/error_code. "
        "Call ida_ping first for plugin_version, supported_ops, bridge_capabilities. "
        "Tools return markdown; ida_ping embeds JSON in a ```json block. "
        "ida_get_callers/callees: max_results=0 for unlimited. ida_ping(include_paths=false) redacts paths."
    ),
)


@mcp.tool()
def ida_ping(include_paths: bool = True) -> str:
    """
    Check IDA bridge: IDB path, input file, plugin_version, supported_ops (v2+), and bridge_capabilities.
    When the in-IDA plugin is legacy, JSON uses plugin_version: null and supported_ops: [] for reliable agent checks.
    Set include_paths=false to redact idb / input_file to "<redacted>" in the displayed JSON (for shared logs).
    Call first to verify connectivity. Returns: optional canonical upgrade warning, then a json fenced block
    (enriched JSON includes bridge_capabilities; see README vs raw GET /health).
    """
    data = _rpc("ping", "")
    global _bridge_meta_cache
    _bridge_meta_cache = data
    if not data.get("ok"):
        return f"**Error:** {data.get('error', data)}"
    warn = ""
    if _is_legacy_plugin(data):
        warn = LEGACY_PLUGIN_WARNING + "\n\n"
    enriched = _enriched_ping_payload(data, include_paths=include_paths)
    body = json.dumps(enriched, indent=2, sort_keys=True)
    return warn + "```json\n" + body + "\n```"


@mcp.tool()
def ida_resolve_function(symbol_or_address: str) -> str:
    """
    Resolve one function: exact mangled name, IDA label, or hex EA (e.g. main, HandleGroundContact__FP…, 0x8158734).
    Plugin v4+: may auto-resolve when there is a single unambiguous match (same logic as ida_find_functions).
    If still unresolved, the response includes nearest substring matches (mangled + demangled).
    """
    data = _rpc("resolve", symbol_or_address.strip())
    if not data.get("ok"):
        return _fmt_error(data)
    fn = data.get("function") or {}
    lines = [
        f"**demangled_long:** `{fn.get('demangled_long')}`",
        f"**mangled:** `{fn.get('mangled')}`",
        f"**ea:** `{fn.get('ea_hex')}`",
    ]
    if data.get("auto_resolved"):
        lines.append("")
        lines.append(
            "**auto_resolved:** true _(exact name not found; plugin picked one candidate — prefer a hex EA when scripting)_"
        )
    if data.get("hint"):
        lines.append("")
        lines.append(f"_{data['hint']}_")
    return "\n".join(lines)


@mcp.tool()
def ida_get_callers(
    symbol_or_address: str,
    max_results: int = DEFAULT_MAX_CALLERS_CALLEES,
) -> str:
    """
    Functions that directly call the target (direct call xrefs only). Same naming rules as ida_resolve_function.
    Entry symbols (e.g. main) may show zero callers if the CRT (e.g. __libc_start_main) is not a call xref in this IDB.
    On failed resolve, returns suggested functions whose mangled or demangled name contains your string.
    max_results: default caps output; use max_results=0 for unlimited (unlike ida_find_functions where limit is always capped at 100).
    Plugin v2+ honors the cap; older plugins may ignore it until upgraded.
    """
    arg = symbol_or_address.strip()
    extra = {}
    requested_cap = False
    if max_results > 0:
        extra["max_results"] = max_results
        requested_cap = True
    data = _rpc("callers", arg, **extra)
    out = _fmt_result(data, "callers", "Callers")
    return _maybe_note_legacy_max_results(out, requested_cap)


@mcp.tool()
def ida_get_callees(
    symbol_or_address: str,
    max_results: int = DEFAULT_MAX_CALLERS_CALLEES,
) -> str:
    """
    Functions the target calls (instruction-level call xrefs). Same naming rules as ida_resolve_function.
    On failed resolve, returns substring suggestions like ida_get_callers.
    Default max_results matches ida_get_callers; use max_results=0 for all callees (ida_find_functions uses a different cap model; see tool list).
    """
    arg = symbol_or_address.strip()
    extra = {}
    requested_cap = False
    if max_results > 0:
        extra["max_results"] = max_results
        requested_cap = True
    data = _rpc("callees", arg, **extra)
    out = _fmt_result(data, "callees", "Callees")
    return _maybe_note_legacy_max_results(out, requested_cap)


@mcp.tool()
def ida_find_functions(pattern: str, max_results: int = 15) -> str:
    """
    Substring search (case-insensitive, not regex) over every function's mangled and long-demangled name.
    Pattern must be at least 2 characters. max_results is capped at 100 at the RPC (unlike callers/callees where 0 means unlimited).
    Slower on huge IDBs; response may include elapsed_ms and total_functions (plugin v3+).
    """
    data = _rpc("find", pattern.strip(), limit=max_results)
    if not data.get("ok"):
        return _fmt_error(data)
    lines = [
        f"**Pattern:** `{data.get('pattern')}`",
        f"**Count:** {data.get('count', 0)}",
    ]
    if data.get("elapsed_ms") is not None:
        lines.append(f"**elapsed_ms:** {data.get('elapsed_ms')}")
    if data.get("total_functions") is not None:
        lines.append(f"**total_functions (IDB):** {data.get('total_functions')}")
    lines.extend(
        [
            "",
            "### Matches",
            "",
        ]
    )
    for row in data.get("matches") or []:
        lines.append(_fmt_fn(row))
    if not data.get("matches"):
        lines.append("_(none)_")
    return "\n".join(lines)


@mcp.tool()
def ida_get_function_asm(
    symbol_or_address: str,
    max_lines: int = 2000,
    start_ea: str | None = None,
    skip_instructions: int = 0,
) -> str:
    """
    Linear disassembly listing for the function containing the resolved EA (one line per instruction/data head).
    Requires in-IDA plugin with `disasm` op (v2+); check ida_ping / bridge_capabilities.disasm.
    Text is plain (IDA color tags stripped) for grep/copy-paste. Resolve rules match ida_resolve_function (v4+ auto-pick).
    max_lines limits rows (1–100000, default 2000). start_ea: hex string or symbol for the first head to emit (must lie inside the function).
    skip_instructions: after that anchor, skip this many additional instruction heads (skip prologue without guessing an inner EA).
    """
    legacy = _fail_if_legacy_plugin_for_disasm_decompile("disasm")
    if legacy:
        return legacy
    pre = _check_required_ops("disasm")
    if pre:
        return pre
    extra: dict = {"max_lines": max_lines}
    if start_ea is not None and str(start_ea).strip():
        extra["start_ea"] = str(start_ea).strip()
    if skip_instructions:
        extra["skip_instructions"] = int(skip_instructions)
    data = _rpc("disasm", symbol_or_address.strip(), **extra)
    if not data.get("ok"):
        _maybe_invalidate_on_unknown_op(data)
        return _fmt_error(data)
    fn = data.get("function") or {}
    meta = [
        f"**demangled_long:** `{fn.get('demangled_long')}`",
        f"**mangled:** `{fn.get('mangled')}`",
        f"**ea:** `{fn.get('ea_hex')}`",
        f"**disasm_start_ea:** `{data.get('disasm_start_ea', fn.get('ea_hex'))}`",
        f"**skipped_instruction_heads:** {data.get('skipped_instruction_heads', 0)}",
        f"**line_count:** {data.get('line_count', 0)}",
        f"**truncated:** {data.get('truncated', False)}",
        f"**plain_text:** {data.get('disasm_plain_text', True)}",
    ]
    if data.get("auto_resolved"):
        meta.append("**auto_resolved:** true")
    head = "\n".join(
        meta
        + [
            "",
            "### Assembly",
            "",
            "```asm",
            data.get("asm") or "",
            "```",
        ]
    )
    return head


@mcp.tool()
def ida_decompile_function(
    symbol_or_address: str,
    fallback_asm: bool = True,
    fallback_max_lines: int = 128,
) -> str:
    """
    Hex-Rays decompiler pseudocode (IDA F5) for the function. Requires in-IDA plugin with `decompile` op (v2+); check ida_ping / bridge_capabilities.decompile.
    Still requires Hex-Rays installed and licensed. On failure, plugin v4+ returns decompile_detail (reason, hints) and may attach fallback_asm when fallback_asm=true.
    Resolve rules match ida_resolve_function (v4+ auto-pick). Prefer hex EA for stable scripts.
    """
    legacy = _fail_if_legacy_plugin_for_disasm_decompile("decompile")
    if legacy:
        return legacy
    pre = _check_required_ops("decompile")
    if pre:
        return pre
    data = _rpc(
        "decompile",
        symbol_or_address.strip(),
        fallback_asm=fallback_asm,
        fallback_asm_max_lines=fallback_max_lines,
    )
    if not data.get("ok"):
        _maybe_invalidate_on_unknown_op(data)
        err = _fmt_error(data)
        detail = data.get("decompile_detail")
        if detail:
            err += "\n\n### decompile_detail\n\n```json\n"
            err += json.dumps(detail, indent=2, sort_keys=True)
            err += "\n```"
        fb = data.get("fallback_asm")
        if fb:
            note = data.get("fallback_asm_note") or "assembly fallback"
            err += f"\n\n### Fallback assembly ({note})\n\n```asm\n{fb}\n```"
        return err
    fn = data.get("function") or {}
    pc = data.get("pseudocode") or ""
    lines = [
        f"**demangled_long:** `{fn.get('demangled_long')}`",
        f"**mangled:** `{fn.get('mangled')}`",
        f"**ea:** `{fn.get('ea_hex')}`",
    ]
    if data.get("auto_resolved"):
        lines.append("**auto_resolved:** true")
    lines.extend(
        [
            "",
            "### Pseudocode (Hex-Rays)",
            "",
            "```c",
            pc,
            "```",
        ]
    )
    return "\n".join(lines)


def _fmt_xrefs(data: dict, heading: str) -> str:
    if not data.get("ok"):
        return _fmt_error(data)
    lines = [
        f"**{heading}:** `{data.get('target_ea') or data.get('from_ea')}`",
        f"**Count (total):** {data.get('count', 0)}",
    ]
    if data.get("truncated"):
        lines.append(
            f"**Returned:** {data.get('returned', 0)} _(cap with max_results; use 0 for unlimited when supported)_"
        )
    lines.extend(["", "### Xrefs", ""])
    for row in data.get("xrefs") or []:
        fr = row.get("from_ea")
        to = row.get("to_ea")
        typ = row.get("type")
        ff = row.get("from_function") or {}
        tf = row.get("to_function") or {}
        fd = ff.get("demangled_long") or ff.get("mangled") or "?"
        td = tf.get("demangled_long") or tf.get("mangled") or "?"
        lines.append(
            f"- `{typ}` **from** `{fr}` _({fd})_ → **to** `{to}` _({td})_"
        )
    if not data.get("xrefs"):
        lines.append("_(none)_")
    return "\n".join(lines)


@mcp.tool()
def ida_get_disasm_linear(
    ea_start: str,
    ea_end: str | None = None,
    max_lines: int = 400,
) -> str:
    """
    Linear disassembly from an arbitrary EA (not snapped to function start). Uses the segment end as the default bound.
    ea_start / ea_end: hex strings (0x…) or symbols resolvable like ida_resolve_function. max_lines caps output (1–50000).
    """
    legacy = _fail_if_legacy_plugin_for_disasm_decompile("disasm")
    if legacy:
        return legacy
    pre = _check_required_ops("disasm_linear")
    if pre:
        return pre
    extra: dict = {"max_lines": max_lines}
    if ea_end is not None and str(ea_end).strip():
        extra["ea_end"] = str(ea_end).strip()
    data = _rpc("disasm_linear", "", ea_start=str(ea_start).strip(), **extra)
    if not data.get("ok"):
        _maybe_invalidate_on_unknown_op(data)
        return _fmt_error(data)
    cf = data.get("containing_function")
    cfl = ""
    if cf:
        cfl = f"**containing_function:** {_fmt_fn(cf).lstrip('- ')}"
    else:
        cfl = "**containing_function:** _(none / not in a function)_"
    return "\n".join(
        [
            f"**ea_start:** `{data.get('ea_start')}`",
            f"**ea_end:** `{data.get('ea_end')}`",
            f"**range_clipped:** {data.get('range_clipped', False)}",
            cfl,
            f"**line_count:** {data.get('line_count', 0)}",
            f"**truncated:** {data.get('truncated', False)}",
            "",
            "### Assembly",
            "",
            "```asm",
            data.get("asm") or "",
            "```",
        ]
    )


@mcp.tool()
def ida_xrefs_to(ea: str, max_results: int = 300) -> str:
    """
    All cross-references **to** this EA (after resolving to an item head): calls, jumps, data refs, etc.
    ea: hex 0x… or symbol. max_results 0 = unlimited (plugin caps at 10000).
    """
    pre = _check_required_ops("xrefs_to")
    if pre:
        return pre
    extra = {}
    if max_results > 0:
        extra["max_results"] = max_results
    data = _rpc("xrefs_to", str(ea).strip(), **extra)
    if not data.get("ok"):
        _maybe_invalidate_on_unknown_op(data)
        return _fmt_error(data)
    return _fmt_xrefs(data, "target_ea")


@mcp.tool()
def ida_xrefs_from(ea: str, max_results: int = 300) -> str:
    """
    Cross-references **from** this instruction head (outgoing). ea: hex 0x… or symbol. max_results 0 = unlimited (plugin caps at 10000).
    """
    pre = _check_required_ops("xrefs_from")
    if pre:
        return pre
    extra = {}
    if max_results > 0:
        extra["max_results"] = max_results
    data = _rpc("xrefs_from", str(ea).strip(), **extra)
    if not data.get("ok"):
        _maybe_invalidate_on_unknown_op(data)
        return _fmt_error(data)
    return _fmt_xrefs(data, "from_ea")


@mcp.tool()
def ida_search_bytes(pattern: str, max_hits: int = 40) -> str:
    """
    Search IDB for a binary pattern (IDA hex syntax, spaces optional, `??` wildcard bytes). max_hits capped (default 40, max 500).
    """
    pre = _check_required_ops("search_bytes")
    if pre:
        return pre
    data = _rpc("search_bytes", "", pattern=pattern.strip(), max_hits=max_hits)
    if not data.get("ok"):
        _maybe_invalidate_on_unknown_op(data)
        return _fmt_error(data)
    lines = [
        f"**Pattern:** `{data.get('pattern')}`",
        f"**Hits:** {data.get('count', 0)}",
        f"**capped:** {data.get('capped', False)}",
        "",
        "### EA hits",
        "",
    ]
    for h in data.get("hits") or []:
        lines.append(f"- `{h}`")
    if not data.get("hits"):
        lines.append("_(none)_")
    return "\n".join(lines)


@mcp.tool()
def ida_search_text(substring: str, max_hits: int = 40) -> str:
    """
    Search disassembly listing text for a substring (case per IDA rules, not regex). max_hits capped (default 40, max 500).
    """
    pre = _check_required_ops("search_text")
    if pre:
        return pre
    data = _rpc("search_text", "", substring=substring.strip(), max_hits=max_hits)
    if not data.get("ok"):
        _maybe_invalidate_on_unknown_op(data)
        return _fmt_error(data)
    lines = [
        f"**Substring:** `{data.get('substring')}`",
        f"**Hits:** {data.get('count', 0)}",
        f"**capped:** {data.get('capped', False)}",
        "",
        "### Matches",
        "",
    ]
    for row in data.get("hits") or []:
        ea = row.get("ea")
        ln = row.get("line") or ""
        lines.append(f"- `{ea}` — `{ln}`")
    if not data.get("hits"):
        lines.append("_(none)_")
    return "\n".join(lines)


@mcp.tool()
def ida_get_struct(struct_name: str) -> str:
    """
    Export a structure layout: member offsets (decimal + hex) and type strings when IDA has tinfo. Read-only.
    """
    pre = _check_required_ops("get_struct")
    if pre:
        return pre
    data = _rpc("get_struct", "", name=struct_name.strip())
    if not data.get("ok"):
        _maybe_invalidate_on_unknown_op(data)
        return _fmt_error(data)
    slim = {
        "name": data.get("name"),
        "size": data.get("size"),
        "member_count": data.get("member_count"),
        "members": data.get("members"),
    }
    lines = [
        f"**Struct:** `{data.get('name')}`",
        f"**size:** {data.get('size')}",
        f"**member_count:** {data.get('member_count')}",
        "",
        "### members (JSON)",
        "",
        "```json",
        json.dumps(slim, indent=2, sort_keys=True),
        "```",
    ]
    return "\n".join(lines)


def main():
    os.chdir(Path(__file__).resolve().parent)
    # MCP Python SDK: FastMCP.run() starts stdio transport for Cursor / Claude Code.
    run = getattr(mcp, "run", None)
    if run is None:
        raise RuntimeError("FastMCP has no run()")
    import inspect

    try:
        sig = inspect.signature(run)
    except (TypeError, ValueError):
        sig = None
    if sig and "transport" in sig.parameters:
        run(transport="stdio")
    else:
        run()


if __name__ == "__main__":
    main()
