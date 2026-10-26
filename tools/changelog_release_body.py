#!/usr/bin/env python3
"""Print CHANGELOG.md section for VERSION (e.g. 8.38) for GitHub release body."""
import re
import sys


def main() -> int:
    version = (sys.argv[1] if len(sys.argv) > 1 else "").strip()
    changelog_path = sys.argv[2] if len(sys.argv) > 2 else "CHANGELOG.md"
    if not version:
        version = open("VERSION", encoding="utf-8").read().strip()
    text = open(changelog_path, encoding="utf-8").read()
    pat = rf"^## v{re.escape(version)}\s*\n(.*?)(?=^## v|\Z)"
    m = re.search(pat, text, re.MULTILINE | re.DOTALL)
    if m:
        print(m.group(1).strip())
    else:
        print(f"SoF Buddy v{version}. See CHANGELOG.md on GitHub for details.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
