#!/usr/bin/env python3
"""Serve update-test JSON and zip fixtures (local QA only)."""
from __future__ import annotations

import argparse
import os
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer

ROOT = os.path.dirname(os.path.abspath(__file__))
FIXTURES = os.path.join(ROOT, "fixtures")


class Handler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=FIXTURES, **kwargs)

    def log_message(self, fmt: str, *args) -> None:
        print(f"[update-test] {self.address_string()} {fmt % args}")


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=8765)
    args = ap.parse_args()
    httpd = ThreadingHTTPServer((args.host, args.port), Handler)
    print(f"Stable feed:  http://{args.host}:{args.port}/releases/stable.json")
    print(f"Latest feed:  http://{args.host}:{args.port}/releases/latest.json")
    print(f"Files root:   {FIXTURES}")
    print("Ctrl+C to stop.")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nStopped.")


if __name__ == "__main__":
    main()
