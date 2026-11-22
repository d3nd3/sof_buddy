#!/usr/bin/env python3
"""Generate Windows VERSIONINFO resources for SoF Buddy binaries.

Keeping the resource metadata in one generator avoids hand-maintained version
numbers in .rc files and gives both the game DLL and the updater a normal
Windows version resource. Microsoft Defender's ML heuristics treat unsigned
MinGW binaries with no version resource as more suspicious than ordinary
signed/versioned applications.
"""

import argparse
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

KINDS = {
    "dll": {
        "file_type": "0x2L",  # VFT_DLL
        "description": "SoF Buddy game module",
        "internal_name": "sof_buddy",
        "original_filename": "sof_buddy.dll",
        "manifest": None,
        "icon": None,
    },
    "exe": {
        "file_type": "0x1L",  # VFT_APP
        "description": "SoF Buddy updater",
        "internal_name": "sof_buddy_update",
        "original_filename": "sof_buddy_update.exe",
        "manifest": "sof_buddy_update.manifest",
        "icon": "sof_buddy.ico",
    },
}


def version_parts(version):
    """Return the four 16-bit FILEVERSION components for a MAJOR.MINOR version."""
    parts = version.replace(" ", "").split(".")
    if len(parts) < 2 or not all(part.isdigit() for part in parts[:2]):
        raise ValueError(f"invalid VERSION {version!r}; expected MAJOR.MINOR")
    major, minor = (int(part) for part in parts[:2])
    if not (0 <= major <= 0xFFFF and 0 <= minor <= 0xFFFF):
        raise ValueError(f"VERSION {version!r} does not fit a Windows version resource")
    return major, minor, 0, 0


def string_value(name, value):
    escaped = value.replace("\\", "\\\\").replace('"', '\\"')
    return f'      VALUE "{name}", "{escaped}\\0"'


def generate(kind, version, build):
    info = KINDS[kind]
    file_version = ", ".join(str(part) for part in version_parts(version))
    text_version = f"{version}.0.0"
    comments = "Open-source SoF mod: https://github.com/d3nd3/sof_buddy"
    if build and build != "local":
        comments = f"{comments} (build {build})"

    lines = [
        "// Generated from VERSION. Do not edit by hand.",
        "LANGUAGE 0x09, 0x01",
        "",
    ]
    if info["icon"]:
        lines += [
            f'1 ICON "{info["icon"]}"',
            "",
        ]
    if info["manifest"]:
        lines += [
            f'1 24 "{info["manifest"]}"',
            "",
        ]
    lines += [
        "1 VERSIONINFO",
        f"FILEVERSION {file_version}",
        f"PRODUCTVERSION {file_version}",
        "FILEFLAGSMASK 0x3fL",
        "FILEFLAGS 0x0L",
        "FILEOS 0x40004L",
        f'FILETYPE {info["file_type"]}',
        "FILESUBTYPE 0x0L",
        "BEGIN",
        '  BLOCK "StringFileInfo"',
        "  BEGIN",
        '    BLOCK "040904b0"',
        "    BEGIN",
        string_value("CompanyName", "d3nd3"),
        string_value("FileDescription", info["description"]),
        string_value("FileVersion", text_version),
        string_value("InternalName", info["internal_name"]),
        string_value("LegalCopyright", "Copyright (C) 2024 d3nd3"),
        string_value("OriginalFilename", info["original_filename"]),
        string_value("ProductName", "SoF Buddy"),
        string_value("ProductVersion", text_version),
        string_value("Comments", comments),
        "    END",
        "  END",
        '  BLOCK "VarFileInfo"',
        "  BEGIN",
        '    VALUE "Translation", 0x409, 1200',
        "  END",
        "END",
        "",
    ]
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kind", choices=sorted(KINDS), required=True)
    parser.add_argument("--version-file", type=Path, default=ROOT / "VERSION")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--build",
        default=os.environ.get("SOFBUDDY_BUILD_NUMBER", "local"),
        help="build number for the resource comments (default: SOFBUDDY_BUILD_NUMBER)",
    )
    args = parser.parse_args()

    version = args.version_file.read_text(encoding="utf-8").strip()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(generate(args.kind, version, args.build.strip()), encoding="utf-8")


if __name__ == "__main__":
    main()
