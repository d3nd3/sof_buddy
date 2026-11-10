#!/usr/bin/env python3
"""Preview the Scaled Menu RMF transformations and parsed values.

This tool mimics the menu paths used by the Scaled Menu feature:

  * fs_loadfile_override.cpp:
      - inject_outer_16_9_wrapper()
      - scale_bghoul_for_wide_aspect()
  * menu.cpp:
      - frame_c ctor: authored W/H * menu canvas
      - hk_frame_link(): authored H * vertical_ratio / canvas
      - ghoul/bghoul parse: authored scale/yoff * canvas

It prints:
  1. the RMF that the FS hook serves,
  2. the actual frame/bghoul values the engine parses,
  3. an inspection RMF with the actual frame/bghoul values substituted.

The inspection RMF is not meant to be re-fed to the engine: feeding actual
values back would scale them again. It is only for reading/debugging.

Usage:
  python3 tools/rmf_scaled_menu_preview.py INPUT.rmf [--width 1920] [--height 1080]
                                                   [--no-chrome] [--no-bghoul]
"""

from __future__ import annotations

import argparse
import math
import re
import sys
from pathlib import Path


FRAME_RE = re.compile(r"<frame\s+([^\s>]+)\s+(-?\d+)\s+(-?\d+)([^>]*)>")
BGHOUL_RE = re.compile(r"<bghoul\b[^>]*>")


def lround(value: float) -> int:
    """C lround(): round half away from zero."""
    if value >= 0:
        return int(math.floor(value + 0.5))
    return int(math.ceil(value - 0.5))


def token_int(attrs: str, name: str, default: int = 0) -> int:
    match = re.search(rf"(?<!\S){re.escape(name)}\s+(-?\d+)", attrs)
    return int(match.group(1)) if match else default


def token_float(attrs: str, name: str, default: float = 0.0) -> float:
    match = re.search(rf"(?<!\S){re.escape(name)}\s+(-?\d+(?:\.\d+)?)", attrs)
    return float(match.group(1)) if match else default


def token_word(attrs: str, name: str) -> str | None:
    match = re.search(rf"(?<!\S){re.escape(name)}\s+([^\s>]+)", attrs)
    return match.group(1) if match else None


def replace_token_number(text: str, key: str, new_value: str) -> tuple[str, bool]:
    """Replace the first numeric value after a keyword, preserving the rest."""
    match = re.search(rf"(?<!\S){re.escape(key)}\s+([-+]?\d+(?:\.\d+)?)", text)
    if not match:
        return text, False
    return text[: match.start(1)] + new_value + text[match.end(1) :], True


def scale_bghoul_for_wide_aspect(content: str, factor: float) -> tuple[str, bool]:
    """Mimic scale_bghoul_for_wide_aspect() from fs_loadfile_override.cpp."""
    if not (factor > 0.0) or abs(factor - 1.0) < 0.001:
        return content, False

    changed = False
    pos = 0
    while True:
        pos = content.find("<bghoul ", pos)
        if pos < 0:
            break
        end = content.find(">", pos)
        if end < 0:
            break
        line = content[pos : end + 1]
        line_changed = False
        for key, is_float in (("xoff", False), ("yoff", False), ("scale", True)):
            match = re.search(rf"(?<!\S){re.escape(key)}\s+([-+]?\d+(?:\.\d+)?)", line)
            if not match:
                continue
            value = float(match.group(1)) * factor
            new_text = f"{value:.2f}" if is_float else str(lround(value))
            line = line[: match.start(1)] + new_text + line[match.end(1) :]
            line_changed = True
        if line_changed:
            content = content[:pos] + line + content[end + 1 :]
            pos += len(line)
            changed = True
        else:
            pos = end + 1
    return content, changed


def wide_main_width(screen_w: int, screen_h: int) -> int:
    if screen_w <= 0 or screen_h <= 0:
        return 480
    return max(1, min(640, lround(((screen_h * 4.0 / 3.0) / screen_w) * 640.0)))


