# SoF Buddy — local update workflow test kit

Closed loop: **no GitHub/SofVault**. The game only talks to your **fixture server**; the “update” zip contains the **next test DLL** (`99.0.0-test`) so you can run the full **quit → install → relaunch** cycle.

## Artifacts (`make update-test`)

| File | Role |
|------|------|
| `sof_buddy-universal-update-test-old.dll` | **Start here** — version `0.0.0-test`, install as `sof_buddy.dll` |
| `sof_buddy-universal-update-test-new.dll` | **After update** — version `99.0.0-test`, packed inside fixture zips |
| `sof_buddy-update-test-kit.zip` | Copy to Windows / Linux / Proton test machines |
| Fixture zips under `fixtures/files/` | What the fake “release” serves |

Production URLs are **blocked** unless `_sofbuddy_update_allow_production_urls 1`.

## Full restart cycle (what you are testing)

1. **Build** (on a machine with mingw): `make update-test`
2. **Serve fixtures** (same host as Wine, or LAN): `make update-test-serve`  
   → `http://127.0.0.1:8765/releases/latest.json`
3. **Install OLD dll** as `sof_buddy.dll` in a throwaway SoF tree (back up your real DLL).
4. **Launch SoF** — startup check should offer **99.0.0-test** (fixture, not cloud).
5. **`sofbuddy_update download`** (or use the startup prompt).
6. **`sofbuddy_update_install`** (or install from the menu) — game should **quit**; engine **`winstart`** runs `update_from_zip.cmd` / GUI updater.
7. **Relaunch SoF** — you should now be on **`99.0.0-test`** (F12 → Version / `_sp_cl_info_sofbuddy` shows `update-test-99.0.0-test-…`).
8. Confirm **`sof_buddy/update/TEST_UPDATE_PAYLOAD.txt`** exists.
9. Second launch: startup check should be **up to date** (same fixture tag as installed DLL).

That is the real production path: download → exit → external installer → user starts game again.

## Remote fixture host

If SoF runs on another PC, copy the kit zip, run `python3 serve.py` on the server, and on the game PC:

```text
set _sofbuddy_update_api_url http://<server-ip>:8765/releases/latest.json
set _sofbuddy_update_releases_url http://<server-ip>:8765/releases/page
```

Private LAN IPs are allowed; `github.com` / `sofvault.org` are not.

## Extra scenarios

| Goal | Command / setting |
|------|-------------------|
| Long release notes in prompt | `set _sofbuddy_update_target_tag 99.1.0-test` then `sofbuddy_update prompt` |
| Release list UI | `sofbuddy_update releases` |
| Disable startup check | `_sofbuddy_update_check_startup 0` |

## Rebuild fixture zips after code changes

```bash
make update-test-fixtures
```
