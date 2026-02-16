#!/usr/bin/env bash
set -e
cd "$(git rev-parse --show-toplevel)"
git add -A
echo "=== status ==="
git status -sb
echo "=== staged diff (stat) ==="
git diff --cached --stat
echo "=== staged diff ==="
git diff --cached
