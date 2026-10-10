#!/usr/bin/env python3
"""Generate features_rows.rmf from FEATURES.txt for the in-game Features tab."""

import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "src/features/internal_menus/menu_library/sof_buddy/features_rows.rmf"

SECTIONS = [
    ("Core / Performance", ["media_timers"]),
    ("Security Fixes", ["console_protection"]),
    ("Bug Fixes", [
        "new_system_bug", "cl_maxfps_singleplayer", "list_match_fix",
        "cbuf_limit_increase",
    ]),
    ("Graphics", [
        "texture_mapping_min_mag", "scaled_con", "scaled_hud", "scaled_menu",
        "hd_textures", "vsync_toggle", "lighting_blend",
    ]),
    ("Gameplay", ["teamicons_offset", "entity_visualizer"]),
    ("Networking", ["http_maps"]),
    ("Menus", ["internal_menus"]),
    ("Input", ["raw_mouse"]),
]

LABELS = {
    "media_timers": "Media Timers",
    "texture_mapping_min_mag": "Texture Mapping",
    "scaled_con": "Scaled Console",
    "scaled_hud": "Scaled HUD",
    "scaled_menu": "Scaled Menu",
    "hd_textures": "HD Textures",
    "vsync_toggle": "VSync Toggle",
    "lighting_blend": "Lighting Blend",
    "teamicons_offset": "Team Icons Offset",
    "entity_visualizer": "Entity Visualizer",
    "http_maps": "HTTP Maps",
    "internal_menus": "Internal Menus",
    "new_system_bug": "New System Bug Fix",
    "console_protection": "Console Protection",
    "cl_maxfps_singleplayer": "Singleplayer Max FPS",
    "cbuf_limit_increase": "Cbuf Limit Increase",
    "raw_mouse": "Raw Mouse",
}


def enabled_features(path: Path) -> set[str]:
    out: set[str] = set()
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("//"):
            continue
        if line.startswith("#"):
            continue
        name = line.split("#", 1)[0].strip()
        if name:
            out.add(name)
    return out


def row(name: str) -> str:
    label = LABELS.get(name, name.replace("_", " ").title())
    return (
        f'<list "Off,On" match "0,1" cvar _sofbuddy_feature_{name} '
        f'atext "{label} : " noshade><br>'
    )


def generate(features_path: Path) -> str:
    enabled = enabled_features(features_path)
    lines = ["; Generated from FEATURES.txt. Do not edit by hand."]
    for section, names in SECTIONS:
        section_rows = [row(n) for n in names if n in enabled]
        if not section_rows:
            continue
        lines.append('<font type title tint cyan atint cyan>')
        lines.append(f'"{section}"')
        lines.append("<font type title tint white atint white>")
        lines.extend(section_rows)
    return "\n".join(lines) + "\n"


def main() -> int:
    features_path = ROOT / "features" / "FEATURES.txt"
    if len(sys.argv) > 1:
        features_path = Path(sys.argv[1])
    content = generate(features_path)
    OUT.parent.mkdir(parents=True, exist_ok=True)
    if OUT.exists() and OUT.read_text(encoding="utf-8") == content:
        return 0
    fd, tmp = tempfile.mkstemp(prefix="features_rows.", suffix=".rmf", dir=OUT.parent)
    try:
        with open(fd, "w", encoding="utf-8") as f:
            f.write(content)
        tmp_path = Path(tmp)
        tmp_path.replace(OUT)
    except Exception:
        Path(tmp).unlink(missing_ok=True)
        raise
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
