#!/usr/bin/env python3
"""Sanity-check updater binaries/scripts before packaging the test kit."""
from __future__ import annotations

import os
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
EXE = os.path.join(ROOT, "bin", "sof_buddy_update.exe")
CMD = os.path.join(ROOT, "rsrc", "win_scripts", "update_from_zip.cmd")


def main() -> int:
    err = 0
    if not os.path.isfile(EXE):
        print(f"error: missing {EXE} (run: make win-updater)", file=sys.stderr)
        err = 1
    else:
        out = subprocess.check_output(
            ["i686-w64-mingw32-objdump", "-p", EXE], text=True, errors="replace"
        )
        if "libwinpthread-1.dll" in out:
            print(
                "error: sof_buddy_update.exe imports libwinpthread (Wine needs static build)",
                file=sys.stderr,
            )
            err = 1
    if not os.path.isfile(CMD):
        print(f"error: missing {CMD}", file=sys.stderr)
        err = 1
    else:
        text = open(CMD, encoding="utf-8", errors="replace").read()
        if '>"%PROGRESS_FILE%" echo %~1|%~2' in text:
            print(
                "error: update_from_zip.cmd report_progress must escape | (breaks GUI silent install)",
                file=sys.stderr,
            )
            err = 1
        if "(echo %~1^|%~2)" not in text:
            print("error: update_from_zip.cmd missing fixed report_progress line", file=sys.stderr)
            err = 1
        if "start /wait /unix /usr/bin/unzip" not in text:
            print("error: update_from_zip.cmd missing Wine /unix unzip path", file=sys.stderr)
            err = 1
    if err:
        return 1
    print("verify_updater: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
