#!/bin/bash

# SoF Buddy Version Increment Script
# Automatically increments the minor version in VERSION file

VERSION_FILE="VERSION"

# Check if VERSION file exists
if [ ! -f "$VERSION_FILE" ]; then
    echo "Error: VERSION file not found!"
    exit 1
fi

# Read current version
CURRENT_VERSION=$(cat "$VERSION_FILE" | tr -d '\r\n')
echo "Current version: $CURRENT_VERSION"

# Parse major and minor versions
IFS='.' read -r MAJOR MINOR <<< "$CURRENT_VERSION"

# Validate version format
if ! [[ "$MAJOR" =~ ^[0-9]+$ ]] || ! [[ "$MINOR" =~ ^[0-9]+$ ]]; then
    echo "Error: Invalid version format. Expected MAJOR.MINOR (e.g., 1.0)"
    exit 1
fi

# Increment minor version
NEW_MINOR=$((MINOR + 1))

# Create new version string
NEW_VERSION="$MAJOR.$NEW_MINOR"

# Write new version to file
echo "$NEW_VERSION" > "$VERSION_FILE"

echo "Version incremented: $CURRENT_VERSION → $NEW_VERSION"
echo "Updated $VERSION_FILE"
if [[ -x ./.cursor/skills/new-release/scripts/new_release.sh ]]; then
  last=$(./.cursor/skills/new-release/scripts/new_release.sh --last-release 2>/dev/null || true)
  [[ -n "$last" ]] && echo "Latest GitHub release: $last → next tag will be v${NEW_VERSION}-build<N>"
fi
