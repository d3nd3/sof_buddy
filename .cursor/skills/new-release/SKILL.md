---
name: new-release
description: Bumps SoF Buddy version, updates VERSION and hdr/version.h, then git add/commit/push to trigger a GitHub Actions release. Use when creating a new release, cutting a build, or when the user asks to release, bump version, or push a new build. Agent must study commit history and changes to carefully create a changelog for the release. Do not skip over anything, if there is change in any feature, it must be documented in the changelog and change. This command is useful eg. `git diff v4.3-build143 --stat`
---

# New Release

## How releases work

- **VERSION** (repo root) holds `MAJOR.MINOR` (e.g. `2.3`). **hdr/version.h** is generated from it (defines `SOFBUDDY_VERSION`).
- Push to **master** runs `.github/workflows/build-and-release.yaml`: it reads **VERSION**, builds, and creates a GitHub release with tag `v{VERSION}-build{run_number}`.

## Preferred: two-phase script (agent workflow)

From repo root:

**1. Prepare:** Bump version and stage version files only.

```bash
./.cursor/skills/new-release/scripts/new_release.sh
```

Or set a specific version: `new_release.sh 2.5`

**2. Changelog:** Base the changelog on the **last release on GitHub** (compare against it):

- Get last release tag: `LAST_TAG=$(./.cursor/skills/new-release/scripts/new_release.sh --last-release)`
- Fetch that tag so it exists locally: `git fetch origin tag "$LAST_TAG" 2>/dev/null || true`
- Commits since last release: `git log "$LAST_TAG..HEAD" --oneline` (or without `--oneline` for full messages)
- Diff since last release: `git diff "$LAST_TAG..HEAD" --stat`
- Create or update the changelog from those commits and changes (no need to git add; step 3 stages everything).

**3. Commit and push:**

```bash
./.cursor/skills/new-release/scripts/new_release.sh --commit
```

This runs `git add .`, then `git commit`, then push. Use `-m "Release vX.Y: summary"` for a custom message, or `--dry-run` to commit but not push.

**Options:**

- `--commit` — run `git add .`, commit with message, and push (no bump). Use after prepare + changelog.
- `--dry-run` — with prepare: only bump and stage; with --commit: commit but do not push.
- `-m "message"` — custom commit message.
- `2.4` (positional) — set version to 2.4 instead of auto-incrementing.

**Examples:**

```bash
new_release.sh --dry-run
new_release.sh 2.5
# ... agent adds changelog ...
new_release.sh --commit -m "Release v2.5: HTTP maps, internal menus"
new_release.sh --commit --dry-run
```

## Manual steps

If not using the script:

1. Bump: `./increment_version.sh` (or write desired `MAJOR.MINOR` to **VERSION**).
2. Regenerate header: `make hdr/version.h`
3. `git add VERSION hdr/version.h` (use `git add -f hdr/version.h` if ignored) and any other files (changelog, etc.).
4. `git commit -m "Release vX.Y"`
5. `git push origin master`

CI will build and publish the release; the build number is assigned by GitHub Actions (`run_number`).
