#!/usr/bin/env bash
# Pull latest Windows XP release from GitHub into the sofvault HTTP mirror.
# Intended for cron on the sofvault host (same pull model as sof1maps sync).
#
# Example crontab (every 30 minutes):
#   */30 * * * * /path/to/sync_from_github.sh >> /var/log/sofvault-buddy-sync.log 2>&1
#
# Override paths for testing:
#   SOFVAULT_ROOT=/tmp/sof_buddy/releases ./sync_from_github.sh

set -euo pipefail

ROOT="${SOFVAULT_ROOT:-/var/www/html/sofvault.org/sof_buddy/releases}"
FILES="$ROOT/files"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

API="https://api.github.com/repos/d3nd3/sof_buddy/releases/latest"
UA="sofvault-mirror/1.0"
XP_ZIP="release_windows_xp.zip"

curl -fsSL \
  -H "Accept: application/vnd.github+json" \
  -H "X-GitHub-Api-Version: 2022-11-28" \
  -H "User-Agent: $UA" \
  "$API" -o "$TMP/release.json"

TAG="$(jq -r '.tag_name // .version // .tag // empty' "$TMP/release.json")"
ASSET_URL="$(jq -r --arg zip "$XP_ZIP" '
  [ .assets[]?.browser_download_url
    | select(test("/" + $zip + "$"; "i"))
  ] | first // empty
' "$TMP/release.json")"

if [[ -z "$TAG" || -z "$ASSET_URL" ]]; then
  echo "No valid release tag or $XP_ZIP asset found"
  exit 1
fi

FN="$XP_ZIP"
mkdir -p "$FILES"

TAG_FILE="$ROOT/.synced_tag"
NEED_DL=0
[[ ! -s "$FILES/$FN" ]] && NEED_DL=1
[[ ! -f "$TAG_FILE" || "$(<"$TAG_FILE")" != "$TAG" ]] && NEED_DL=1

if [[ "$NEED_DL" -eq 1 ]]; then
  curl -fL --retry 3 -H "User-Agent: $UA" "$ASSET_URL" -o "$FILES/$FN.tmp"
  mv "$FILES/$FN.tmp" "$FILES/$FN"
  echo "$TAG" > "$TAG_FILE"
fi

cat > "$ROOT/latest.json.tmp" <<EOF
{
  "tag_name": "$TAG",
  "html_url": "http://sofvault.org/sof_buddy/releases/latest",
  "zip_url": "http://sofvault.org/sof_buddy/releases/files/$FN"
}
EOF
mv "$ROOT/latest.json.tmp" "$ROOT/latest.json"

echo "$TAG" > "$ROOT/latest"
echo "Synced $TAG -> $FN"
