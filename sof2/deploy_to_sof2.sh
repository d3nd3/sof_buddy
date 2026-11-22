#!/bin/bash
# Copy sof2bud into $SOF2_DIR and patch SoF2MP.exe to load it.
# Requires SOF2_DIR in the environment, or pass the game folder as the first argument.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")" && pwd)"

if [[ -n "${SOF2_DIR:-}" ]]; then
	:
elif [[ -n "${1:-}" ]]; then
	SOF2_DIR="$1"
elif [[ -d "${HOME}/.local/share/wineprefixes/sof2/drive_c/Program Files (x86)/Soldier of Fortune II - Double Helix" ]]; then
	SOF2_DIR="${HOME}/.local/share/wineprefixes/sof2/drive_c/Program Files (x86)/Soldier of Fortune II - Double Helix"
else
	echo "Set SOF2_DIR to your SoF2 install root, or run: $0 /path/to/game" >&2
	exit 1
fi

if [[ ! -d "${SOF2_DIR}" ]]; then
	echo "SOF2_DIR is not a directory: ${SOF2_DIR}" >&2
	exit 1
fi

if [[ ! -f "${REPO_ROOT}/bin/sof2bud.dll" ]]; then
	echo "Building sof2bud.dll..."
	make -C "${REPO_ROOT}"
fi

DEST_BUDDY="${SOF2_DIR}/sof2_buddy"
MENU_SRC="${REPO_ROOT}/menus/sof_buddy"
MENU_DEST="${SOF2_DIR}/menus/sof_buddy"
MENUS_DIR="${SOF2_DIR}/menus"
mkdir -p "${DEST_BUDDY}" "${MENUS_DIR}"
THEREST_PK3="${SOF2_DIR}/base/therest.pk3"
if [[ -f "${THEREST_PK3}" ]]; then
	if [[ ! -s "${MENUS_DIR}/defines.rmf" ]]; then
		unzip -p "${THEREST_PK3}" menus/defines.rmf > "${MENUS_DIR}/defines.rmf"
	fi
fi
if [[ -d "${MENU_SRC}" ]]; then
	mkdir -p "${MENU_DEST}/tabs"
	cp -f "${MENU_SRC}"/*.rmf "${MENU_DEST}/"
	cp -f "${MENU_SRC}"/tabs/*.rmf "${MENU_DEST}/tabs/"
fi
# Import hijack resolves DLL next to SoF2MP.exe (same as sof_buddy.dll on SoF1).
cp -f "${REPO_ROOT}/bin/sof2bud.dll" "${SOF2_DIR}/sof2bud.dll"
cp -f "${REPO_ROOT}/bin/sof2bud.dll" "${DEST_BUDDY}/sof2bud.dll"
cp -f "${REPO_ROOT}/sof2_buddy/enable_sof2mp.sh" "${DEST_BUDDY}/enable_sof2mp.sh"
cp -f "${REPO_ROOT}/sof2_buddy/patches.json" "${DEST_BUDDY}/patches.json"
cp -f "${REPO_ROOT}/rsrc/win_scripts/"* "${DEST_BUDDY}/" 2>/dev/null || true
chmod +x "${DEST_BUDDY}/enable_sof2mp.sh"

EXE="${SOF2_DIR}/SoF2MP.exe"
if [[ ! -f "${EXE}" ]]; then
	echo "Warning: ${EXE} not found — files copied to ${DEST_BUDDY} only." >&2
	exit 0
fi

bash "${DEST_BUDDY}/enable_sof2mp.sh" "${EXE}"
echo "Deployed to ${SOF2_DIR}"
echo "Wine: run from game dir — ${REPO_ROOT}/run_sof2mp_wine.sh"
