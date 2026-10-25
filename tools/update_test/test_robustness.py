#!/usr/bin/env python3
"""Fixture + logic + optional Wine startup checks for updater/downgrade/stable UI."""
from __future__ import annotations

import json
import os
import re
import subprocess
import sys
import time
import urllib.error
import urllib.request
import zipfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
FIXTURES = os.path.join(os.path.dirname(__file__), "fixtures")
BASE_URL = os.environ.get("SOFBUDDY_UPDATE_TEST_URL", "http://127.0.0.1:8765")
OLD_DLL = os.path.join(ROOT, "bin", "sof_buddy-universal-update-test-old.dll")
NEW_DLL = os.path.join(ROOT, "bin", "sof_buddy-universal-update-test-new.dll")
UNIVERSAL_DLL = os.path.join(ROOT, "bin", "sof_buddy-universal.dll")
SOF_DIR = os.environ.get(
    "SOFBUDDY_SOF_DIR",
    os.path.expanduser("~/.wine/drive_c/users/dinda/Soldier of Fortune"),
)


def parse_version_parts(v: str) -> list[int]:
    parts: list[int] = []
    for piece in re.split(r"[.\-]", v):
        if not piece:
            continue
        n = 0
        for ch in piece:
            if ch.isdigit():
                n = n * 10 + int(ch)
            else:
                break
        parts.append(n)
    return parts or [0]


def compare_versions(local: str, remote: str) -> int:
    la, ra = parse_version_parts(local), parse_version_parts(remote)
    n = max(len(la), len(ra))
    for i in range(n):
        a = la[i] if i < len(la) else 0
        b = ra[i] if i < len(ra) else 0
        if a < b:
            return -1
        if a > b:
            return 1
    return 0


def list_tags_filtered(tags: list[str], stable_tag: str, show_all: bool) -> list[str]:
    out: list[str] = []
    for tag in tags:
        if not show_all:
            if not stable_tag or tag != stable_tag:
                continue
        out.append(tag)
    return out


def startup_effective_target(install_target: str, startup_stable_only: bool) -> str:
    if startup_stable_only:
        return "stable"
    return install_target or "stable"


def fetch_json(path: str) -> dict | list:
    url = f"{BASE_URL.rstrip('/')}/{path.lstrip('/')}"
    with urllib.request.urlopen(url, timeout=8) as resp:
        return json.loads(resp.read().decode("utf-8"))


def fail(msg: str) -> None:
    print(f"FAIL: {msg}", file=sys.stderr)
    raise SystemExit(1)


def ok(msg: str) -> None:
    print(f"ok: {msg}")


def test_http_feeds() -> tuple[str, str]:
    stable = fetch_json("/releases/stable.json")
    latest = fetch_json("/releases/latest.json")
    tags = fetch_json("/releases/list.json")
    if stable.get("tag_name") != "99.0.0-test":
        fail(f"stable.json tag unexpected: {stable.get('tag_name')}")
    if latest.get("tag_name") != "99.1.0-test":
        fail(f"latest.json tag unexpected: {latest.get('tag_name')}")
    names = [t["tag_name"] for t in tags]
    if "99.0.0-test" not in names or "99.1.0-test" not in names:
        fail(f"list.json missing fixture tags: {names}")
    ok(f"HTTP feeds stable={stable['tag_name']} latest={latest['tag_name']}")
    return stable["tag_name"], latest["tag_name"]


def test_logic(stable_tag: str, latest_tag: str) -> None:
    local_old, local_new = "0.0.0-test", "99.0.0-test"
    if compare_versions(local_old, stable_tag) >= 0:
        fail("old test DLL should be older than stable fixture")
    if compare_versions(local_new, stable_tag) != 0:
        fail("new test DLL should match stable fixture")
    if compare_versions(local_new, latest_tag) >= 0:
        fail("new test DLL should be older than latest fixture")

    all_tags = ["99.1.0-test", "99.0.0-test", "0.0.0-test"]
    filt = list_tags_filtered(all_tags, stable_tag, False)
    if filt != [stable_tag]:
        fail(f"filtered list wrong: {filt}")
    if list_tags_filtered(all_tags, "", False) != []:
        fail("empty stable_tag should list no version tags when show_all off")

    if startup_effective_target("latest", True) != "stable":
        fail("startup stable-only should ignore latest install target")
    if startup_effective_target("latest", False) != "latest":
        fail("startup should use install target when stable-only off")
    ok("version compare + list filter + startup target logic")


