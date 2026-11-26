#!/usr/bin/env bash
# Verify that the XP updater endpoints are reachable over plain HTTP.
#
# Windows XP WinHTTP cannot negotiate modern TLS/SNI, so every URL the XP
# updater reads directly must return 200 on http:// without a redirect to
# https://.  Run this on the mirror host (or anywhere) after nginx changes.
#
#   ./check_http.sh
#   SOFVAULT_BASE=http://127.0.0.1/sof_buddy/releases ./check_http.sh
#
# Exit code 0 means every endpoint is HTTP-clean.

set -euo pipefail

BASE="${SOFVAULT_BASE:-http://sofvault.org/sof_buddy/releases}"
BASE="${BASE%/}"
MAX_TIME="${SOFVAULT_CHECK_TIMEOUT:-15}"
failed=0

check_path() {
  local path="$1"
  local url="$BASE/$path"
  local code redirect
  code="$(curl -sS -I -o /dev/null --max-time "$MAX_TIME" \
    -w '%{http_code} %{redirect_url}' "$url" 2>/dev/null || true)"
  redirect="${code#* }"
  code="${code%% *}"

  if [[ "$code" == "200" && -z "$redirect" ]]; then
    printf 'OK    %s\n' "$url"
  else
    if [[ -n "$redirect" ]]; then
      printf 'FAIL  %s (HTTP %s -> %s)\n' "$url" "$code" "$redirect"
    else
      printf 'FAIL  %s (HTTP %s)\n' "$url" "$code"
    fi
    failed=1
  fi
}

check_path "latest.json"
check_path "latest"
check_path "files/release_windows_xp.zip"

# list.json and by_tag are used by Refresh Release List / install-version
# selection.  They must not redirect XP to HTTPS either.
LATEST_JSON="$(curl -fsS --max-time "$MAX_TIME" "$BASE/latest.json" 2>/dev/null || true)"
TAG="$(printf '%s' "$LATEST_JSON" | jq -r '.tag_name // empty' 2>/dev/null || true)"

check_path "list.json"
if [[ -n "$TAG" ]]; then
  check_path "by_tag/$TAG.json"
fi

# Optional channel manifests (older mirror layouts and future sync scripts).
for manifest in stable.json unstable.json; do
  code="$(curl -sS -o /dev/null --max-time "$MAX_TIME" \
    -w '%{http_code}' "$BASE/$manifest" 2>/dev/null || true)"
  if [[ "$code" == "200" ]]; then
    check_path "$manifest"
  else
    printf 'SKIP  %s/%s (not published)\n' "$BASE" "$manifest"
  fi
done

if [[ "$failed" -ne 0 ]]; then
  printf '\nXP updater endpoints must be served over HTTP. Add nginx exceptions for\n'
  printf '/sof_buddy/releases/ before the global HTTP -> HTTPS redirect; see README.md.\n'
  exit 1
fi

printf '\nAll XP updater endpoints are HTTP-clean.\n'
