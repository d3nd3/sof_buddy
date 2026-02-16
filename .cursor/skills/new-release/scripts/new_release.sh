#!/usr/bin/env bash
set -e
cd "$(git rev-parse --show-toplevel)"
VERSION_FILE="VERSION"
VERSION_H="hdr/version.h"
DRY_RUN=
MSG=
OVERRIDE_VERSION=

while [[ $# -gt 0 ]]; do
  case $1 in
    --dry-run) DRY_RUN=1; shift ;;
    -m) MSG="$2"; shift 2 ;;
    [0-9]*.*[0-9]*) OVERRIDE_VERSION="$1"; shift ;;
    *) echo "Usage: $0 [--dry-run] [-m 'commit msg'] [VERSION]"; exit 1 ;;
  esac
done

if [[ -n "$OVERRIDE_VERSION" ]]; then
  echo "$OVERRIDE_VERSION" > "$VERSION_FILE"
  echo "Set VERSION to $OVERRIDE_VERSION"
else
  [[ -f ./increment_version.sh ]] || { echo "increment_version.sh not found"; exit 1; }
  ./increment_version.sh
fi

make "$VERSION_H"
NEW_VER=$(cat "$VERSION_FILE" | tr -d '\r\n')
git add "$VERSION_FILE" "$VERSION_H"
[[ -n "$MSG" ]] || MSG="Release v$NEW_VER"
git commit -m "$MSG"
if [[ -n "$DRY_RUN" ]]; then
  echo "Dry run: not pushing. Commit created."
else
  git push origin master
  echo "Pushed. CI will create release v${NEW_VER}-build<N>."
fi
