#!/usr/bin/env bash
# Pull the latest Windows XP releases from GitHub into the sofvault HTTP mirror.
# Intended for cron on the sofvault host (same pull model as sof1maps sync).
#
# The live host only exempts /sof_buddy/releases/files/ from its global
# HTTP -> HTTPS redirect (plus the legacy root latest.json and latest files).
# Everything the XP updater needs is therefore published under files/:
#
#   files/latest.json          newest stable release (also at the root)
#   files/stable.json          stable channel manifest
#   files/unstable.json        newest prerelease manifest, when one exists
#   files/list.json            compact install-version list
#   files/by_tag/<tag>.json    per-tag manifests
#   files/release_windows_xp.zip
#   files/release_windows_xp_unstable.zip
#
# Root copies are kept for older clients and non-XP consumers.
#
# Example crontab (every 30 minutes):
#   */30 * * * * /path/to/sync_from_github.sh >> /var/log/sofvault-buddy-sync.log 2>&1
#
# Override paths for testing:
#   SOFVAULT_ROOT=/tmp/sof_buddy/releases ./sync_from_github.sh

set -euo pipefail

ROOT="${SOFVAULT_ROOT:-/var/www/html/sofvault.org/sof_buddy/releases}"
HOST_BASE="${SOFVAULT_PUBLIC_BASE:-http://sofvault.org/sof_buddy/releases}"
HOST_BASE="${HOST_BASE%/}"
FILES="$ROOT/files"
BY_TAG="$ROOT/by_tag"
FILES_BY_TAG="$FILES/by_tag"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

API_LATEST="https://api.github.com/repos/d3nd3/sof_buddy/releases/latest"
API_LIST="https://api.github.com/repos/d3nd3/sof_buddy/releases?per_page=100"
UA="sofvault-mirror/1.0"
STABLE_ZIP="release_windows_xp.zip"
UNSTABLE_ZIP="release_windows_xp_unstable.zip"
STABLE_ZIP_URL="$HOST_BASE/files/$STABLE_ZIP"
UNSTABLE_ZIP_URL="$HOST_BASE/files/$UNSTABLE_ZIP"

gh_get() {
  curl -fsSL \
    -H "Accept: application/vnd.github+json" \
    -H "X-GitHub-Api-Version: 2022-11-28" \
    -H "User-Agent: $UA" \
    "$1" -o "$2"
}

gh_get "$API_LATEST" "$TMP/release.json"
gh_get "$API_LIST" "$TMP/releases.json"

# Never overwrite good manifests with a GitHub rate-limit/error object.
jq -e 'type == "array"' "$TMP/releases.json" >/dev/null || {
  echo "GitHub release list is not an array (rate limit or API error); keeping the previous mirror"
  exit 1
}

TAG="$(jq -r '.tag_name // .version // .tag // empty' "$TMP/release.json")"
PRERELEASE="$(jq -r '.prerelease // false' "$TMP/release.json")"
STABLE_ASSET="$(jq -r --arg zip "$STABLE_ZIP" '
  [ .assets[]?.browser_download_url
    | select(test("/" + $zip + "$"; "i"))
  ] | first // empty
' "$TMP/release.json")"

UNSTABLE_TAG="$(jq -r --arg stable "$TAG" '
  [ .[]
    | select((.prerelease == true) or (.tag_name | test("-unstable")))
    | select(.tag_name != $stable)
  ] | first | .tag_name // empty
' "$TMP/releases.json")"

UNSTABLE_ASSET=""
if [[ -n "$UNSTABLE_TAG" ]]; then
  # Every release ships the package as release_windows_xp.zip; the mirror
  # stores the prerelease copy under a separate local name so both channels
  # can be installed without overwriting each other.
  UNSTABLE_ASSET="$(jq -r --arg tag "$UNSTABLE_TAG" --arg zip "$STABLE_ZIP" '
    [ .[]
      | select(.tag_name == $tag)
      | .assets[]?.browser_download_url
      | select(test("/" + $zip + "$"; "i"))
    ] | first // empty
  ' "$TMP/releases.json")"
fi

if [[ -z "$TAG" || -z "$STABLE_ASSET" ]]; then
  echo "No valid stable release tag or $STABLE_ZIP asset found"
  exit 1
fi

mkdir -p "$FILES" "$BY_TAG" "$FILES_BY_TAG"

download_zip() {
  local asset="$1" out="$2" state="$3" tag="$4"
  [[ -n "$asset" ]] || return 0
  if [[ -s "$out" && -f "$state" && "$(<"$state")" == "$tag" ]]; then
    return 0
  fi
  curl -fL --retry 3 -H "User-Agent: $UA" "$asset" -o "$out.tmp"
  mv "$out.tmp" "$out"
  printf '%s\n' "$tag" > "$state"
  echo "Downloaded $tag -> $out"
}

download_zip "$STABLE_ASSET" "$FILES/$STABLE_ZIP" "$ROOT/.synced_tag" "$TAG"

if [[ -n "$UNSTABLE_TAG" && -n "$UNSTABLE_ASSET" ]]; then
  download_zip "$UNSTABLE_ASSET" "$FILES/$UNSTABLE_ZIP" \
    "$ROOT/.synced_unstable_tag" "$UNSTABLE_TAG"
else
  # An unstable release without an XP asset must not leave an older package
  # advertised under the new tag.
  rm -f "$FILES/$UNSTABLE_ZIP" "$ROOT/.synced_unstable_tag"
fi

jq -r '.body // ""' "$TMP/release.json" > "$TMP/stable_body.txt"

