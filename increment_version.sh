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

# Handle major version reset (e.g., 1.9 → 2.0)
if [ "$NEW_MINOR" -gt 9 ]; then
    NEW_MAJOR=$((MAJOR + 1))
    NEW_MINOR=0
else
    NEW_MAJOR=$MAJOR
fi

# Create new version string
NEW_VERSION="$NEW_MAJOR.$NEW_MINOR"

# Write new version to file
echo "$NEW_VERSION" > "$VERSION_FILE"

echo "Version incremented: $CURRENT_VERSION → $NEW_VERSION"
echo "Updated $VERSION_FILE"
