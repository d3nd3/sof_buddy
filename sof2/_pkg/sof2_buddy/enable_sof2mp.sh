#!/bin/bash
set -euo pipefail
DIR="$(cd "$(dirname "$0")" && pwd)"
GAME="$(cd "$DIR/.." && pwd)"
EXE="${1:-$GAME/SoF2MP.exe}"
DLL_NAME="sof2bud.dll"
OFFSET=0x157A30
DEV_DLL="$(cd "$DIR/../.." 2>/dev/null && pwd)/bin/sof2bud.dll"
if [[ ! -f "$EXE" ]]; then echo "SoF2MP.exe not found: $EXE" >&2; exit 1; fi
dll_in_game="$GAME/$DLL_NAME"
if [[ -f "$dll_in_game" ]]; then true
elif [[ -f "$DIR/$DLL_NAME" ]]; then cp -f "$DIR/$DLL_NAME" "$dll_in_game"
elif [[ -f "$DEV_DLL" ]]; then cp -f "$DEV_DLL" "$dll_in_game"
else echo "sof2bud.dll missing (run deploy_to_sof2.sh or make -C sof2)" >&2; exit 1; fi
backup="${EXE}.bak"
[[ -f "$backup" ]] || cp -f "$EXE" "$backup"
printf '%s' "$DLL_NAME" | dd of="$EXE" bs=1 seek=$((OFFSET)) conv=notrunc status=none
echo "Patched import to $DLL_NAME (backup: $backup)"
