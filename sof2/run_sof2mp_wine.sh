#!/bin/bash
# SoF2MP must run with CWD = game folder (base/, Menusx86.dll, etc.).
# If Wine logs "experimental wow64" + "stack overflow" before any game UI (even SoF2MP.exe.bak),
# the prefix is 64-bit WoW64 — use Wine 10.15+ or a fresh WINEARCH=win32 prefix + reinstall.
set -euo pipefail
GAME="${SOF2_DIR:-${HOME}/.local/share/wineprefixes/sof2/drive_c/Program Files (x86)/Soldier of Fortune II - Double Helix}"
export WINEPREFIX="${WINEPREFIX:-${HOME}/.local/share/wineprefixes/sof2}"
cd "$GAME"
exec wine ./SoF2MP.exe "$@"
