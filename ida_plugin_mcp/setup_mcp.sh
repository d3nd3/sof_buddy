#!/usr/bin/env bash
# SoF Buddy — install MCP Python deps into .venv-mcp and optionally run the Cursor bridge.
# Usage:
#   ./ida_plugin_mcp/setup_mcp.sh install    # create venv + pip install mcp (default)
#   ./ida_plugin_mcp/setup_mcp.sh run        # run stdio server (for testing; Cursor usually invokes python itself)
#   ./ida_plugin_mcp/setup_mcp.sh print-config   # echo suggested Cursor MCP command

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
VENV="$ROOT/.venv-mcp"
PY="$VENV/bin/python"
PIP="$VENV/bin/pip"
REQ="$SCRIPT_DIR/requirements-mcp.txt"
SERVER="$SCRIPT_DIR/cursor_mcp_server.py"

die() {
  echo "error: $*" >&2
  exit 1
}

cmd_install() {
  command -v python3 >/dev/null || die "python3 not found"
  if [[ ! -d "$VENV" ]]; then
    echo "Creating venv: $VENV"
    python3 -m venv "$VENV"
  fi
  [[ -x "$PIP" ]] || die "pip missing in venv"
  echo "Upgrading pip & installing MCP SDK..."
  "$PIP" install -U pip
  "$PIP" install -r "$REQ"
  echo ""
  echo "Install OK."
  echo "  Python: $PY"
  echo "  Server: $SERVER"
  echo ""
  echo "Cursor MCP: set command to \"$PY\" and args to [\"$SERVER\"]"
  echo "  (or run: $0 print-config)"
}

cmd_run() {
  [[ -x "$PY" ]] || die "venv missing; run: $0 install"
  [[ -f "$SERVER" ]] || die "missing $SERVER"
  export IDA_MCP_BASE="${IDA_MCP_BASE:-http://127.0.0.1:31337}"
  exec "$PY" "$SERVER"
}

cmd_print_config() {
  [[ -x "$PY" ]] || die "venv missing; run: $0 install"
  cat <<EOF
Add to Cursor MCP (merge with your existing mcpServers):

  "ida-sof-buddy": {
    "command": "$PY",
    "args": ["$SERVER"],
    "env": {
      "IDA_MCP_BASE": "http://127.0.0.1:31337"
    }
  }

EOF
}

usage() {
  echo "usage: $0 {install|run|print-config}"
  echo "  install       Create $ROOT/.venv-mcp and pip install mcp (default)"
  echo "  run           Start the stdio MCP server (IDA plugin must be listening)"
  echo "  print-config  Print JSON snippet for Cursor MCP command/args"
}

main() {
  case "${1:-install}" in
    install) cmd_install ;;
    run) cmd_run ;;
    print-config) cmd_print_config ;;
    -h|--help|help) usage ;;
    *) usage; exit 1 ;;
  esac
}

main "$@"