def inject_outer_16_9_wrapper(content: str, screen_w: int, screen_h: int) -> tuple[str, bool]:
    """Mimic inject_outer_16_9_wrapper() from menu_rmf_transform.cpp."""
    tag = "<exinclude _spf_cl_video_shortscreen "
    pos = content.find(tag)
    if pos < 0:
        return content, False
    end = content.find(">", pos)
    if end < 0:
        return content, False

    rest = content[pos + len(tag) : end]
    parts = rest.split()
    if len(parts) < 2:
        return content, False
    ss_name, ns_name = parts[0], parts[1]
    stem = ss_name.rsplit("/", 1)[-1]
    if stem.endswith(".rmf"):
        stem = stem[:-4]
    if stem.endswith("_ss"):
        stem = stem[:-3]

    whitelist = {
        "join", "keys", "game", "lock", "bots", "chat", "load", "mail", "misc",
        "motd", "save", "audio", "stats", "game2", "game3", "scale", "socks",
        "start", "video", "outfit", "advjoin", "credits", "dmflags", "options",
        "pickbot", "joystick", "objectives", "testmotd", "language", "player",
    }
    if stem not in whitelist:
        return content, False

    ns_page = ns_name[:-4] if ns_name.endswith(".rmf") else ns_name
    main_w = wide_main_width(screen_w, screen_h)
    side_w = (640 - main_w) // 2
    block = (
        "<frame 16_9_center 0 0>\n"
        f"<frame 16_9_left {side_w} 0 page 16_9_l cut 16_9_center>\n"
        f"<frame ss_content {main_w} 0 page {ns_page} cut 16_9_center>\n"
        f"<frame 16_9_right {side_w} 0 page 16_9_r cut 16_9_center>"
    )
    content = content[:pos] + block + content[end + 1 :]
    return content, True


class Frame:
    def __init__(self, match: re.Match[str]):
        self.name = match.group(1)
        self.authored_w = int(match.group(2))
        self.authored_h = int(match.group(3))
        self.attrs = match.group(4)
        self.cut = token_word(self.attrs, "cut")
        self.border_pad = token_int(self.attrs, "border", 0)
        self.actual_w = 0
        self.actual_h = 0
        self.virtual_h = 0.0


def parse_frames(content: str) -> list[Frame]:
    return [Frame(match) for match in FRAME_RE.finditer(content)]


