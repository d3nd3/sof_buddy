#!/usr/bin/env bash
# Pull latest Windows XP release + release list from GitHub into the sofvault HTTP mirror.
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
BY_TAG="$ROOT/by_tag"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

API_LATEST="https://api.github.com/repos/d3nd3/sof_buddy/releases/latest"
API_LIST="https://api.github.com/repos/d3nd3/sof_buddy/releases?per_page=100"
UA="sofvault-mirror/1.0"
XP_ZIP="release_windows_xp.zip"
MIRROR_ZIP="http://sofvault.org/sof_buddy/releases/files/$XP_ZIP"

gh_get() {
  curl -fsSL \
    -H "Accept: application/vnd.github+json" \
    -H "X-GitHub-Api-Version: 2022-11-28" \
    -H "User-Agent: $UA" \
    "$1" -o "$2"
}

gh_get "$API_LATEST" "$TMP/release.json"
gh_get "$API_LIST" "$TMP/list.json"

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
mkdir -p "$FILES" "$BY_TAG"

TAG_FILE="$ROOT/.synced_tag"
NEED_DL=0
[[ ! -s "$FILES/$FN" ]] && NEED_DL=1
[[ ! -f "$TAG_FILE" || "$(<"$TAG_FILE")" != "$TAG" ]] && NEED_DL=1

if [[ "$NEED_DL" -eq 1 ]]; then
  curl -fL --retry 3 -H "User-Agent: $UA" "$ASSET_URL" -o "$FILES/$FN.tmp"
  mv "$FILES/$FN.tmp" "$FILES/$FN"
  echo "$TAG" > "$TAG_FILE"
fi

mv "$TMP/list.json" "$ROOT/list.json.tmp"
mv "$ROOT/list.json.tmp" "$ROOT/list.json"

cat > "$ROOT/latest.json.tmp" <<EOF
{
  "tag_name": "$TAG",
  "html_url": "http://sofvault.org/sof_buddy/releases/latest",
  "zip_url": "$MIRROR_ZIP"
}
EOF
mv "$ROOT/latest.json.tmp" "$ROOT/latest.json"

# Per-tag manifests for XP install-version picker (latest tag gets mirror zip_url).
rm -f "$BY_TAG"/*.json.tmp 2>/dev/null || true
jq -c '.[]' "$ROOT/list.json" | while IFS= read -r rel; do
  t="$(jq -r '.tag_name // empty' <<<"$rel")"
  [[ -z "$t" ]] && continue
  html="$(jq -r '.html_url // "http://sofvault.org/sof_buddy/releases/latest"' <<<"$rel")"
  if [[ "$t" == "$TAG" ]]; then
    zip="$MIRROR_ZIP"
  else
    zip=""
  fi
  jq -n --arg tag "$t" --arg html "$html" --arg zip "$zip" \
    '{tag_name:$tag, html_url:$html, zip_url:$zip}' > "$BY_TAG/${t}.json.tmp"
  mv "$BY_TAG/${t}.json.tmp" "$BY_TAG/${t}.json"
done

echo "$TAG" > "$ROOT/latest"
echo "Synced $TAG -> $FN (+ list.json, $(find "$BY_TAG" -name '*.json' | wc -l) tag manifests)"
