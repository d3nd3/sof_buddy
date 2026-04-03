# IDA MCP bridge (SoF Buddy)

Two parts:

1. **IDA plugin** (`sof_buddy_ida_mcp.py`) — runs inside IDA, binds **127.0.0.1** only, exposes **POST `/rpc`** with JSON `{ "op": "…", "arg": "…" }`. All IDC access runs on IDA’s main thread via `ida_kernwin.execute_sync`.
2. **Cursor MCP server** (`cursor_mcp_server.py`) — stdio MCP process that forwards tool calls to the HTTP bridge.

Responses prefer **long demangled** names (`get_long_name` + `INF_LONG_DN` + `DQT_FULL`), matching the approach in `ida_dump_main.py`.

## IDA: install the plugin

Copy or symlink into IDA’s `plugins` directory, for example:

```bash
ln -sf "$(pwd)/sof_buddy_ida_mcp.py" /opt/idapro-9.0/plugins/sof_buddy_ida_mcp.py
```

Restart IDA, open your **`.i64` / `.idb`**. The Output window should show:

```text
[SoF Buddy IDA MCP] listening on http://127.0.0.1:31337/rpc (POST JSON)
```

Optional environment (set before starting IDA):

| Variable       | Default   | Meaning        |
|----------------|-----------|----------------|
| `IDA_MCP_PORT` | `31337`   | TCP port       |

## HTTP API (for debugging)

`POST http://127.0.0.1:31337/rpc` with JSON body:

| `op`       | `arg`              | Optional | Result |
|------------|--------------------|----------|--------|
| `ping`     | ignored            |          | `idb`, `input_file`, **`plugin_version`**, **`supported_ops`** (canonical op names) |
| `resolve`  | symbol or `0x…`    |          | `function`; or `ok:false` + up to **12** `suggestions[]`. **v4+:** may set **`auto_resolved`** + **`hint`** when a single unambiguous substring/exact match is picked (same pool as `find`). |
| `find`     | substring (≥2 chars) | `limit` (1–100, default 15) | `matches[]`, `count`, **`elapsed_ms`**, **`total_functions`** (plugin **v3+**); validation errors include **`error_code`**: `pattern_too_short` |
| `callers`  | symbol or `0x…`    | `max_results` (0 = all, max 50000) | `target`, `callers[]`, `count`, `returned`, `truncated`; **`auto_resolved`** (v4+); or `suggestions[]` on failed resolve |
| `callees`  | symbol or `0x…`    | `max_results` (0 = all, max 50000) | `target`, `callees[]`, `count`, `returned`, `truncated`; **`auto_resolved`** (v4+); or `suggestions[]` on failed resolve |
| `disasm`   | symbol or `0x…`    | `max_lines` (1–100000, default 2000); **v4+:** `start_ea` / `start_from_ea`, `skip_instructions` / `skip_lines` | `function`, `asm`, `line_count`, `truncated`, **`disasm_start_ea`**, **`skipped_instruction_heads`**, **`disasm_plain_text`: true**; **`error_code`**: `start_ea_not_in_function` |
| `disasm_linear` | `ea_start` (body or `arg`) | `ea_end`, `max_lines` (1–50000, default 400) | Linear listing from **any** EA (default bound = segment end, span clipped); `containing_function`, `asm`, **`range_clipped`** |
| `decompile` | symbol or `0x…`   | **v4+:** `fallback_asm` / `include_asm_on_failure`, `fallback_asm_max_lines` / `fallback_max_lines` | `function`, `pseudocode`; failures: **`error_code`**, **`decompile_detail`**, optional **`fallback_asm`** |
| `xrefs_to` | `ea` / `address` / `arg` | `max_results` (0 = unlimited, cap 10000) | `target_ea`, `xrefs[]` (`from_ea`, `to_ea`, `type`, `from_function`, `to_function`), `truncated` |
| `xrefs_from` | same | same | `from_ea`, `xrefs[]`, `truncated` |
| `search_bytes` | `pattern` (hex + `??`) | `max_hits` (max 500, default 40) | `hits[]` (hex strings), `capped` |
| `search_text` | `substring` | `max_hits` (max 500) | `hits[]` (`ea`, `line`), `capped` |
| `get_struct` | `name` / `struct` | | `members[]` (`offset`, `offset_hex`, `name`, `type`), `size`, `member_count` |

Example find: `{"op":"find","arg":"GroundContact","limit":20}`.

Example disasm: `{"op":"disasm","arg":"0x401000","max_lines":500}`.

Example decompile: `{"op":"decompile","arg":"main"}`.

`GET /health` returns the same JSON as **`POST /rpc`** with **`op: ping`** from the IDA plugin (`ok`, `idb`, `input_file`, and when v2+ **`plugin_version`**, **`supported_ops`**). There is no separate MCP-only layer on HTTP.