def compute_frame_actuals(
    frames: list[Frame],
    canvas: float,
    target: float,
    screen_w: int,
    screen_h: int,
    ss_content: bool = False,
) -> None:
    by_name = {frame.name: frame for frame in frames}
    resolved: set[str] = set()

    def resolve(frame: Frame, stack: set[str]) -> tuple[int, int]:
        if frame.name in resolved:
            return frame.actual_w, frame.actual_h
        if frame.name in stack:
            return frame.actual_w, frame.actual_h
        stack.add(frame.name)

        if frame.authored_w > 0:
            shrink = ss_content and not (
                frame.name.startswith("16_9_") or frame.name == "ss_content"
            )
            frame.actual_w = (
                lround(frame.authored_w * target)
                if shrink
                else int(frame.authored_w * canvas)
            )
        elif frame.cut and frame.cut in by_name:
            parent = by_name[frame.cut]
            parent_w, _ = resolve(parent, stack)
            frame.actual_w = parent_w - 2 * lround(parent.border_pad * target)
        else:
            frame.actual_w = screen_w

        if frame.authored_h > 0:
            frame.actual_h = lround(frame.authored_h * target)
        elif frame.cut and frame.cut in by_name:
            parent = by_name[frame.cut]
            _, parent_h = resolve(parent, stack)
            frame.actual_h = parent_h - 2 * lround(parent.border_pad * target)
        else:
            frame.actual_h = 0

        frame.actual_w = max(0, frame.actual_w)
        frame.actual_h = max(0, frame.actual_h)
        frame.virtual_h = frame.actual_h / canvas if canvas > 1e-6 else 0.0
        resolved.add(frame.name)
        stack.remove(frame.name)
        return frame.actual_w, frame.actual_h

    for frame in frames:
        resolve(frame, set())

    # Frames cut to the same parent and authored with W=0 fill the remaining width.
    groups: dict[str, list[Frame]] = {}
    for frame in frames:
        if frame.cut and frame.authored_w == 0:
            groups.setdefault(frame.cut, []).append(frame)
    for parent_name, members in groups.items():
        parent = by_name.get(parent_name)
        if not parent:
            continue
        used = sum(
            frame.actual_w
            for frame in frames
            if frame.cut == parent_name and frame.authored_w > 0
        )
        content_w = parent.actual_w - 2 * lround(parent.border_pad * target)
        remaining_w = max(0, content_w - used)
        fill = remaining_w // len(members) if members else 0
        for frame in members:
            frame.actual_w = fill
        if members:
            members[-1].actual_w += remaining_w - fill * len(members)

    # Top-level zero-height frames fill the remaining screen height.
    used_h = sum(
        frame.actual_h
        for frame in frames
        if frame.authored_h > 0 and not frame.cut
    )
    remaining = max(0, screen_h - used_h)
    for frame in frames:
        if frame.authored_h == 0 and not frame.cut:
            frame.actual_h = remaining
            frame.virtual_h = remaining / canvas if canvas > 1e-6 else 0.0

    # Propagate filled top-level heights to cut children resolved before the fill.
    for frame in frames:
        if frame.cut and frame.authored_h == 0 and frame.cut in by_name:
            parent = by_name[frame.cut]
            frame.actual_h = max(
                0, parent.actual_h - 2 * lround(parent.border_pad * target)
            )
            frame.virtual_h = frame.actual_h / canvas if canvas > 1e-6 else 0.0


def add_ss_content_cut(content: str) -> tuple[str, bool]:
    changed = False
    pos = 0
    while True:
        pos = content.find("<frame ", pos)
        if pos < 0:
            break
        end = content.find(">", pos)
        if end < 0:
            break
        line = content[pos : end + 1]
        if not re.search(r"(?<!\S)cut\s+\S+", line):
            line = line[:-1] + " cut ss_content>"
            content = content[:pos] + line + content[end + 1 :]
            pos += len(line)
            changed = True
        else:
            pos = end + 1
    return content, changed


def actual_frame_rmf(content: str, frames: list[Frame]) -> str:
    """Replace frame W/H tokens with the actual parsed values."""
    out = content
    for frame in frames:
        match = FRAME_RE.search(out)
        while match:
            if match.group(1) == frame.name and int(match.group(2)) == frame.authored_w and int(
                match.group(3)
            ) == frame.authored_h:
                replacement = f"<frame {frame.name} {frame.actual_w} {frame.actual_h}{match.group(4)}>"
                out = out[: match.start()] + replacement + out[match.end() :]
                break
            match = FRAME_RE.search(out, match.end())
    return out


def parse_bghouls(content: str, canvas: float) -> list[dict[str, object]]:
    rows: list[dict[str, object]] = []
    for match in BGHOUL_RE.finditer(content):
        line = match.group(0)
        scale = token_float(line, "scale", 1.0)
        xoff = token_int(line, "xoff", 0)
        yoff = token_int(line, "yoff", 0)
        scale_pos = line.find(" scale ")
        yoff_pos = line.find(" yoff ")
        xoff_pos = line.find(" xoff ")

        scale_actual = scale * canvas
        if xoff_pos >= 0 and (scale_pos < 0 or xoff_pos < scale_pos):
            xoff_actual = xoff * canvas
        else:
            xoff_actual = xoff * scale_actual
        if yoff_pos >= 0 and (scale_pos < 0 or yoff_pos < scale_pos):
            yoff_actual = yoff * canvas
        else:
            yoff_actual = yoff * scale_actual

        rows.append(
            {
                "line": line,
                "authored_xoff": xoff,
                "authored_yoff": yoff,
                "authored_scale": scale,
                "actual_xoff": lround(xoff_actual),
                "actual_yoff": lround(yoff_actual),
                "actual_scale": scale_actual,
            }
        )
    return rows


