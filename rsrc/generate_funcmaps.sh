#!/usr/bin/env bash
set -euo pipefail

# generate_funcmaps.sh
# Builds the PE function map generator and emits JSON maps for SoF modules.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

OUT_DIR_DEFAULT="$ROOT_DIR/rsrc/funcmaps"

SOF_ROOT="${SOF_INSTALL_DIR:-}"
if [[ -z "$SOF_ROOT" && -n "${WINEPREFIX:-}" ]]; then
  SOF_ROOT="$WINEPREFIX/drive_c/Program Files/Soldier of Fortune"
fi
SOF_EXE_DEFAULT="${SOF_ROOT:+$SOF_ROOT/SoF.exe}"
REF_GL_DLL_DEFAULT="${SOF_ROOT:+$SOF_ROOT/ref_gl.dll}"
PLAYER_DLL_DEFAULT="${SOF_ROOT:+$SOF_ROOT/Base/player.dll}"
GAMEX86_DLL_DEFAULT="${SOF_ROOT:+$SOF_ROOT/Base/gamex86.dll}"
SPCL_DLL_DEFAULT="${SOF_ROOT:+$SOF_ROOT/spcl.dll}"

usage() {
    cat <<EOF
Usage: $(basename "$0") [out_dir] [SoF.exe] [ref_gl.dll] [player.dll] [gamex86.dll] [spcl.dll]

Set SOF_INSTALL_DIR (game folder with SoF.exe) or pass paths as arguments.

Examples:
  SOF_INSTALL_DIR=/path/to/SoF $(basename "$0")
  $(basename "$0") "$ROOT_DIR/rsrc/funcmaps" /path/to/SoF.exe /path/to/ref_gl.dll ...
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
    usage
    exit 0
fi

OUT_DIR="${1:-$OUT_DIR_DEFAULT}"
SOF_EXE="${2:-$SOF_EXE_DEFAULT}"
REF_GL_DLL="${3:-$REF_GL_DLL_DEFAULT}"
PLAYER_DLL="${4:-$PLAYER_DLL_DEFAULT}"
GAMEX86_DLL="${5:-$GAMEX86_DLL_DEFAULT}"
SPCL_DLL="${6:-$SPCL_DLL_DEFAULT}"

for spec in \
  "SoF.exe:$SOF_EXE" \
  "ref_gl.dll:$REF_GL_DLL" \
  "player.dll:$PLAYER_DLL" \
  "gamex86.dll:$GAMEX86_DLL" \
  "spcl.dll:$SPCL_DLL"; do
  label="${spec%%:*}"
  path="${spec#*:}"
  if [[ -z "$path" || ! -f "$path" ]]; then
    echo "Error: missing $label (set SOF_INSTALL_DIR or pass paths)" >&2
    exit 1
  fi
done

echo "[funcmaps] Building generator..."
make -C "$ROOT_DIR" --no-print-directory funcmap-gen

GEN_BIN="$ROOT_DIR/tools/pe_funcmap_gen"
if [[ ! -x "$GEN_BIN" ]]; then
    echo "Error: generator not found at $GEN_BIN" >&2
    exit 1
fi

mkdir -p "$OUT_DIR"

echo "[funcmaps] Writing maps to: $OUT_DIR"
echo "[funcmaps] Images:"
echo "  - $SOF_EXE"
echo "  - $REF_GL_DLL"
echo "  - $PLAYER_DLL"
echo "  - $GAMEX86_DLL"
echo "  - $SPCL_DLL"

"$GEN_BIN" "$OUT_DIR" "$SOF_EXE" "$REF_GL_DLL" "$PLAYER_DLL" "$GAMEX86_DLL" "$SPCL_DLL"

echo "[funcmaps] Done. Generated files:"
ls -1 "$OUT_DIR" | sed 's/^/  - /'
