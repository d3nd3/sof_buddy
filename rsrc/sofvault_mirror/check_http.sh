#!/usr/bin/env bash
# Verify that the XP updater endpoints are reachable over plain HTTP.
#
# Windows XP WinHTTP cannot negotiate modern TLS/SNI.  The live SofVault host
# only serves /sof_buddy/releases/files/ (plus the legacy root latest.json and
# latest files) over HTTP, so the updater reads its manifests from files/.
# Root list.json and by_tag/ are checked as warnings for older clients.
#
#   ./check_http.sh
#   SOFVAULT_BASE=http://127.0.0.1/sof_buddy/releases ./check_http.sh
#
# Exit code 0 means every XP-critical endpoint is HTTP-clean.

set -euo pipefail

BASE="${SOFVAULT_BASE:-http://sofvault.org/sof_buddy/releases}"
BASE="${BASE%/}"
MAX_TIME="${SOFVAULT_CHECK_TIMEOUT:-15}"
failed=0

check_path() {
  local path="$1"
  local required="${2:-1}"
  local url="$BASE/$path"
  local code redirect
  code="$(curl -sS -I -o /dev/null --max-time "$MAX_TIME" \
    -w '%{http_code} %{redirect_url}' "$url" 2>/dev/null || true)"
  redirect="${code#* }"
  code="${code%% *}"

  if [[ "$code" == "200" && -z "$redirect" ]]; then
    printf 'OK    %s\n' "$url"
    return 0
  fi

  if [[ "$required" == "1" ]]; then
    if [[ -n "$redirect" ]]; then
      printf 'FAIL  %s (HTTP %s -> %s)\n' "$url" "$code" "$redirect"
    else
      printf 'FAIL  %s (HTTP %s)\n' "$url" "$code"
    fi
    failed=1
  else
    if [[ -n "$redirect" ]]; then
      printf 'WARN  %s (HTTP %s -> %s; older clients only)\n' "$url" "$code" "$redirect"
    else
      printf 'WARN  %s (HTTP %s; older clients only)\n' "$url" "$code"
    fi
  fi
}

# XP-critical endpoints.
check_path "latest.json"
check_path "latest"
check_path "files/latest.json"
check_path "files/stable.json"
check_path "files/list.json"
check_path "files/release_windows_xp.zip"

LATEST_JSON="$(curl -fsS --max-time "$MAX_TIME" "$BASE/latest.json" 2>/dev/null || true)"
TAG="$(printf '%s' "$LATEST_JSON" | jq -r '.tag_name // empty' 2>/dev/null || true)"
UNSTABLE_TAG="$(printf '%s' "$LATEST_JSON" | jq -r '.unstable_tag // empty' 2>/dev/null || true)"

if [[ -n "$TAG" ]]; then
  check_path "files/by_tag/$TAG.json"
else
  printf 'WARN  could not read tag_name from %s/latest.json\n' "$BASE"
fi

if [[ -n "$UNSTABLE_TAG" ]]; then
  check_path "files/unstable.json"
  check_path "files/release_windows_xp_unstable.zip"
else
  printf 'SKIP  unstable channel not published\n'
fi

# Legacy root manifests: older clients use these, and they should be fixed on
# the server when possible, but current XP builds no longer require them.
check_path "list.json" 0
if [[ -n "$TAG" ]]; then
  check_path "by_tag/$TAG.json" 0
fi

if [[ "$failed" -ne 0 ]]; then
  printf '\nXP-critical endpoints must be served over HTTP. Keep the\n'
  printf '/sof_buddy/releases/files/ nginx exception before the global redirect;\n'
  printf 'see README.md and re-run sync_from_github.sh to publish files/ manifests.\n'
  exit 1
fi

printf '\nAll XP-critical updater endpoints are HTTP-clean.\n'