def test_fixture_zips() -> None:
    zip_path = os.path.join(FIXTURES, "files", "release_windows_universal.zip")
    if not os.path.isfile(zip_path):
        fail(f"missing {zip_path} (run: make update-test-fixtures)")
    with zipfile.ZipFile(zip_path) as zf:
        blob = zf.read("sof_buddy.dll")
    if b"99.0.0-test" not in blob:
        fail("fixture zip DLL is not 99.0.0-test post-update build")
    ok("fixture zip embeds post-update test DLL")


def test_by_tag_feeds() -> None:
    for tag in ("99.0.0-test", "99.1.0-test"):
        doc = fetch_json(f"/releases/by_tag/{tag}.json")
        if doc.get("tag_name") != tag:
            fail(f"by_tag/{tag}.json mismatch")
    ok("per-tag fixture JSON")


def wine_probe(sof_dir: str) -> bool:
    sof_exe = os.path.join(sof_dir, "SoF.exe")
    if not os.path.isfile(sof_exe):
        return False
    try:
        r = subprocess.run(
            ["wine", sof_exe, "-h"],
            cwd=sof_dir,
            capture_output=True,
            timeout=12,
            env={**os.environ, "WINEDEBUG": "-all"},
        )
    except (subprocess.TimeoutExpired, FileNotFoundError):
        return False
    err = (r.stderr or b"").decode("utf-8", errors="replace")
    if "invalid frame" in err or "dbus" in err.lower():
        return False
    return r.returncode in (0, 1)


def test_artifacts() -> None:
    for path in (OLD_DLL, NEW_DLL, os.path.join(ROOT, "bin", "sof_buddy_update.exe")):
        if not os.path.isfile(path):
            fail(f"missing {path} (run: make update-test)")
    blob = open(UNIVERSAL_DLL, "rb").read() if os.path.isfile(UNIVERSAL_DLL) else b""
    for needle in (
        b"_sofbuddy_update_startup_stable_only",
        b"_sofbuddy_update_show_all_releases",
        b"update_downgrade",
        b"Install stable release",
    ):
        if blob and needle not in blob:
            fail(f"universal DLL missing {needle!r} (run: make universal)")
    ok("test DLLs + universal strings")


def read_pending_tag(sof_dir: str) -> str | None:
    path = os.path.join(sof_dir, "sof_buddy", "update", "pending_update.txt")
    if not os.path.isfile(path):
        return None
    tag = None
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            if line.startswith("tag="):
                tag = line.strip().split("=", 1)[1]
    return tag


def sof_cfg_paths(sof_dir: str) -> list[str]:
    """Single cfg path — never create a lowercase base/ on Linux (shadows Base/ paks)."""
    base_cap = os.path.join(sof_dir, "Base", "sofbuddy.cfg")
    base_low = os.path.join(sof_dir, "base", "sofbuddy.cfg")
    if os.path.isfile(base_cap) or os.path.isdir(os.path.join(sof_dir, "Base")):
        return [base_cap]
    if os.path.isfile(base_low):
        return [base_low]
    return [base_cap]


