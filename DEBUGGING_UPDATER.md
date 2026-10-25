# Debugging the updater (local, no cloud)

Use this when you want the **real** update path—download → **game quits** → `winstart` / installer → **you relaunch**—without hitting GitHub or SofVault.

## Get the portable kit zip (one command)

From the repo root (needs mingw cross-compiler, same as a normal release build):

```bash
make update-test
```

The file you copy elsewhere:

```text
bin/sof_buddy-update-test-kit.zip
```

That zip includes:

- `sof_buddy-universal-update-test-old.dll` — install as `sof_buddy.dll` to **start** (`0.0.0-test`)
- `sof_buddy-universal-update-test-new.dll` — what the fixture “release” installs (`99.0.0-test`)
- `sof_buddy/sof_buddy_update.exe` — GUI updater (must be **static**; see Wine notes below)
- `sof_buddy/update_from_zip.cmd` and `sof_buddy/update_from_zip.sh` — install scripts (keep in sync with the game tree)
- `fixtures/` — JSON + release zips served locally
- `serve.py`, `README.md` — run the fake release server

Individual binaries also land in `bin/` if you prefer not to use the zip.

`make update-test-package` runs `tools/update_test/verify_updater.py` first (static GUI exe + fixed `.cmd`).

## Quick test loop

1. Unzip the kit on the machine that runs SoF (or copy **old DLL + `sof_buddy/` scripts + `sof_buddy_update.exe`** into your game tree).
2. On the host that serves fixtures (often the same PC as Wine):

   ```bash
   make update-test-serve
   ```

   Feed: `http://127.0.0.1:8765/releases/latest.json`

3. Point the game at the fixture (test DLL defaults to `127.0.0.1:8765`; or set `_sofbuddy_update_api_url` / `_sofbuddy_update_releases_url` in `sofbuddy.cfg`).
4. Launch SoF → startup prompt → **Download + install (Y)** (or `sofbuddy_update prompt` then Y).
5. Game exits; **`sof_buddy_update.exe`** runs via `winstart` / `update_from_zip.cmd`.
6. **Start SoF again** — version should be `99.0.0-test`; check `sof_buddy/update/TEST_UPDATE_PAYLOAD.txt`.

Test builds **block** production URLs unless `_sofbuddy_update_allow_production_urls 1`.

## Wine / Proton (Linux)

Typical layout: `~/.wine/drive_c/.../Soldier of Fortune/` with `sof_buddy.dll` in the game root and tools under `sof_buddy/`.

### Graphical updater only (fast check)

From the **game root** (paths with spaces quoted):

```bash
cd "$HOME/.wine/drive_c/users/dinda/Soldier of Fortune"
wine sof_buddy/sof_buddy_update.exe sof_buddy/update/sof_buddy-99.0.0-test.zip
```

You should see progress reach 100% and **Close** enabled—not an immediate error dialog.

### Common failures

| Symptom | Cause | Fix |
|--------|--------|-----|
| `libwinpthread-1.dll` not found when starting `sof_buddy_update.exe` | Non-static mingw build | Use `bin/sof_buddy_update.exe` from `make win-updater` (linked with `-static`). Run `make update-test-verify`. |
| `Could not find sof_buddy\update_from_zip.cmd` | Game root detection bug (fixed in current `sof_buddy_update.exe`) | Copy fresh `sof_buddy_update.exe` into `sof_buddy/`. |
| GUI: **The update could not be applied** with only a log **path** (no text) | Older `sof_buddy_update.exe` showed the log filename, not its contents | Rebuild: `make win-updater`, copy `bin/sof_buddy_update.exe` → `sof_buddy/`. |
| Log: `extract failed: …\sof_buddy\sof_buddy_update.exe` | Release zip updates the GUI exe while it is **still running** | Current updater **skips** its own path and continues; copy `bin/sof_buddy_update.exe` manually if you need that binary refreshed. |
| GUI: **The update could not be applied** (empty or tiny log) | Stale `update_from_zip.cmd`: silent progress used `echo 5\|Preparing...` and `\|` is a **cmd pipe** | Copy `rsrc/win_scripts/update_from_zip.cmd` → `sof_buddy/update_from_zip.cmd`, or reinstall from a new kit / fixture zip after `make update-test-fixtures`. |
| **Invalid Name** / ShellExecute errors under Wine | Game path contains spaces (`Soldier of Fortune`); `start /unix` + host `unzip` breaks | Current **`sof_buddy_update.exe`** extracts the zip **in-process** (miniz) and only falls back to `.cmd` if that fails. Rebuild with `make win-updater`. |
| GUI fails; terminal shows **dbus** / **unzip** under Wine | Fallback `.cmd` path on Wine | Prefer in-process GUI extract; fix `.cmd` or use `update_from_zip.sh` from Linux. |
| Update “succeeds” then next run still broken | Fixture zip overwrote `sof_buddy/` with an old `.cmd` | Re-copy scripts from kit or repo, then `make update-test-fixtures` before serving. |

### Logs and artifacts

| Path | Meaning |
|------|---------|
| `sof_buddy/update/.install.log` | stdout/stderr from `update_from_zip.cmd` when the GUI updater runs |
| `sof_buddy/update/.progress` | Last `percent\|status` line for the progress bar |
| `sof_buddy/update/install_context.env` | Wine/Proton flags and optional `HOST_*` paths for host extract |

### Manual extract (bypass GUI)

```bash
SOF="$HOME/.wine/drive_c/users/dinda/Soldier of Fortune"
/bin/bash "$SOF/sof_buddy/update_from_zip.sh" "$SOF/sof_buddy/update/sof_buddy-99.0.0-test.zip"
```

Or under Wine:

```bash
cd "$SOF" && wine cmd /c "sof_buddy\\update_from_zip.cmd sof_buddy\\update\\sof_buddy-99.0.0-test.zip"
```

## Other machines on your LAN

Copy `bin/sof_buddy-update-test-kit.zip`, run `python3 serve.py` from the unpacked folder, then on the game PC:

```text
set _sofbuddy_update_api_url http://<server-ip>:8765/releases/latest.json
set _sofbuddy_update_releases_url http://<server-ip>:8765/releases/page
```

## Rebuild only fixture zips (after code changes)

```bash
make update-test-fixtures
```

Re-packages the kit zip:

```bash
make update-test-package
```

## More detail

See [tools/update_test/README.md](tools/update_test/README.md) (release list UI, `99.1.0-test` long-notes scenario, cvars).
