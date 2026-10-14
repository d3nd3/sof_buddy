#!/bin/bash
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
bash "$DIR/patch_sof_binary.sh" "$DIR/../SoF.exe" WSOCK32.dll
