#!/usr/bin/env python3
"""Extract CHANGELOG.md section for VERSION (GitHub release / Discord announce)."""
import argparse
import re
import sys


def extract_section(text: str, version: str) -> str:
    pat = rf"^## v{re.escape(version)}\s*\n(.*?)(?=^## v|\Z)"
    m = re.search(pat, text, re.MULTILINE | re.DOTALL)
    return m.group(1).strip() if m else ""


def to_discord(body: str, product: str, version: str, build: str | None) -> str:
    lines: list[str] = []
    if build:
        lines.append(f"**{product} v{version}** (Build {build})")
    else:
        lines.append(f"**{product} v{version}**")
    lines.append("")
    for line in body.splitlines():
        s = line.rstrip()
        if s.startswith("### "):
            lines.append(f"**{s[4:].strip()}**")
        elif s.startswith("## "):
            continue
        else:
            lines.append(s)
    out = "\n".join(lines).strip()
    if len(out) > 1900:
        out = out[:1890].rstrip() + "\n… _(truncated)_"
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("version", nargs="?", help="MAJOR.MINOR (default: VERSION file)")
    ap.add_argument("changelog", nargs="?", default="CHANGELOG.md")
    ap.add_argument("--product", default="SoF Buddy", help="Title prefix (use sof2bud for SoF2)")
    ap.add_argument("--discord", action="store_true", help="Discord-friendly formatting")
    ap.add_argument("--build", help="CI build number for announce title")
    args = ap.parse_args()

    version = (args.version or "").strip()
    changelog = args.changelog
    if "sof2/CHANGELOG" in changelog.replace("\\", "/"):
        product = "sof2bud"
        version_file = "sof2/VERSION"
    else:
        product = args.product
        version_file = "VERSION"
    if not version:
        version = open(version_file, encoding="utf-8").read().strip()

    text = open(changelog, encoding="utf-8").read()
    body = extract_section(text, version)
    if not body:
        fallback = f"{product} v{version}. See {changelog} on GitHub for details."
        print(fallback)
        return 0

    if args.discord:
        print(to_discord(body, product, version, args.build))
    else:
        print(body)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
