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
   Copy the kit’s **`sof_buddy/`** folder (or at least **`sof_buddy_update.exe`**, **`update_from_zip.cmd`**, **`update_from_zip.sh`**) into your game tree. On Wine, use the **`bin/sof_buddy_update.exe`** from `make win-updater` (static build)—see [DEBUGGING_UPDATER.md](../../DEBUGGING_UPDATER.md#wine--proton-linux).
4. **Launch SoF** — startup check should offer **99.0.0-test** (fixture, not cloud).
5. On the startup prompt, choose **Download + install (Y)** — one step: download, then quit; **`winstart`** runs `update_from_zip.cmd` / GUI updater.
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
| Stable vs latest channels | Default target **stable** → `stable.json` (99.0.0-test); **latest** → `latest.json` (99.1.0-test) |
| Long release notes in prompt | `set _sofbuddy_update_target_tag 99.1.0-test` then `sofbuddy_update prompt` |
| Release list UI | `sofbuddy_update releases` |
| Disable startup check | `_sofbuddy_update_check_startup 0` |
| Graphical updater (progress bar) | `sof_buddy/sof_buddy_update.exe` required when `_sofbuddy_update_use_gui` is 1 (default on universal) |

## Verify updater artifacts

```bash
make update-test-verify
```

Checks static `sof_buddy_update.exe` and Wine-safe `update_from_zip.cmd`.

## Linux / Wine: `Base` vs `base`

Game data lives in **`Base/`** (pak files). Do **not** create a separate lowercase **`base/`** directory on a case-sensitive filesystem (typical Linux Wine prefixes). An empty `base/` makes SoF fail with *Could not open string package 'str_server'*. The robustness script writes only `Base/sofbuddy.cfg`.

## Robustness suite

```bash
make update-test-serve   # in another terminal
make update-test-robustness
```

HTTP fixtures, version/list-filter logic, zip payload, updater binary checks, and (when `SoF.exe` starts under Wine) startup manifest tests for **startup stable-only** vs **install target = latest**.

## Rebuild fixture zips after code changes

```bash
make update-test-fixtures
```
