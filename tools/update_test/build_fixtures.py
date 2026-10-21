#!/usr/bin/env python3
"""Build minimal release-shaped zips (payload = post-update test DLL)."""
from __future__ import annotations

import argparse
import os
import shutil
import sys
import tempfile
import zipfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
FIXTURE_FILES = os.path.join(os.path.dirname(__file__), "fixtures", "files")
DEFAULT_NEW_DLL = os.path.join(ROOT, "bin", "sof_buddy-universal-update-test-new.dll")
ZIP_NAMES = (
    "release_windows_universal.zip",
    "release_linux_wine_universal.zip",
    "release_windows.zip",
    "release_linux_wine.zip",
)


def stage_tree(staging: str, dll_path: str) -> None:
    shutil.copy2(dll_path, os.path.join(staging, "sof_buddy.dll"))
    buddy = os.path.join(staging, "sof_buddy")
    os.makedirs(os.path.join(buddy, "funcmaps"), exist_ok=True)
    os.makedirs(os.path.join(buddy, "update"), exist_ok=True)
    for name in os.listdir(os.path.join(ROOT, "rsrc", "funcmaps")):
        if name.endswith(".json"):
            shutil.copy2(
                os.path.join(ROOT, "rsrc", "funcmaps", name),
                os.path.join(buddy, "funcmaps", name),
            )
    for rel in (
        "rsrc/win_scripts/update_from_zip.cmd",
        "rsrc/lin_scripts/update_from_zip.sh",
        "rsrc/lin_scripts/patch_sof_binary.sh",
    ):
        src = os.path.join(ROOT, rel)
        if os.path.isfile(src):
            shutil.copy2(src, os.path.join(buddy, os.path.basename(src)))
    updater = os.path.join(ROOT, "bin", "sof_buddy_update.exe")
    if os.path.isfile(updater):
        shutil.copy2(updater, os.path.join(buddy, "sof_buddy_update.exe"))
    with open(os.path.join(buddy, "update", "TEST_UPDATE_PAYLOAD.txt"), "w", encoding="utf-8") as f:
        f.write("SoF Buddy update-test fixture payload (99.0.0-test DLL inside zip).\n")


def write_zip(staging: str, out_path: str) -> None:
    with zipfile.ZipFile(out_path, "w", zipfile.ZIP_DEFLATED) as zf:
        for dirpath, _dirnames, filenames in os.walk(staging):
            for name in filenames:
                full = os.path.join(dirpath, name)
                arc = os.path.relpath(full, staging).replace(os.sep, "/")
                zf.write(full, arc)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--dll", default=DEFAULT_NEW_DLL, help="Post-update DLL embedded as sof_buddy.dll")
    args = ap.parse_args()
    if not os.path.isfile(args.dll):
        print(f"error: DLL not found: {args.dll}", file=sys.stderr)
        print("Run: make update-test", file=sys.stderr)
        return 1
    os.makedirs(FIXTURE_FILES, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        stage_tree(tmp, args.dll)
        for name in ZIP_NAMES:
            out = os.path.join(FIXTURE_FILES, name)
            write_zip(tmp, out)
            print(f"wrote {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