def actual_bghoul_rmf(content: str, rows: list[dict[str, object]]) -> str:
    out = content
    for row in rows:
        line = str(row["line"])
        replacement = line
        replacement, _ = replace_token_number(replacement, "xoff", str(row["actual_xoff"]))
        replacement, _ = replace_token_number(replacement, "yoff", str(row["actual_yoff"]))
        replacement, _ = replace_token_number(replacement, "scale", f"{float(row['actual_scale']):.2f}")
        out = out.replace(line, replacement, 1)
    return out


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("--width", type=int, default=1920)
    parser.add_argument("--height", type=int, default=1080)
    parser.add_argument("--canvas", type=float, default=None, help="override menu canvas scale (default width/640)")
    parser.add_argument("--target", type=float, default=None, help="override vertical target scale (default height/480)")
    parser.add_argument("--no-chrome", action="store_true", help="skip 16:9 chrome injection")
    parser.add_argument(
        "--ss-content",
        action="store_true",
        help="treat the input as the ss_content page and add cut ss_content to uncut frames",
    )
    parser.add_argument("--no-bghoul", action="store_true", help="skip bghoul ratio adjustment")
    args = parser.parse_args()

    content = args.input.read_text(encoding="utf-8", errors="replace")
    canvas = args.canvas if args.canvas is not None else args.width / 640.0
    target = args.target if args.target is not None else args.height / 480.0
    factor = target / canvas if canvas > 1e-6 else 1.0

    print(f"Input: {args.input}")
    print(
        f"Video: {args.width}x{args.height}  canvas={canvas:.4f}  "
        f"target={target:.4f}  vertical_factor={factor:.4f}"
    )
    print(f"Wide chrome injection: {'no' if args.no_chrome else 'yes'}")
    print(f"Bghoul ratio adjustment: {'no' if args.no_bghoul else 'yes'}")
    print()

    served = content
    if not args.no_bghoul:
        served, _ = scale_bghoul_for_wide_aspect(served, factor)
    if not args.no_chrome:
        served, _ = inject_outer_16_9_wrapper(served, args.width, args.height)
    if args.ss_content:
        served, _ = add_ss_content_cut(served)

    print("=== Served RMF (after FS hook) ===")
    print(served)
    print()

    frames = parse_frames(served)
    compute_frame_actuals(frames, canvas, target, args.width, args.height, args.ss_content)
    print("=== Frame actual values ===")
    print(
        f"{'name':<18} {'authored W/H':>14} {'actual W/H':>14} "
        f"{'virtual H':>10} {'cut':<18} {'border':>7}"
    )
    for frame in frames:
        print(
            f"{frame.name:<18} {frame.authored_w:>6}/{frame.authored_h:<7} "
            f"{frame.actual_w:>6}/{frame.actual_h:<7} "
            f"{frame.virtual_h:>10.2f} {(frame.cut or '-'):<18} {frame.border_pad:>7}"
        )
    print()

    bghouls = parse_bghouls(served, canvas)
    if bghouls:
        print("=== Bghoul actual values ===")
        print(
            f"{'authored xoff/yoff/scale':>34} {'actual xoff/yoff/scale':>34}"
        )
        for row in bghouls:
            print(
                f"{row['authored_xoff']:>10}/{row['authored_yoff']:>7}/{row['authored_scale']:>7.2f}"
                f"   {row['actual_xoff']:>10}/{row['actual_yoff']:>7}/{row['actual_scale']:>7.2f}"
            )
        print()

    actual = actual_frame_rmf(served, frames)
    if bghouls:
        actual = actual_bghoul_rmf(actual, bghouls)
    print("=== Actual-values RMF (inspection only; do not re-feed) ===")
    print(actual)

    return 0


if __name__ == "__main__":
    sys.exit(main())
