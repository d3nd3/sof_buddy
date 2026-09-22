# SoF Buddy — Release instructions (maintainers)

## Two version numbers (do not confuse them)

| Part | You control? | Example |
|------|----------------|---------|
| **Version** (`MAJOR.MINOR`) | Yes — `VERSION` file | `8.1` |
| **Build** | No — GitHub Actions `run_number` | `189` |

Published tags look like **`v8.1-build189`**.

- You only edit `VERSION` and `CHANGELOG.md`.
- You never edit the build number locally.

## What happens when you push to `master`

1. CI **always** compiles and uploads build artifacts.
2. A **GitHub Release** is created **only when `VERSION` changed** in that commit.
   - Same `VERSION` + new push → build runs, **no** new GitHub release.
   - Bumped `VERSION` + push → build runs, **new** release `vX.Y-build<N>`.
3. On a `VERSION` bump, CI checks that `CHANGELOG.md` has a `## vX.Y` section.

See current releases: https://github.com/d3nd3/sof_buddy/releases

## Before you start

- Work on an up-to-date `master` branch.
- You need push access to `origin/master`.
- Network access for `git push` and optionally `curl` (for `--check` / `--last-release`).

## Quick path (recommended script)

From the repository root:

**1. Bump version and stage header files:**

```sh
./.cursor/skills/new-release/scripts/new_release.sh
```

Or set an explicit version:

```sh
./.cursor/skills/new-release/scripts/new_release.sh 8.2
```

**2. Write the changelog** — see [Writing CHANGELOG](#writing-changelog) below.

**3. Verify before commit:**

```sh
./.cursor/skills/new-release/scripts/new_release.sh --check
```

**4. Commit and push** (`--check` runs again automatically):

```sh
./.cursor/skills/new-release/scripts/new_release.sh --commit \
  -m "Release v8.2: short summary of what shipped"
```

**5. Watch CI:** https://github.com/d3nd3/sof_buddy/actions  
When the workflow finishes, the new tag appears under Releases.

## Writing CHANGELOG

Base release notes on everything since the **last published GitHub release**, not just your latest commit.

Get the last release tag:

```sh
LAST_TAG=$(./.cursor/skills/new-release/scripts/new_release.sh --last-release)
echo "$LAST_TAG"
```

Example output: `v8.1-build189`

Review what changed:

```sh
git fetch origin tag "$LAST_TAG" 2>/dev/null || true
git log "$LAST_TAG..HEAD" --oneline
git diff "$LAST_TAG..HEAD" --stat
```

Add a new section at the **top** of `CHANGELOG.md`:

```markdown
## v8.2

### Area name
- Bullet describing user-visible change
- Another change
```

**Rules:**

- The heading must be exactly `## v8.2` (match `VERSION` in the `VERSION` file).
- Document every feature, fix, and behavior change since `LAST_TAG`.
- Do not ship a release with an empty or placeholder section.

## Fully manual path (no prepare script)

1. Bump `VERSION`:

   ```sh
   ./increment_version.sh
   ```

   Or edit `VERSION` by hand (format `MAJOR.MINOR`, e.g. `8.2`).

2. Regenerate the C header:

   ```sh
   make hdr/version.h
   ```

3. Update `CHANGELOG.md` (see above).

4. Validate:

   ```sh
   ./.cursor/skills/new-release/scripts/new_release.sh --check
   ```

5. Stage, commit, push:

   ```sh
   git add VERSION hdr/version.h CHANGELOG.md
   git add -f hdr/version.h
   git add .    # if other release-related files changed
   git commit -m "Release v8.2: short summary"
   git push origin master
   ```

## Script reference

| Command | Purpose |
|---------|---------|
| `new_release.sh` | Auto-increment `VERSION`, run `make hdr/version.h`, stage version files |
| `new_release.sh X.Y` | Set `VERSION` to `X.Y` instead of auto-increment |
| `new_release.sh --check` | Fail if bump/changelog/latest-release checks fail |
| `new_release.sh --commit [-m "msg"]` | `--check`, then `git add .`, commit, push |
| `new_release.sh --commit --dry-run` | Commit locally but do not push |
| `new_release.sh --last-release` | Print latest GitHub release tag |

All paths are under `./.cursor/skills/new-release/scripts/`.

## Troubleshooting

| Symptom | Fix |
|---------|-----|
| `--check`: `VERSION is still X.Y` | Run `new_release.sh` with no args, or `./increment_version.sh`, before `--check` |
| `--check`: `CHANGELOG.md missing section ## vX.Y` | Add `## vX.Y` at top of `CHANGELOG.md` matching `VERSION` |
| `--check`: `VERSION must be greater than latest release` | Bump `VERSION` again — that version is already published |
| Pushed but no new GitHub release | `VERSION` did not change in that commit; bump + changelog, push again |
| CI failed on **Validate release metadata** | `VERSION` changed but changelog section is missing |
| Confusion over old tags like `v7.9-build185` | Historical only — use `--last-release`, not old changelog sections |

## After release

- Confirm the tag on https://github.com/d3nd3/sof_buddy/releases
- In-game updater and badges use the new `vX.Y-build<N>` tag
- Normal bugfix work on `master` does not need a `VERSION` bump until the next public release
