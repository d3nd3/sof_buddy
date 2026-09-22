---
name: new-release
description: Bumps SoF Buddy VERSION, updates hdr/version.h and CHANGELOG, validates with --check, then commit/push to publish a GitHub release. Use when creating a release, cutting a build, or when the user asks to release or bump version. CI always builds on master push but publishes a GitHub release only when VERSION changes in that commit. Agent must study changes since the last release tag and document every feature/fix in CHANGELOG.md. Useful diff base: git diff v4.3-build143 --stat
---

# New Release

Human-oriented guide: [RELEASE_INSTRUCTIONS.md](../../../RELEASE_INSTRUCTIONS.md) (repo root).

## How releases work

| Part | Source | Example |
|------|--------|---------|
| **Version** (`MAJOR.MINOR`) | `VERSION` file (bump via this skill) | `8.1` |
| **Build** | GitHub Actions `run_number` (automatic, never edit locally) | `189` |

- **VERSION** (repo root) holds `MAJOR.MINOR`. **hdr/version.h** is generated from it (`SOFBUDDY_VERSION`).
- Push to **master** runs `.github/workflows/build-and-release.yaml`:
  - **Always:** compile + artifacts.
  - **Only when `VERSION` changed in that commit:** validate changelog, then publish GitHub release tag `v{VERSION}-build{run_number}`.
- Pushing feature fixes **without** bumping `VERSION` does **not** create a new GitHub release.

## Agent workflow

From repo root:

**1. Prepare** — bump version and stage version files only.

```bash
./.cursor/skills/new-release/scripts/new_release.sh
```

Or set a specific version: `./.cursor/skills/new-release/scripts/new_release.sh 2.5`

**2. Changelog** — base on the **last GitHub release** (not `HEAD` alone):

```bash
LAST_TAG=$(./.cursor/skills/new-release/scripts/new_release.sh --last-release)
git fetch origin tag "$LAST_TAG" 2>/dev/null || true
git log "$LAST_TAG..HEAD" --oneline
git diff "$LAST_TAG..HEAD" --stat
```

Add `## vX.Y` at the top of **CHANGELOG.md** covering **all** commits since `$LAST_TAG`. Do not skip features or behavior changes.

**3. Verify** — required before commit (`--commit` runs this automatically):

```bash
./.cursor/skills/new-release/scripts/new_release.sh --check
```

Fails when:

- `VERSION` equals `HEAD` (forgot to bump),
- `CHANGELOG.md` has no `## vX.Y` for the current `VERSION`,
- `VERSION` is not greater than the latest published release version.

**4. Commit and push:**

```bash
./.cursor/skills/new-release/scripts/new_release.sh --commit -m "Release vX.Y: short summary"
```

Runs `--check`, then `git add .`, `git commit`, and `git push origin master`.

## Script options

| Flag | Purpose |
|------|---------|
| *(no args)* | Auto-increment `VERSION`, regenerate `hdr/version.h`, stage both |
| `X.Y` | Set `VERSION` to `X.Y` instead of auto-increment |
| `--check` | Validate bump + changelog (+ version > latest release) |
| `--commit` | `--check`, then add/commit/push (use after prepare + changelog) |
| `--last-release` | Print latest GitHub release tag (e.g. `v8.1-build189`) |
| `--dry-run` | With `--commit`: commit but do not push |
| `-m "msg"` | Custom commit message (default `Release vX.Y`) |
| `--ci` | Internal: skip HEAD bump check (used by CI `version_gate` job) |

## Examples

```bash
./.cursor/skills/new-release/scripts/new_release.sh
# ... write CHANGELOG ## v8.2 ...
./.cursor/skills/new-release/scripts/new_release.sh --check
./.cursor/skills/new-release/scripts/new_release.sh --commit -m "Release v8.2: loading menu defaults"
```

## Manual steps (without script)

1. `./increment_version.sh` (or write `MAJOR.MINOR` to **VERSION**).
2. `make hdr/version.h`
3. Update **CHANGELOG.md** with `## vX.Y`.
4. `./.cursor/skills/new-release/scripts/new_release.sh --check`
5. `git add .` && `git commit -m "Release vX.Y: …"` && `git push origin master`

CI publishes the release only if **VERSION** changed in that commit.

## Common mistakes

- **Expecting a new release from a normal push** — bump `VERSION` and add changelog, or only CI build runs.
- **Changelog only for the latest commit** — diff since `$LAST_TAG`, include every shipped change.
- **Skipping `--check`** — catches forgotten bumps and missing changelog sections before push.
- **Confusing old tags** — `v7.9-build185` is historical; compare against `--last-release`, not old CHANGELOG sections.
