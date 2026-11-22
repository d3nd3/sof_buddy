# sof2bud — release notes & Discord

Same workflow as SoF Buddy, scoped to `sof2/`:

| | SoF Buddy | sof2bud |
|---|-----------|---------|
| Version file | `VERSION` | `sof2/VERSION` |
| Changelog | `CHANGELOG.md` | **`sof2/CHANGELOG.md`** |
| Tag | `v8.47-build123` | **`sof2-v0.2-build123`** |
| CI | `build-and-release.yaml` | **`sof2-build-and-release.yaml`** |

## Writing `sof2/CHANGELOG.md`

Mirror the SoF1 style in root `CHANGELOG.md`:

1. Add **`## vX.Y`** at the top (must match `sof2/VERSION` when you cut the release).
2. Use **`###` sections** by area (Launch, Baseline, Graphics, Menus, Install, …).
3. Bullets are **user-visible** (what players get, not file names unless helpful).
4. Move finished work out of **`## Unreleased`** into the new version section before bumping `VERSION`.
5. Do not ship an empty or placeholder `## vX.Y` block.

Review since last **sof2** tag:

```bash
LAST=$(./tools/new_release.sh --sof2 --last-release)
git log "$LAST..HEAD" --oneline -- sof2/
```

## Cut a release

```bash
./sof2/tools/new_release.sh          # bump sof2/VERSION
# edit sof2/CHANGELOG.md — add ## vX.Y, clear Unreleased
./sof2/tools/new_release.sh --check
./sof2/tools/new_release.sh --commit -m "sof2bud vX.Y: one-line summary"
```

## GitHub release body

CI runs:

```bash
python3 tools/changelog_release_body.py VERSION sof2/CHANGELOG.md
```

That section becomes the GitHub Release description (and what most bots read).

## Discord **#sof2-releases**

**Filter:** only posts where `tag_name` matches `^sof2-v[0-9]`.

**Body source:** GitHub `release.body` for that tag (from `sof2/CHANGELOG.md`).

Optional formatted copy for webhooks (bold headers, no `##`):

```bash
python3 tools/changelog_release_body.py 0.2 sof2/CHANGELOG.md --product sof2bud --discord
```

CI also writes `sof2_discord_announce.md` on each sof2 release (artifact + same text appended under a horizontal rule in the GitHub release).

**Do not** maintain a second changelog file; keep **`sof2/CHANGELOG.md`** as the single source of truth.
