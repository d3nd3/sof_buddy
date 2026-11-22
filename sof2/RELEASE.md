# sof2bud releases

Releases are **independent** from SoF Buddy: version in `sof2/VERSION`, changelog in **`sof2/CHANGELOG.md`** (SoF Buddy–style `###` sections), GitHub tags `sof2-vX.Y-buildN`, Discord **#sof2-releases** via the same notes.

**How to write release notes:** [`sof2/docs/RELEASE_CHANGELOG.md`](docs/RELEASE_CHANGELOG.md)

## Cut a release

```bash
./sof2/tools/new_release.sh          # bump sof2/VERSION
# edit sof2/CHANGELOG.md — add ## vX.Y (move bullets out of Unreleased)
./sof2/tools/new_release.sh --check
./sof2/tools/new_release.sh --commit -m "sof2bud vX.Y: summary"
```

CI runs only when `sof2/**` changes on `master` (or via **Actions → SoF2 Build and Release → Run workflow** with *force release*).

## Artifacts

- `sof2_release_windows.zip` — `sof2bud.dll` + `sof2_buddy/` (patch scripts, `patches.json`)
- `sof2bud_setup_vX.Y.exe` — Windows installer (patches `SoF2MP.exe` import on finish)
