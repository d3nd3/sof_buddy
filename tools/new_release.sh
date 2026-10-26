#!/usr/bin/env bash
set -e
cd "$(git rev-parse --show-toplevel)"
VERSION_FILE="VERSION"
VERSION_H="hdr/version.h"
CHANGELOG="CHANGELOG.md"
PRODUCT=sof1
DRY_RUN=
MSG=
OVERRIDE_VERSION=
COMMIT_ONLY=
LAST_RELEASE=
CHECK_ONLY=
CI_MODE=

while [[ $# -gt 0 ]]; do
  case $1 in
    --sof2) PRODUCT=sof2; shift ;;
    --dry-run) DRY_RUN=1; shift ;;
    --commit) COMMIT_ONLY=1; shift ;;
    --check) CHECK_ONLY=1; shift ;;
    --ci) CI_MODE=1; shift ;;
    --last-release) LAST_RELEASE=1; shift ;;
    -m) MSG="$2"; shift 2 ;;
    [0-9]*.*[0-9]*) OVERRIDE_VERSION="$1"; shift ;;
    *) echo "Usage: $0 [--sof2] [--dry-run] [--commit] [--check] [--ci] [--last-release] [-m 'msg'] [VERSION]"; exit 1 ;;
  esac
done

if [[ "$PRODUCT" == sof2 ]]; then
  VERSION_FILE="sof2/VERSION"
  VERSION_H=
  CHANGELOG="sof2/CHANGELOG.md"
fi

read_version() { tr -d '\r\n' < "$1"; }

version_gt() {
  local a="$1" b="$2"
  local a_major a_minor b_major b_minor
  IFS=. read -r a_major a_minor <<< "$a"
  IFS=. read -r b_major b_minor <<< "$b"
  if (( a_major > b_major )); then return 0; fi
  if (( a_major < b_major )); then return 1; fi
  (( a_minor > b_minor ))
}

latest_release_tag() {
  local repo
  repo=$(git remote get-url origin 2>/dev/null | sed -n 's/.*github.com[:/]\([^.]*\)\.git/\1/p')
  [[ -n "$repo" ]] || return 1
  curl -fsSL "https://api.github.com/repos/$repo/releases?per_page=100" | python3 -c "
import json, re, sys
product = sys.argv[1]
for r in json.load(sys.stdin):
    t = r.get('tag_name', '')
    if product == 'sof2' and re.match(r'^sof2-v[0-9]', t):
        print(t); break
    if product == 'sof1' and re.match(r'^v[0-9]', t) and not t.startswith('sof2-'):
        print(t); break
" "$PRODUCT"
}

version_from_tag() {
  local tag="$1"
  if [[ "$PRODUCT" == sof2 ]]; then
    sed -n 's/^sof2-v\([0-9][0-9.]*\)-build[0-9][0-9]*$/\1/p' <<< "$tag"
  else
    sed -n 's/^v\([0-9][0-9.]*\)-build[0-9][0-9]*$/\1/p' <<< "$tag"
  fi
}

run_release_check() {
  local new_ver head_ver last_tag last_ver
  new_ver=$(read_version "$VERSION_FILE")
  [[ "$new_ver" =~ ^[0-9]+\.[0-9]+$ ]] || { echo "check: invalid VERSION '$new_ver' (expected MAJOR.MINOR)"; return 1; }

  if [[ -z "$CI_MODE" ]]; then
    head_ver=$(git show HEAD:"$VERSION_FILE" 2>/dev/null | tr -d '\r\n' || true)
    if [[ -n "$head_ver" && "$new_ver" == "$head_ver" ]]; then
      echo "check: VERSION is still $new_ver (bump VERSION before release; run without args or ./tools/increment_version.sh)"
      return 1
    fi
  fi

  if ! grep -qE "^## v${new_ver}([[:space:]]|$)" "$CHANGELOG"; then
    echo "check: $CHANGELOG missing section '## v${new_ver}'"
    return 1
  fi

  if last_tag=$(latest_release_tag); then
    last_ver=$(version_from_tag "$last_tag")
    if [[ -n "$last_ver" ]] && ! version_gt "$new_ver" "$last_ver"; then
      echo "check: VERSION $new_ver must be greater than latest release $last_ver ($last_tag)"
      return 1
    fi
    echo "check: ok — release v${new_ver} (latest published: $last_tag)"
  else
    echo "check: ok — release v${new_ver} (no prior GitHub release found)"
  fi
}

if [[ -n "$LAST_RELEASE" ]]; then
  latest_release_tag || true
  exit 0
fi

if [[ -n "$CHECK_ONLY" ]]; then
  run_release_check
  exit 0
fi

if [[ -n "$COMMIT_ONLY" ]]; then
  run_release_check
  NEW_VER=$(read_version "$VERSION_FILE")
  [[ -n "$MSG" ]] || MSG="Release v$NEW_VER"
  git add .
  git commit -m "$MSG"
  if [[ -n "$DRY_RUN" ]]; then
    echo "Dry run: commit created, not pushing."
  else
    git push origin master
    if [[ "$PRODUCT" == sof2 ]]; then
      echo "Pushed. CI will create release sof2-v${NEW_VER}-build<N> when sof2/VERSION changes on master."
    else
      echo "Pushed. CI will create release v${NEW_VER}-build<N> when VERSION changes on master."
    fi
  fi
  exit 0
fi

if [[ -n "$OVERRIDE_VERSION" ]]; then
  echo "$OVERRIDE_VERSION" > "$VERSION_FILE"
  echo "Set VERSION to $OVERRIDE_VERSION"
else
  if [[ "$PRODUCT" == sof2 ]]; then
    ./sof2/tools/increment_version.sh
  else
    [[ -f ./tools/increment_version.sh ]] || { echo "tools/increment_version.sh not found"; exit 1; }
    ./tools/increment_version.sh
  fi
fi

NEW_VER=$(read_version "$VERSION_FILE")
git add "$VERSION_FILE"
if [[ -n "$VERSION_H" ]]; then
  make "$VERSION_H"
  git add -f "$VERSION_H"
fi
if last_tag=$(latest_release_tag 2>/dev/null); then
  echo "Latest GitHub release: $last_tag"
fi
if [[ "$PRODUCT" == sof2 ]]; then
  echo "Staged sof2/VERSION (v$NEW_VER). Add sof2/CHANGELOG.md, run: $0 --sof2 --check"
  echo "Then: $0 --sof2 --commit [--dry-run] [-m 'sof2bud v$NEW_VER']"
else
  echo "Staged VERSION and hdr/version.h (v$NEW_VER). Add CHANGELOG, run: $0 --check"
  echo "Then: $0 --commit [--dry-run] [-m 'Release v$NEW_VER']"
fi
