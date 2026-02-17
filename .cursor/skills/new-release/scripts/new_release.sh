#!/usr/bin/env bash
set -e
cd "$(git rev-parse --show-toplevel)"
VERSION_FILE="VERSION"
VERSION_H="hdr/version.h"
DRY_RUN=
MSG=
OVERRIDE_VERSION=
COMMIT_ONLY=
LAST_RELEASE=

while [[ $# -gt 0 ]]; do
  case $1 in
    --dry-run) DRY_RUN=1; shift ;;
    --commit) COMMIT_ONLY=1; shift ;;
    --last-release) LAST_RELEASE=1; shift ;;
    -m) MSG="$2"; shift 2 ;;
    [0-9]*.*[0-9]*) OVERRIDE_VERSION="$1"; shift ;;
    *) echo "Usage: $0 [--dry-run] [--commit] [--last-release] [-m 'msg'] [VERSION]"; exit 1 ;;
  esac
done

if [[ -n "$LAST_RELEASE" ]]; then
  REPO=$(git remote get-url origin | sed -n 's/.*github.com[:/]\([^.]*\)\.git/\1/p')
  [[ -n "$REPO" ]] && curl -s "https://api.github.com/repos/$REPO/releases/latest" | grep '"tag_name"' | head -1 | sed 's/.*"tag_name"[^"]*"\([^"]*\)".*/\1/'
  exit 0
fi

if [[ -n "$COMMIT_ONLY" ]]; then
  NEW_VER=$(cat "$VERSION_FILE" | tr -d '\r\n')
  [[ -n "$MSG" ]] || MSG="Release v$NEW_VER"
  git add .
  git commit -m "$MSG"
  if [[ -n "$DRY_RUN" ]]; then
    echo "Dry run: commit created, not pushing."
  else
    git push origin master
    echo "Pushed. CI will create release v${NEW_VER}-build<N>."
  fi
  exit 0
fi

if [[ -n "$OVERRIDE_VERSION" ]]; then
  echo "$OVERRIDE_VERSION" > "$VERSION_FILE"
  echo "Set VERSION to $OVERRIDE_VERSION"
else
  [[ -f ./increment_version.sh ]] || { echo "increment_version.sh not found"; exit 1; }
  ./increment_version.sh
fi

make "$VERSION_H"
NEW_VER=$(cat "$VERSION_FILE" | tr -d '\r\n')
git add "$VERSION_FILE" && git add -f "$VERSION_H"
echo "Staged VERSION and hdr/version.h (v$NEW_VER). Create changelog, then run: $0 --commit [--dry-run] [-m 'Release v$NEW_VER']"
