#!/bin/bash
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION_FILE="$ROOT/VERSION"
CURRENT_VERSION=$(tr -d '\r\n' < "$VERSION_FILE")
IFS='.' read -r MAJOR MINOR <<< "$CURRENT_VERSION"
NEW_VERSION="$MAJOR.$((MINOR + 1))"
echo "$NEW_VERSION" > "$VERSION_FILE"
echo "sof2 VERSION: $CURRENT_VERSION -> $NEW_VERSION"
