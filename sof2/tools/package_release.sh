#!/usr/bin/env bash
# Build zips for CI/local release (run from repo root or sof2/).
set -euo pipefail
SOF2="$(cd "$(dirname "$0")/.." && pwd)"
cd "$SOF2"
make
rm -rf _pkg && mkdir -p _pkg/sof2_buddy
cp -f bin/sof2bud.dll _pkg/sof2bud.dll
cp -f bin/sof2bud.dll _pkg/sof2_buddy/sof2bud.dll
cp -f sof2_buddy/patches.json _pkg/sof2_buddy/
cp -f sof2_buddy/enable_sof2mp.sh _pkg/sof2_buddy/
cp -f rsrc/win_scripts/patch_sof2mp.ps1 rsrc/win_scripts/enable_sof2mp.cmd _pkg/sof2_buddy/
(
	cd _pkg
	zip -r ../release_windows.zip sof2bud.dll sof2_buddy/
)
cp -f release_windows.zip sof2bud_installer_payload.zip
echo "Created $SOF2/release_windows.zip"
