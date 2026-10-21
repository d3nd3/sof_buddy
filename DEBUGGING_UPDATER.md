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
- `sof_buddy_update.exe` — GUI updater
- `fixtures/` — JSON + release zips served locally
- `serve.py`, `README.md` — run the fake release server

Individual binaries also land in `bin/` if you prefer not to use the zip.

## Quick test loop

1. Unzip the kit on the machine that runs SoF (or only copy the **old** DLL into your game tree as `sof_buddy.dll`).
2. On the host that serves fixtures (often the same PC as Wine):

   ```bash
   make update-test-serve
   ```

   Feed: `http://127.0.0.1:8765/releases/latest.json`

3. Launch SoF → accept/download update → **`sofbuddy_update_install`** (or the startup menu).
4. Game exits; installer runs via **winstart**.
5. **Start SoF again** — version should be `99.0.0-test`; check `sof_buddy/update/TEST_UPDATE_PAYLOAD.txt`.

Test builds **block** production URLs unless `_sofbuddy_update_allow_production_urls 1`.

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
