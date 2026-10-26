#!/usr/bin/env python3
"""Generate features_rows.rmf from FEATURES.txt for the in-game Features tab."""

import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "src/features/internal_menus/menu_library/sof_buddy/features_rows.rmf"

sys.path.insert(0, str(ROOT / "tools"))
from generate_installer import component_label, read_features  # noqa: E402

SECTION_RECOMMENDED = "Recommended / tested features"
SECTION_OPTIONAL = "Optional / unstable features"

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
    "master_server_fallback": "GameSpy Master Fallback",
    "internal_menus": "Internal Menus",
    "new_system_bug": "New System Bug Fix",
    "console_protection": "Console Protection",
    "cl_maxfps_singleplayer": "Singleplayer Max FPS",
    "list_match_fix": "List Match Fix",
    "cbuf_limit_increase": "Cbuf Limit Increase",
    "raw_mouse": "Raw Mouse",
    "more_crosshairs": "More Crosshairs",
}


def row(name: str) -> str:
    label = LABELS.get(name, name.replace("_", " ").title())
    return (
        f'<list "Off,On,Unavailable" match "0,1,2" cvar _sofbuddy_feature_{name} '
        f'atext "{label} : " noshade><br>'
    )


def tier_header(title: str, first: bool) -> list[str]:
    out: list[str] = []
    if not first:
        out.append("<hbr>")
    out.append('<font type title tint cyan atint cyan>')
    out.append(f'<text "{title}" align center border 0 0><hbr>')
    out.append('<font type title tint white atint white>')
    return out


def category_header(title: str, first_in_tier: bool) -> list[str]:
    out: list[str] = []
    if not first_in_tier:
        out.append("<hbr>")
    out.append('<font type title tint sb_subtle atint sb_subtle>')
    out.append(f'<text "{title}" align center border 0 0><hbr>')
    out.append('<font type title tint white atint white>')
    return out


def generate(features_path: Path) -> str:
    categories = read_features(features_path)
    recommended: list[tuple[str, list[str]]] = []
    optional: list[tuple[str, list[str]]] = []

    for category, features in categories:
        required = "(always enabled)" in category.lower()
        cat_title = component_label(category)
        rec_rows: list[str] = []
        opt_rows: list[str] = []
        for name, disabled in features:
            if required or not disabled:
                rec_rows.append(row(name))
            else:
                opt_rows.append(row(name))
        if rec_rows:
            recommended.append((cat_title, rec_rows))
        if opt_rows:
            optional.append((cat_title, opt_rows))

    lines: list[str] = []
    first_tier = True
    if recommended:
        lines.extend(tier_header(SECTION_RECOMMENDED, first_tier))
        first_tier = False
        first_cat = True
        for cat_title, rows in recommended:
            lines.extend(category_header(cat_title, first_cat))
            first_cat = False
            lines.extend(rows)
    if optional:
        lines.extend(tier_header(SECTION_OPTIONAL, first_tier))
        first_tier = False
        first_cat = True
        for cat_title, rows in optional:
            lines.extend(category_header(cat_title, first_cat))
            first_cat = False
            lines.extend(rows)
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
        Path(tmp).replace(OUT)
    except Exception:
        Path(tmp).unlink(missing_ok=True)
        raise
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
