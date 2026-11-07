#!/usr/bin/env python3
"""Sanity-check updater binaries/scripts before packaging the test kit."""
from __future__ import annotations

import os
import re
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
EXE = os.path.join(ROOT, "bin", "sof_buddy_update.exe")
CMD = os.path.join(ROOT, "rsrc", "win_scripts", "update_from_zip.cmd")
CMD_INSTALLED = os.path.join(ROOT, "sof_buddy", "update_from_zip.cmd")


def check_cmd(path: str, err: list[str]) -> None:
    data = open(path, "rb").read()
    text = data.decode("utf-8", errors="replace")
    if b"\r\n" not in data or re.search(rb"(?<!\r)\n", data):
        err.append(f"{path} must use CRLF line endings (cmd.exe corrupts LF batch files)")
    for needle in (
        "SOFBUDDY_UPDATER_REEXEC",
        ":wait_host_extract",
        ".extract_ok",
        ":detect_universal",
        "SOFBUDDY_IS_UNIVERSAL",
        "--delegated",
        'set /p "FRESH=Start with fresh settings (y/N): "',
        'set /p "REPLACE_FEATURES=Update feature settings to this release\'s defaults? (Y/n): "',
        'set /p "RESET_FEATURES=Expecting a new feature? Reset feature toggles (y/N): "',
    ):
        if needle not in text:
            err.append(f"{path} missing {needle!r}")
    if "(Y/n^)" in text or "(y/N^)" in text:
        err.append(f"{path} has a literal ^ in a set /p prompt")


def main() -> int:
    err: list[str] = []
    if not os.path.isfile(EXE):
        print(f"error: missing {EXE} (run: make win-updater)", file=sys.stderr)
        err.append("missing exe")
    else:
        out = subprocess.check_output(
            ["i686-w64-mingw32-objdump", "-p", EXE], text=True, errors="replace"
        )
        if "libwinpthread-1.dll" in out:
            print(
                "error: sof_buddy_update.exe imports libwinpthread (Wine needs static build)",
                file=sys.stderr,
            )
            err.append("non-static exe")
    if not os.path.isfile(CMD):
        print(f"error: missing {CMD}", file=sys.stderr)
        err.append("missing cmd")
    else:
        text = open(CMD, encoding="utf-8", errors="replace").read()
        if '>"%PROGRESS_FILE%" echo %~1|%~2' in text:
            print(
                "error: update_from_zip.cmd report_progress must escape | (breaks GUI silent install)",
                file=sys.stderr,
            )
            err.append("unescaped progress pipe")
        if "(echo %~1^|%~2)" not in text:
            print("error: update_from_zip.cmd missing fixed report_progress line", file=sys.stderr)
            err.append("missing fixed progress line")
        if "start /wait /unix /usr/bin/unzip" not in text:
            print("error: update_from_zip.cmd missing Wine /unix unzip path", file=sys.stderr)
            err.append("missing unix unzip")
        if "SOFBUDDY_UPDATER_REPLACE_FEATURES" not in text or ":prepare_features" not in text:
            print("error: update_from_zip.cmd missing feature-settings replacement path", file=sys.stderr)
            err.append("missing feature path")
        check_cmd(CMD, err)
        if os.path.isfile(CMD_INSTALLED):
            if open(CMD_INSTALLED, "rb").read() != open(CMD, "rb").read():
                print(
                    f"error: {CMD_INSTALLED} is out of sync with {CMD}",
                    file=sys.stderr,
                )
                err.append("installed cmd out of sync")
        else:
            print(f"error: missing {CMD_INSTALLED}", file=sys.stderr)
            err.append("missing installed cmd")

    shell = os.path.join(ROOT, "rsrc", "lin_scripts", "update_from_zip.sh")
    if not os.path.isfile(shell):
        print(f"error: missing {shell}", file=sys.stderr)
        err.append("missing sh")
    else:
        text = open(shell, encoding="utf-8", errors="replace").read()
        for needle in (
            "SOFBUDDY_UPDATER_REPLACE_FEATURES",
            "SOFBUDDY_UPDATE_SH_REEXEC",
            "SOFBUDDY_IS_UNIVERSAL",
            "--delegated",
            ".extract_ok",
            "flavor.txt",
        ):
            if needle not in text:
                print(f"error: update_from_zip.sh missing {needle!r}", file=sys.stderr)
                err.append(f"sh missing {needle}")
    if err:
        return 1
    print("verify_updater: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
