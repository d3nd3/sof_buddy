#!/usr/bin/env bash
exec "$(cd "$(dirname "$0")/../.." && pwd)/tools/new_release.sh" --sof2 "$@"
