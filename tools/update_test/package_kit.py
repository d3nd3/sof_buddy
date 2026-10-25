#!/usr/bin/env python3
"""Zip the update-test kit for copying to other machines."""
from __future__ import annotations

import argparse
import os
import zipfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = os.path.join(ROOT, "bin", "sof_buddy-update-test-kit.zip")

INCLUDE = (
    ("bin/sof_buddy-universal-update-test-old.dll", "stage-old/sof_buddy.dll"),
    ("bin/sof_buddy-universal-update-test-new.dll", "stage-new/sof_buddy.dll"),
    ("bin/sof_buddy-universal-update-test-old.dll", "sof_buddy-universal-update-test-old.dll"),
    ("bin/sof_buddy-universal-update-test-new.dll", "sof_buddy-universal-update-test-new.dll"),
    ("bin/sof_buddy_update.exe", "sof_buddy/sof_buddy_update.exe"),
    ("bin/sof_buddy_update.exe", "sof_buddy_update.exe"),
    ("rsrc/win_scripts/update_from_zip.cmd", "sof_buddy/update_from_zip.cmd"),
    ("rsrc/lin_scripts/update_from_zip.sh", "sof_buddy/update_from_zip.sh"),
    ("tools/update_test/README.md", "README.md"),
    ("tools/update_test/serve.py", "serve.py"),
    ("tools/update_test/build_fixtures.py", "build_fixtures.py"),
)


def add_tree(zf: zipfile.ZipFile, src_dir: str, arc_prefix: str) -> None:
    for dirpath, _dirnames, filenames in os.walk(src_dir):
        for name in filenames:
            full = os.path.join(dirpath, name)
            rel = os.path.relpath(full, src_dir).replace(os.sep, "/")
            zf.write(full, f"{arc_prefix}/{rel}")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("-o", "--output", default=OUT)
    args = ap.parse_args()
    fixtures = os.path.join(os.path.dirname(__file__), "fixtures")
    os.makedirs(os.path.dirname(args.output), exist_ok=True)
    with zipfile.ZipFile(args.output, "w", zipfile.ZIP_DEFLATED) as zf:
        for src_rel, arc in INCLUDE:
            path = os.path.join(ROOT, src_rel)
            if os.path.isfile(path):
                zf.write(path, arc)
        add_tree(zf, fixtures, "fixtures")
    print(args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
