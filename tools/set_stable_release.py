#!/usr/bin/env python3
"""Point stable.json at an existing GitHub release tag (does not create a GitHub release)."""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import tempfile
import urllib.error
import urllib.request

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
REPO = "d3nd3/sof_buddy"
DEFAULT_REPO_STABLE = os.path.join(ROOT, "releases", "stable.json")
PREFERRED_ZIPS = (
    "release_windows_universal.zip",
    "release_linux_wine_universal.zip",
    "release_windows.zip",
    "release_linux_wine.zip",
    "release_windows_xp.zip",
)


def gh_release(tag: str) -> dict:
    try:
        out = subprocess.check_output(
            ["gh", "api", f"repos/{REPO}/releases/tags/{tag}"],
            text=True,
        )
        return json.loads(out)
    except FileNotFoundError:
        url = f"https://api.github.com/repos/{REPO}/releases/tags/{tag}"
        req = urllib.request.Request(url, headers={"Accept": "application/vnd.github+json"})
        with urllib.request.urlopen(req, timeout=60) as resp:
            return json.loads(resp.read().decode())
    except subprocess.CalledProcessError as e:
        raise SystemExit(f"GitHub release not found for tag {tag}: {e}") from e
    except urllib.error.HTTPError as e:
        raise SystemExit(f"GitHub release not found for tag {tag}: HTTP {e.code}") from e


def pick_assets(release: dict) -> list[dict]:
    assets = []
    by_name = {a.get("name", ""): a.get("browser_download_url", "") for a in release.get("assets") or []}
    for name in PREFERRED_ZIPS:
        url = by_name.get(name)
        if url:
            assets.append({"browser_download_url": url})
    if not assets:
        for a in release.get("assets") or []:
            url = a.get("browser_download_url")
            if url and str(url).lower().endswith(".zip"):
                assets.append({"browser_download_url": url})
    return assets


def build_manifest(release: dict, zip_url: str | None = None) -> dict:
    assets = pick_assets(release)
    primary = zip_url or (assets[0]["browser_download_url"] if assets else "")
    return {
        "tag_name": release.get("tag_name") or "",
        "name": release.get("name") or release.get("tag_name") or "",
        "html_url": release.get("html_url") or f"https://github.com/{REPO}/releases/tag/{release.get('tag_name', '')}",
        "body": release.get("body") or "",
        "zip_url": primary,
        "assets": assets,
        "stable": True,
    }


def atomic_write(path: str, data: dict) -> None:
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    fd, tmp = tempfile.mkstemp(dir=os.path.dirname(path) or ".", prefix=".stable-", suffix=".json")
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=2)
            f.write("\n")
        os.replace(tmp, path)
    finally:
        if os.path.exists(tmp):
            os.remove(tmp)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("tag", help="Existing GitHub release tag (e.g. v8.44-build252)")
    ap.add_argument(
        "--repo-stable",
        default=DEFAULT_REPO_STABLE,
        help="Write releases/stable.json in git (raw.githubusercontent feed)",
    )
    ap.add_argument(
        "--sofvault-root",
        help="Also write stable.json under sofvault mirror root (alongside latest.json)",
    )
    ap.add_argument(
        "--fixture-releases",
        default=os.path.join(ROOT, "tools", "update_test", "fixtures", "releases"),
        help="Also write stable.json for local update-test serve.py",
    )
    ap.add_argument(
        "--zip-url",
        help="Override zip_url (e.g. sofvault mirror URL for XP)",
    )
    args = ap.parse_args()

    release = gh_release(args.tag)
    manifest = build_manifest(release, args.zip_url)

    atomic_write(args.repo_stable, manifest)
    print(f"wrote {args.repo_stable} -> {manifest['tag_name']}")

    if args.fixture_releases:
        fix = os.path.abspath(args.fixture_releases)
        test_fix = os.path.abspath(os.path.join(ROOT, "tools", "update_test", "fixtures", "releases"))
        if fix != test_fix:
            atomic_write(os.path.join(fix, "stable.json"), manifest)
            print(f"wrote {fix}/stable.json")

    if args.sofvault_root:
        root = os.path.abspath(args.sofvault_root)
        atomic_write(os.path.join(root, "stable.json"), manifest)
        with open(os.path.join(root, "stable"), "w", encoding="utf-8") as f:
            f.write(manifest["tag_name"] + "\n")
        print(f"wrote {root}/stable.json")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
