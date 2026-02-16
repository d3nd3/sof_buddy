---
name: new-release
description: Bumps SoF Buddy version, updates VERSION and hdr/version.h, then git add/commit/push to trigger a GitHub Actions release. Use when creating a new release, cutting a build, or when the user asks to release, bump version, or push a new build.
---

# New Release

## How releases work

- **VERSION** (repo root) holds `MAJOR.MINOR` (e.g. `2.3`). **hdr/version.h** is generated from it (defines `SOFBUDDY_VERSION`).
- Push to **master** runs `.github/workflows/build-and-release.yaml`: it reads **VERSION**, builds, and creates a GitHub release with tag `v{VERSION}-build{run_number}`.

## Preferred: run the script

From repo root:

```bash
./.cursor/skills/new-release/scripts/new_release.sh
```

Bumps minor version (via `./increment_version.sh`), regenerates **hdr/version.h**, stages **VERSION** and **hdr/version.h**, commits with message `Release vX.Y`, pushes to **origin master**.

**Options:**

- `--dry-run` — bump and commit only; do not push.
- `-m "message"` — use custom commit message.
- `2.4` (positional) — set version to 2.4 instead of auto-incrementing.

**Examples:**

```bash
.cursor/skills/new-release/scripts/new_release.sh --dry-run
.cursor/skills/new-release/scripts/new_release.sh -m "Release v2.4: fix update checker"
.cursor/skills/new-release/scripts/new_release.sh 2.5
```

## Manual steps

If not using the script:

1. Bump: `./increment_version.sh` (or write desired `MAJOR.MINOR` to **VERSION**).
2. Regenerate header: `make hdr/version.h`
3. `git add VERSION hdr/version.h` (and any other files you want in the release commit).
4. `git commit -m "Release vX.Y"`
5. `git push origin master`

CI will build and publish the release; the build number is assigned by GitHub Actions (`run_number`).
