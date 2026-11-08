# SoF Buddy — Release instructions (maintainers)

## Two version numbers (do not confuse them)

| Part | You control? | Example |
|------|----------------|---------|
| **Version** (`MAJOR.MINOR`) | Yes — `VERSION` file | `8.1` |
| **Build** | No — GitHub Actions `run_number` | `189` |

Stable tags look like **`v8.1-build189`**; unstable builds look like **`v8.1-build190-unstable`**.

- You only edit `VERSION` and `CHANGELOG.md`.
- You never edit the build number locally.

## Branches and channels

- **`unstable`** — day-to-day development. Every push builds and publishes a **prerelease** tagged `vX.Y-build<N>-unstable`. Its release notes come from `## Unreleased` in `CHANGELOG.md`.
- **`master`** — stable. Merge `unstable` here when a build is ready for everyone. Bump `VERSION` and add the matching `## vX.Y` section to publish a stable release.

No `stable.json` pointer is used anymore. The in-game updater treats the newest non-prerelease GitHub release as **stable** and the newest prerelease as **unstable**.

## What happens when you push

1. CI runs a quick **version gate** on every push (unless ignored paths).
2. On `unstable`, CI always builds and publishes a prerelease.
3. On `master`, **build, installer, and GitHub Release** run **only when `VERSION` changed** in that commit.
   - Same `VERSION` + new push → gate only; **no** compile, **no** stable release.
   - Bumped `VERSION` + push → full pipeline, **new** stable release `vX.Y-build<N>`.
   - The release also includes `sof_buddy_setup_v<version>.exe`, which installs the Windows
     package, writes the selected feature set to `sof_buddy/features.cfg`, and
     offers the Windows 10+ application-compatibility patch and optional full-violence
     registry setup.
   - On a `VERSION` bump, CI checks that `CHANGELOG.md` has a `## vX.Y` section.

See current releases: https://github.com/d3nd3/sof_buddy/releases

## Before you start

- Work on an up-to-date `unstable` branch; merge to `master` only when the build is stable.
- You need push access to `origin/unstable` and `origin/master`.
- Network access for `git push` and optionally `curl` (for `--check` / `--last-release`).

## Quick path (recommended script)

From the repository root:

**1. On `master`, after merging `unstable`, bump version and stage header files:**

```sh
./tools/new_release.sh
```

Or set an explicit version:

```sh
./tools/new_release.sh 8.2
```

**2. Write the changelog** — see [Writing CHANGELOG](#writing-changelog) below.

**3. Verify before commit:**

```sh
./tools/new_release.sh --check
```

**4. Commit and push** (`--check` runs again automatically):

```sh
./tools/new_release.sh --commit \
  -m "Release v8.2: short summary of what shipped"
```

**5. Watch CI:** https://github.com/d3nd3/sof_buddy/actions  
When the workflow finishes, the new tag appears under Releases.

## Writing CHANGELOG

Base release notes on everything since the **last published GitHub release**, not just your latest commit.

Get the last release tag:

```sh
LAST_TAG=$(./tools/new_release.sh --last-release)
echo "$LAST_TAG"
```

Example output: `v8.1-build189`

Review what changed:

```sh
git fetch origin tag "$LAST_TAG" 2>/dev/null || true
git log "$LAST_TAG..HEAD" --oneline
git diff "$LAST_TAG..HEAD" --stat
```

While developing on `unstable`, add user-visible notes under `## Unreleased` at the top of `CHANGELOG.md`; those become the unstable prerelease notes.

When promoting to `master`, rename that section to the stable version:

```markdown
## v8.2

### Area name
- Bullet describing user-visible change
- Another change
```

**Rules:**

- Stable heading must be exactly `## v8.2` (match `VERSION` in the `VERSION` file).
- Document every feature, fix, and behavior change since the last stable tag.
- Do not ship a release with an empty or placeholder section.

## Fully manual path (no prepare script)

1. On `master`, bump `VERSION`:

   ```sh
   ./tools/increment_version.sh
   ```

   Or edit `VERSION` by hand (format `MAJOR.MINOR`, e.g. `8.2`).

2. Regenerate the C header:

   ```sh
   make hdr/version.h
   ```

3. Update `CHANGELOG.md` (see above).

4. Validate:

   ```sh
   ./tools/new_release.sh --check
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

Release helper: `./tools/new_release.sh`.

## Troubleshooting

| Symptom | Fix |
|---------|-----|
| `--check`: `VERSION is still X.Y` | Run `new_release.sh` with no args, or `./tools/increment_version.sh`, before `--check` |
| `--check`: `CHANGELOG.md missing section ## vX.Y` | Add `## vX.Y` at top of `CHANGELOG.md` matching `VERSION` |
| `--check`: `VERSION must be greater than latest release` | Bump `VERSION` again — that version is already published |
| Pushed but no new GitHub release | `VERSION` did not change in that commit; bump + changelog, push again |
| CI failed on **Validate release metadata** | `VERSION` changed but changelog section is missing |
| Confusion over old tags like `v7.9-build185` | Historical only — use `--last-release`, not old changelog sections |

## After release

- Confirm the tag on https://github.com/d3nd3/sof_buddy/releases
- Stable tags are `vX.Y-build<N>` (non-prerelease); unstable tags end in `-unstable` and are marked prerelease.
- Normal development happens on `unstable`; merge to `master` only when ready for stable.
