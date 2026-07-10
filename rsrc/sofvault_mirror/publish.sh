#!/usr/bin/env bash
# Publish Windows XP updater mirror files to sofvault.org.
# Run after each release (or use CI artifact sofvault_latest.json + release_windows_xp.zip).
#
# Usage:
#   SOFVAULT_DEST=user@sofvault.org:/var/www/sof_buddy/releases \
#     ./rsrc/sofvault_mirror/publish.sh sofvault_latest.json release_windows_xp.zip

set -euo pipefail

DEST="${SOFVAULT_DEST:?Set SOFVAULT_DEST=user@host:/remote/sof_buddy/releases}"
JSON="${1:?manifest json (e.g. sofvault_latest.json)}"
ZIP="${2:?release_windows_xp.zip}"

scp "$JSON" "$DEST/latest.json"
scp "$ZIP" "$DEST/files/release_windows_xp.zip"
echo "Published XP mirror to $DEST"