The **stdio MCP `ida_ping` tool** adds **`bridge_capabilities`** (booleans for `disasm`, `decompile`, `max_results_honored`), sets **`stdio_mcp_enrichment`: true**, and when the plugin is legacy forces **`plugin_version`: null** and **`supported_ops`: []** so agents can branch without parsing markdown warnings. Pass **`include_paths=false`** to redact **`idb`** / **`input_file`** to `"<redacted>"` in the displayed JSON (shared logs).

**`max_results` semantics differ by tool:** **`find`** uses **`limit`** (always capped at **100**). **`callers`** / **`callees`** use **`max_results`** where **`0` means unlimited** (up to 50000 when capped).

**`decompile` `error_code` values:** `no_hexrays`, `hexrays_init_failed`, `resolve_failed`, `not_in_function`, `decompile_failed`, `decompile_none`, **`decompile_thunk`** (v4+), **`hexrays_decompilation_failure`** (v4+, typed Hex-Rays failure when available). Failures may include **`decompile_detail`** (reason, hints, thunk flag) and **`fallback_asm`** when requested.

Unknown `op` responses from a current plugin also include `plugin_version` and `supported_ops` so clients can see what the running IDA plugin supports.

## Troubleshooting

- **`unknown op: 'disasm'` or `unknown op: 'decompile'`** — The copy of `sof_buddy_ida_mcp.py` loaded inside IDA is older than the stdio MCP bridge in this repo. Install the current file into IDA’s `plugins/` directory (symlink as in [IDA: install the plugin](#ida-install-the-plugin)), restart IDA, and call **`ida_ping`** to confirm `supported_ops` lists `disasm` and `decompile`.
- **`ida_decompile_function` fails with Hex-Rays errors** — Separate issue: Hex-Rays not installed, not licensed, or the function does not decompile. The bridge distinguishes that from an unknown op.

## Cursor: MCP server (host Python)

Use a normal OS Python — **not** IDA’s embedded Python.

### One-liner (recommended)

From the **repo root**:

```bash
./ida_plugin_mcp/setup_mcp.sh install
```

### Hook up Cursor (pick one path)

**Path A — this repo is your Cursor workspace (simplest)**  

The repo already contains **`.cursor/mcp.json`**: it runs  
`${workspaceFolder}/.venv-mcp/bin/python` on `ida_plugin_mcp/cursor_mcp_server.py`.  
After `setup_mcp.sh install`, **reload MCP** so Cursor picks up the venv:

- Command Palette → **“Developer: Reload Window”**, or restart Cursor.

Then ensure **IDA is running** with the plugin and an IDB open, and try a tool such as **`ida_ping`** from the agent (with MCP tools enabled for that chat).

You do **not** need `print-config` for Path A unless you want to copy absolute paths for a different setup.

**Path B — you use `print-config` or edit MCP JSON by hand**  

1. Run `./ida_plugin_mcp/setup_mcp.sh print-config`. It prints one server block, e.g. `"ida-sof-buddy": { ... }`.
2. Open Cursor’s MCP configuration JSON (where all servers live). Typical places:
   - **Project:** `.cursor/mcp.json` at your workspace root (same folder as this file’s parent `sof_buddy`), or  
   - **User-wide:** Cursor Settings → **MCP** → open/edit the config file Cursor points to (wording varies by version).
3. **Merge** that block into the existing **`mcpServers`** object. You must keep valid JSON: one comma between this entry and any neighbors.

   Example — you already have another server:

   ```json
   {
     "mcpServers": {
       "some-other-server": { "command": "…", "args": [] },
       "ida-sof-buddy": {
         "command": "/ABS/PATH/sof_buddy/.venv-mcp/bin/python",
         "args": ["/ABS/PATH/sof_buddy/ida_plugin_mcp/cursor_mcp_server.py"],
         "env": { "IDA_MCP_BASE": "http://127.0.0.1:31337" }
       }
     }
   }
   ```

   Replace `/ABS/PATH/...` with the paths **`print-config`** printed (or use Path A’s `${workspaceFolder}/...` style if your Cursor build expands it).

4. **Save** the file, then **reload the window** or restart Cursor so the new server starts.
5. **IDA** must be running with **`[SoF Buddy IDA MCP] listening…`** before tools like `ida_get_callers` return data; otherwise you’ll get connection errors from the bridge.

### Test the stdio server in a terminal (optional)

IDA must be running with the plugin loaded:

```bash
./ida_plugin_mcp/setup_mcp.sh run
```

It will block on stdio; use Ctrl+C to stop. This is only for sanity-checking; normal use is Cursor starting that process via MCP.

### If `python3 -m pip` says `No module named pip` (Debian / Ubuntu)

System Python often has no `pip`. Either:

**A. Distro package (needs sudo):**

```bash
sudo apt install python3-pip
python3 -m pip install -r ida_plugin_mcp/requirements-mcp.txt
```

**B. Virtualenv (pip bundled; no `python3-pip` required):**

```bash
cd /path/to/sof_buddy
python3 -m venv .venv-mcp
./.venv-mcp/bin/pip install -r ida_plugin_mcp/requirements-mcp.txt
```

Then point Cursor’s MCP `command` at **`${workspaceFolder}/.venv-mcp/bin/python`** (and the same `args` as before) instead of `python3`.

### Otherwise

```bash
python3 -m pip install -r ida_plugin_mcp/requirements-mcp.txt
```

Optional:

| Variable        | Default                      |
|-----------------|------------------------------|
| `IDA_MCP_BASE`  | `http://127.0.0.1:31337`     |
| `IDA_MCP_TIMEOUT` | `120` (seconds)          |

### Reference: same server as in `.cursor/mcp.json`

After `setup_mcp.sh install`, prefer the **venv** interpreter (matches the script):

```json
"ida-sof-buddy": {
  "command": "${workspaceFolder}/.venv-mcp/bin/python",
  "args": ["${workspaceFolder}/ida_plugin_mcp/cursor_mcp_server.py"],
  "env": {
    "IDA_MCP_BASE": "http://127.0.0.1:31337"
  }
}
```

If `${workspaceFolder}` is not expanded by your Cursor build, run **`print-config`** and paste the absolute paths from there instead.

### MCP tools

Calling them in Cursor is the same as any other MCP tool: enable the server, then the model can invoke them by name with arguments (no exact mangled name required for discovery).

**Return shape:** Every tool returns a **single string** (markdown: headings, lists, fenced code). There is no structured JSON tool output from the MCP layer. **`ida_ping`** wraps the RPC payload in a **`json`** fenced block (pretty-printed). Reference descriptors for schema-aware clients: **`ida_plugin_mcp/mcp_tool_descriptors/`** (optional; Cursor may regenerate its own copies when the MCP server starts).

- **`ida_ping(include_paths=true)`** — connectivity; JSON includes **`bridge_capabilities`** and **`plugin_version` / `supported_ops`**. Set **`include_paths=false`** to redact paths in the JSON block. Canonical upgrade **Warning** only here when legacy.
- **`ida_resolve_function(symbol_or_address)`** — **hex EA** is the most stable. **v4+** may **auto-resolve** a unique match (same rules as find). If unresolved, up to **12** **nearest matches**; use **`ida_find_functions`** for a wider search.
- **`ida_find_functions(pattern, max_results=15)`** — **substring** match, **case-insensitive**, **not regex**; **`max_results`** is a **hard cap** (max **100**). Plugin v3+ adds **`elapsed_ms`** and **`total_functions`** in the RPC (shown when present). Not the same semantics as **`ida_get_callers`** (see below).
- **`ida_get_callers(symbol_or_address, max_results=80)`** — direct **call** xrefs only. **`main`** and other **entry points** may show **0 callers** if the CRT startup is not modeled as a call xref. Default cap; **`max_results=0`** = **unlimited** (distinct from **`find`**).
- **`ida_get_callees(symbol_or_address, max_results=80)`** — same **`max_results`** rules as **`ida_get_callers`**.
- **`ida_get_function_asm(symbol_or_address, max_lines=2000, start_ea=None, skip_instructions=0)`** — linear **disassembly**; **v4+** **`start_ea`** begins at the head inside the function; **`skip_instructions`** skips extra heads after that anchor. Plain text listing. Legacy plugin → **fail-fast** (see **`ida_ping`**).
- **`ida_get_disasm_linear(ea_start, ea_end=None, max_lines=400)`** — listing from **any** EA (not function entry).
- **`ida_xrefs_to(ea, max_results=300)`** / **`ida_xrefs_from(ea, max_results=300)`** — xrefs **to** / **from** an EA (all xref kinds, not only calls).
- **`ida_search_bytes(pattern, max_hits=40)`** / **`ida_search_text(substring, max_hits=40)`** — capped searches.
- **`ida_get_struct(struct_name)`** — read-only **member offsets** + type strings (JSON block).
- **`ida_decompile_function(symbol_or_address, fallback_asm=True, fallback_max_lines=128)`** — **Hex-Rays** (F5). **v4+** failures may include **`decompile_detail`** JSON and **`fallback_asm`**. Same legacy **fail-fast** as asm when needed.

**Tip:** For ambiguous names, call `ida_find_functions` first, then `ida_get_callers` / `ida_get_callees` with the chosen **mangled** name or **ea_hex** from the list.

## Security

The plugin listens on **localhost only**. Do not expose this port to a network.

**Paths:** RPC **`ping`** and MCP **`ida_ping`** include full local **`idb`** and **`input_file`** paths. For screenshots or shared agent logs, use **`ida_ping(include_paths=false)`** so the stdio bridge redacts those strings in the JSON block (the MCP process still connects to IDA over localhost the same way).