# Channel manifests -----------------------------------------------------------
jq -n \
  --arg tag "$TAG" \
  --arg name "$(jq -r '.name // ("Release " + (.tag_name // ""))' "$TMP/release.json")" \
  --arg html "$HOST_BASE/latest" \
  --arg zip "$STABLE_ZIP_URL" \
  --rawfile body "$TMP/stable_body.txt" \
  '{tag_name:$tag, prerelease:false, name:$name, html_url:$html, zip_url:$zip, body:$body}' \
  > "$TMP/stable.json"

if [[ -n "$UNSTABLE_TAG" ]]; then
  jq -r --arg tag "$UNSTABLE_TAG" '
    [ .[] | select(.tag_name == $tag) ] | first | .body // ""
  ' "$TMP/releases.json" > "$TMP/unstable_body.txt"
  unstable_zip=""
  [[ -n "$UNSTABLE_ASSET" ]] && unstable_zip="$UNSTABLE_ZIP_URL"
  jq -n \
    --arg tag "$UNSTABLE_TAG" \
    --arg name "$(jq -r --arg tag "$UNSTABLE_TAG" '
      [ .[] | select(.tag_name == $tag) ] | first | .name // ("Release " + $tag)
    ' "$TMP/releases.json")" \
    --arg html "$HOST_BASE/latest" \
    --arg zip "$unstable_zip" \
    --rawfile body "$TMP/unstable_body.txt" \
    '{tag_name:$tag, prerelease:true, name:$name, html_url:$html, zip_url:$zip, body:$body}' \
    > "$TMP/unstable.json"
else
  rm -f "$TMP/unstable.json" "$ROOT/unstable.json" "$FILES/unstable.json"
fi

# Latest stable manifest (root + HTTP-clean files/ copy) ----------------------
jq -n \
  --arg tag "$TAG" \
  --arg name "$(jq -r '.name // ("Release " + (.tag_name // ""))' "$TMP/release.json")" \
  --arg html "$HOST_BASE/latest" \
  --arg zip "$STABLE_ZIP_URL" \
  --arg unstable "$UNSTABLE_TAG" \
  --rawfile body "$TMP/stable_body.txt" \
  --argjson prerelease "$PRERELEASE" \
  '{tag_name:$tag, prerelease:$prerelease, name:$name, html_url:$html, zip_url:$zip, body:$body, stable_tag:$tag, unstable_tag:$unstable}' \
  > "$ROOT/latest.json.tmp"
mv "$ROOT/latest.json.tmp" "$ROOT/latest.json"

# Compact install-version list.  Only the two current channel packages carry
# a zip_url; older tags are listed for comparison but are not archived.
jq \
  --arg stable "$TAG" \
  --arg unstable "$UNSTABLE_TAG" \
  --arg stable_zip "$STABLE_ZIP_URL" \
  --arg unstable_zip "$UNSTABLE_ZIP_URL" \
  --arg html "$HOST_BASE/latest" \
  '[ .[] | {
      tag_name: .tag_name,
      prerelease: (.prerelease // false),
      name: (.name // ("Release " + .tag_name)),
      html_url: $html,
      zip_url: (if .tag_name == $stable then $stable_zip
                elif (.tag_name == $unstable and $unstable != "") then $unstable_zip
                else "" end)
    } ]' "$TMP/releases.json" > "$TMP/list.json"

# Per-tag manifests, written to the legacy by_tag/ and the HTTP-clean
# files/by_tag/ tree.
jq -c '.[]' "$TMP/list.json" | while IFS= read -r rel; do
  t="$(jq -r '.tag_name // empty' <<<"$rel")"
  [[ -z "$t" ]] && continue
  jq -n --argjson rel "$rel" \
    '{tag_name:$rel.tag_name, prerelease:($rel.prerelease // false),
      name:$rel.name, html_url:$rel.html_url, zip_url:($rel.zip_url // "")}' \
    > "$TMP/by_tag.json"
  cp "$TMP/by_tag.json" "$BY_TAG/$t.json.tmp"
  mv "$BY_TAG/$t.json.tmp" "$BY_TAG/$t.json"
  cp "$TMP/by_tag.json" "$FILES_BY_TAG/$t.json.tmp"
  mv "$FILES_BY_TAG/$t.json.tmp" "$FILES_BY_TAG/$t.json"
done

jq -r '.[].tag_name' "$TMP/list.json" > "$TMP/tags.txt"
for dir in "$BY_TAG" "$FILES_BY_TAG"; do
  find "$dir" -maxdepth 1 -type f -name '*.json' -print | while IFS= read -r f; do
    base="$(basename "$f" .json)"
    grep -Fxq "$base" "$TMP/tags.txt" || rm -f "$f"
  done
done

# Publish the root copies last so a client never sees a manifest that points
# at a package that has not finished downloading.
for name in stable.json unstable.json list.json; do
  [[ -f "$TMP/$name" ]] || continue
  cp "$TMP/$name" "$ROOT/$name.tmp"
  mv "$ROOT/$name.tmp" "$ROOT/$name"
  cp "$TMP/$name" "$FILES/$name.tmp"
  mv "$FILES/$name.tmp" "$FILES/$name"
done
cp "$ROOT/latest.json" "$FILES/latest.json.tmp"
mv "$FILES/latest.json.tmp" "$FILES/latest.json"

printf '%s\n' "$TAG" > "$ROOT/latest"

echo "Synced stable=$TAG unstable=${UNSTABLE_TAG:-none} -> $STABLE_ZIP (+ files/ manifests)"
