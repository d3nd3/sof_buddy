#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
if [[ "${SOFBUDDY_UPDATE_SH_ROOT:-}" == /* ]]; then
    ROOT_DIR="$(cd -- "${SOFBUDDY_UPDATE_SH_ROOT}" && pwd)"
else
    ROOT_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
fi
UPDATE_DIR="${ROOT_DIR}/sof_buddy/update"

load_install_context() {
    local file="${UPDATE_DIR}/install_context.env" line key value
    [[ -f "${file}" ]] || return 0
    while IFS= read -r line || [[ -n "${line}" ]]; do
        [[ -z "${line}" || "${line}" == \#* ]] && continue
        key="${line%%=*}"
        value="${line#*=}"
        [[ "${key}" =~ ^SOFBUDDY_[A-Z0-9_]+$ ]] || continue
        [[ -z "${!key:-}" ]] || continue
        printf -v "${key}" '%s' "${value}"
        export "${key}"
    done < "${file}"
}

# Run from a temp copy: extraction replaces update_from_zip.sh, and bash reads
# scripts from the file while executing. Replacing it mid-run corrupts the
# remaining commands.
if [[ -z "${SOFBUDDY_UPDATE_SH_REEXEC:-}" ]]; then
    export SOFBUDDY_UPDATE_SH_ROOT="${ROOT_DIR}"
    export SOFBUDDY_UPDATE_SH_REEXEC=1
    reexec_dir="${TMPDIR:-/tmp}"
    if [[ ! -d "${reexec_dir}" || ! -w "${reexec_dir}" ]]; then
        reexec_dir="${UPDATE_DIR}"
        mkdir -p -- "${reexec_dir}"
    fi
    if reexec_script="$(mktemp "${reexec_dir%/}/sofbuddy_update.XXXXXX.sh")"; then
        cp -f -- "${BASH_SOURCE[0]}" "${reexec_script}"
        set +e
        bash "${reexec_script}" "$@"
        reexec_rc=$?
        set -e
        rm -f -- "${reexec_script}"
        exit "${reexec_rc}"
    fi
fi

load_install_context

delegated=0
zip_args=()
while [[ $# -gt 0 ]]; do
    case "$1" in
        --reset-features) SOFBUDDY_UPDATER_RESET_FEATURES=1; export SOFBUDDY_UPDATER_RESET_FEATURES; shift ;;
        --fresh-settings) SOFBUDDY_UPDATER_FRESH=1; export SOFBUDDY_UPDATER_FRESH; shift ;;
        --delegated) delegated=1; shift ;;
        --*) shift ;;
        *) zip_args+=("$1"); shift ;;
    esac
done

is_universal=0
if [[ -n "${SOFBUDDY_IS_UNIVERSAL:-}" ]]; then
    if [[ "${SOFBUDDY_IS_UNIVERSAL}" == "1" ]]; then
        is_universal=1
    fi
elif [[ -f "${ROOT_DIR}/sof_buddy/flavor.txt" ]]; then
    flavor=""
    read -r flavor < "${ROOT_DIR}/sof_buddy/flavor.txt" || true
    if [[ "${flavor,,}" == "universal" ]]; then
        is_universal=1
    fi
elif [[ -f "${ROOT_DIR}/sof_buddy/sof_buddy_update.exe" ]]; then
    # Universal packages have always shipped the graphical updater; older
    # installs predate flavor.txt, so use it as a fallback marker.
    is_universal=1
fi

interactive=1
if [[ "${delegated}" == "1" || "${SOFBUDDY_UPDATER_SILENT:-}" == "1" || "${SOFBUDDY_UPDATER_NO_PAUSE:-}" == "1" ]]; then
    interactive=0
fi

ZIP_PATH="${zip_args[0]:-}"
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

FEATURES_FILE="${ROOT_DIR}/sof_buddy/features.cfg"
FEATURES_BACKUP=""
KEEP_FEATURES=0
FEATURES_ABSENT=0
EXTRACT_MARKER="${UPDATE_DIR}/.extract_ok"
mkdir -p "${UPDATE_DIR}"
rm -f -- "${EXTRACT_MARKER}" || true
cleanup_features() {
    local rc=$?
    trap - EXIT
    if [[ -n "${FEATURES_BACKUP}" && -f "${FEATURES_BACKUP}" ]]; then
        if [[ ${rc} -ne 0 || "${KEEP_FEATURES}" == "1" ]]; then
            cp -f -- "${FEATURES_BACKUP}" "${FEATURES_FILE}" || true
        fi
        rm -f -- "${FEATURES_BACKUP}" || true
    elif [[ ${rc} -eq 0 && "${FEATURES_ABSENT}" == "1" ]]; then
        rm -f -- "${FEATURES_FILE}" || true
    fi
    exit "${rc}"
}
trap cleanup_features EXIT

if [[ "${delegated}" == "1" ]]; then
    : # update_from_zip.cmd already prepared the feature options
elif [[ "${is_universal}" == "1" ]]; then
    replace_features="${SOFBUDDY_UPDATER_REPLACE_FEATURES:-}"
    if [[ -z "${replace_features}" ]]; then
        if [[ "${interactive}" == "0" ]]; then
            replace_features=1
        else
            read -r -p "Merge new recommended feature settings with your overrides? [Y/n]: " replace_features || true
            replace_features="${replace_features:-Y}"
        fi
    fi
    if [[ -f "${FEATURES_FILE}" ]]; then
        FEATURES_BACKUP="${UPDATE_DIR}/features.cfg.keep"
        mkdir -p "${UPDATE_DIR}"
        cp -f -- "${FEATURES_FILE}" "${FEATURES_BACKUP}"
    fi
    case "${replace_features,,}" in
        0|n|no|false)
            KEEP_FEATURES=1
            rm -f -- "${ROOT_DIR}/sof_buddy/features.cfg.prev"
            echo "Keeping your current feature settings."
            ;;
        *)
            echo "Merging new recommended feature settings with your overrides."
            if [[ -f "${FEATURES_FILE}" ]]; then
                cp -f -- "${FEATURES_FILE}" "${ROOT_DIR}/sof_buddy/features.cfg.prev"
            fi
            rm -f -- "${FEATURES_FILE}"
            ;;
    esac
else
    # Compile-time channels cannot change features in-game; leave the file alone.
    KEEP_FEATURES=1
    if [[ -f "${FEATURES_FILE}" ]]; then
        FEATURES_BACKUP="${UPDATE_DIR}/features.cfg.keep"
        mkdir -p "${UPDATE_DIR}"
        cp -f -- "${FEATURES_FILE}" "${FEATURES_BACKUP}"
    else
        FEATURES_ABSENT=1
    fi
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

if [[ "${delegated}" == "1" ]]; then
    printf 'ok\n' > "${EXTRACT_MARKER}" || true
fi

echo
echo "Update extraction complete."
echo "Deleting used update zip..."
if rm -f -- "${ZIP_PATH}"; then
    echo "Deleted: ${ZIP_PATH}"
else
    echo "Warning: could not delete ${ZIP_PATH}"
fi

if [[ "${delegated}" == "1" ]]; then
    exit 0
fi

reset_feature_toggles() {
    echo "Searching for saved feature toggles to reset..."
    rm -f -- "${ROOT_DIR}/sof_buddy/features.cfg"
    rm -f -- "${ROOT_DIR}/sof_buddy/features.cfg.prev"
    find "${ROOT_DIR}" -type f -name "sofbuddy_features.cfg" -print -delete 2>/dev/null || true
    while IFS= read -r -d '' cfg; do
        grep_rc=0
        grep -v "_sofbuddy_feature_" "${cfg}" > "${cfg}.tmp" || grep_rc=$?
        if [[ ${grep_rc} -le 1 ]]; then
            mv -f "${cfg}.tmp" "${cfg}"
        else
            rm -f -- "${cfg}.tmp"
        fi
    done < <(find "${ROOT_DIR}" -type f -name "sofbuddy.cfg" -print0 2>/dev/null)
}

if [[ "${SOFBUDDY_UPDATER_FRESH:-}" == "1" ]]; then
    find "${ROOT_DIR}" -type f -name "sofbuddy.cfg" -print -delete 2>/dev/null || true
elif [[ "${interactive}" == "1" ]]; then
    echo
    read -r -p "Start with fresh settings? [y/N]: " ans || true
    if [[ "${ans:-}" =~ ^y(es)?$ ]]; then
        echo "Searching for old configuration to prune..."
        find "${ROOT_DIR}" -type f -name "sofbuddy.cfg" -print -delete 2>/dev/null || true
    fi
fi
if [[ "${is_universal}" == "1" ]]; then
    if [[ "${SOFBUDDY_UPDATER_RESET_FEATURES:-}" == "1" ]]; then
        reset_feature_toggles
    elif [[ "${interactive}" == "1" ]]; then
        echo
        read -r -p "Expecting a new feature? Reset feature toggles [y/N]: " ans || true
        if [[ "${ans:-}" =~ ^y(es)?$ ]]; then
            reset_feature_toggles
        fi
    fi
fi
rm -f -- "${UPDATE_DIR}/pending_update.txt" "${UPDATE_DIR}/install_context.env" 2>/dev/null || true
echo
echo "You can now launch SoF normally."
