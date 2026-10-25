#!/bin/sh
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="${1:-$HOME/Downloads/sofplus-clients}"
DST="$ROOT/rsrc/sofplus_client_patches"
for v in cd gog cd-kb4571756 gog-kb4571756; do
  from="$SRC/sofplus-client-patch-20140531-$v"
  test -d "$from" || { echo "missing $from" >&2; exit 1; }
  rm -rf "$DST/$v"
  cp -a "$from" "$DST/$v"
done
echo "Synced SoF Plus patch kits into $DST"