def wine_startup_manifest(
    dll_path: str,
    extra_sets: dict[str, str],
    expect_tag: str,
    label: str,
) -> None:
    if not os.path.isdir(SOF_DIR):
        fail(f"Wine SoF dir missing: {SOF_DIR}")
    sof_exe = os.path.join(SOF_DIR, "SoF.exe")
    if not os.path.isfile(sof_exe):
        fail(f"SoF.exe not found under {SOF_DIR}")

    cfg_paths = sof_cfg_paths(SOF_DIR)
    cfg_path = next((p for p in cfg_paths if os.path.isfile(p)), cfg_paths[0])
    dll_dest = os.path.join(SOF_DIR, "sof_buddy.dll")
    pending = os.path.join(SOF_DIR, "sof_buddy", "update", "pending_update.txt")
    cfg_backup = cfg_path + ".robustness_bak"
    dll_backup = dll_dest + ".robustness_bak"

    if os.path.isfile(cfg_path):
        subprocess.check_call(["cp", "-f", cfg_path, cfg_backup])
    if os.path.isfile(dll_dest):
        subprocess.check_call(["cp", "-f", dll_dest, dll_backup])

    lines = ["// sof_buddy robustness test (auto)"]
    lines.append('set _sofbuddy_feature_internal_menus "1"')
    lines.append('set _sofbuddy_update_check_startup "1"')
    for k, v in extra_sets.items():
        lines.append(f'set {k} "{v}"')
    cfg_body = "\n".join(lines) + "\n"
    os.makedirs(os.path.dirname(cfg_path), exist_ok=True)
    with open(cfg_path, "w", encoding="utf-8") as f:
        f.write(cfg_body)

    subprocess.check_call(["cp", "-f", dll_path, dll_dest])
    if os.path.isfile(pending):
        os.remove(pending)
    if os.path.isfile(pending):
        fail(f"could not remove stale {pending}")

    subprocess.run(["pkill", "-f", "SoF.exe"], check=False)
    time.sleep(1)
    env = os.environ.copy()
    env["WINEDEBUG"] = "-all"
    proc = subprocess.Popen(
        ["wine", sof_exe],
        cwd=SOF_DIR,
        env=env,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    deadline = time.time() + 45
    tag = None
    try:
        while time.time() < deadline:
            if os.path.isfile(pending):
                tag = read_pending_tag(SOF_DIR)
                if tag:
                    break
            time.sleep(0.5)
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=8)
        except subprocess.TimeoutExpired:
            proc.kill()
        subprocess.run(["pkill", "-f", "SoF.exe"], check=False)
        if os.path.isfile(cfg_backup):
            subprocess.check_call(["cp", "-f", cfg_backup, cfg_path])
            os.remove(cfg_backup)
            for p in cfg_paths:
                if p != cfg_path and os.path.isfile(p):
                    os.remove(p)
        if os.path.isfile(dll_backup):
            subprocess.check_call(["mv", "-f", dll_backup, dll_dest])

    if tag != expect_tag:
        fail(f"{label}: pending tag={tag!r} expected {expect_tag!r}")
    ok(f"Wine startup {label} -> tag={tag}")


def test_wine(stable_tag: str, latest_tag: str) -> None:
    if not wine_probe(SOF_DIR):
        print("skip: Wine SoF probe failed (run manually on a machine where SoF.exe starts)")
        return
    wine_startup_manifest(
        OLD_DLL,
        {
            "_sofbuddy_update_target_tag": "latest",
            "_sofbuddy_update_startup_stable_only": "1",
        },
        stable_tag,
        "old DLL + install=latest + startup stable-only",
    )
    wine_startup_manifest(
        OLD_DLL,
        {
            "_sofbuddy_update_target_tag": "latest",
            "_sofbuddy_update_startup_stable_only": "0",
        },
        latest_tag,
        "old DLL + install=latest + startup follows latest",
    )
    wine_startup_manifest(
        NEW_DLL,
        {
            "_sofbuddy_update_target_tag": "latest",
            "_sofbuddy_update_startup_stable_only": "1",
        },
        stable_tag,
        "new DLL on stable + startup stable-only (no latest nag)",
    )


def main() -> int:
    if "--no-wine" in sys.argv:
        wine = False
    else:
        wine = os.environ.get("SOFBUDDY_UPDATE_TEST_NO_WINE", "") != "1"
    try:
        stable_tag, latest_tag = test_http_feeds()
    except (urllib.error.URLError, TimeoutError) as e:
        fail(f"fixture server not reachable at {BASE_URL}: {e}")
    test_logic(stable_tag, latest_tag)
    test_by_tag_feeds()
    test_fixture_zips()
    test_artifacts()
    subprocess.check_call([sys.executable, os.path.join(os.path.dirname(__file__), "verify_updater.py")])
    if wine:
        test_wine(stable_tag, latest_tag)
    else:
        print("skip: Wine startup tests (--no-wine or SOFBUDDY_UPDATE_TEST_NO_WINE=1)")
    print("test_robustness: all passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
