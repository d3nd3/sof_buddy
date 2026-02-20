#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
UPDATE_DIR="${ROOT_DIR}/sof_buddy/update"

ZIP_PATH="${1:-}"
if [[ -n "${ZIP_PATH}" ]]; then
    if [[ ! -f "${ZIP_PATH}" && -f "${ROOT_DIR}/${ZIP_PATH}" ]]; then
        ZIP_PATH="${ROOT_DIR}/${ZIP_PATH}"
    fi
else
    if [[ ! -d "${UPDATE_DIR}" ]]; then
        echo "Update folder not found: ${UPDATE_DIR}"
        echo "First run: sofbuddy_update download"
        exit 1
    fi
    ZIP_PATH="$(find "${UPDATE_DIR}" -maxdepth 1 -type f -name '*.zip' -printf '%T@ %p\n' | sort -nr | head -n1 | cut -d' ' -f2-)"
fi

if [[ -z "${ZIP_PATH}" || ! -f "${ZIP_PATH}" ]]; then
    echo "No update zip found."
    echo "First run: sofbuddy_update download"
    exit 1
fi

echo "Applying update from:"
echo "  ${ZIP_PATH}"
echo
echo "Make sure Soldier of Fortune is fully closed before continuing."
echo

if command -v unzip >/dev/null 2>&1; then
    unzip -o "${ZIP_PATH}" -d "${ROOT_DIR}"
elif command -v 7z >/dev/null 2>&1; then
    7z x -y "${ZIP_PATH}" "-o${ROOT_DIR}"
elif command -v bsdtar >/dev/null 2>&1; then
    bsdtar -xf "${ZIP_PATH}" -C "${ROOT_DIR}"
else
    echo "No extractor found. Install one of: unzip, 7z, bsdtar"
    exit 1
fi

echo
echo "Update extraction complete."
echo "Deleting used update zip..."
if rm -f -- "${ZIP_PATH}"; then
    echo "Deleted: ${ZIP_PATH}"
else
    echo "Warning: could not delete ${ZIP_PATH}"
fi
echo
read -r -p "Start with fresh settings? [Y/n]: " ans
if [[ ! "${ans,,}" =~ ^n(o)?$ ]]; then
    echo "Searching for old configuration to prune..."
    find "${ROOT_DIR}" -type f -name "sofbuddy.cfg" -print -delete 2>/dev/null || true
fi
echo
echo "You can now launch SoF normally."
