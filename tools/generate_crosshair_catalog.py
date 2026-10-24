#!/usr/bin/env python3
"""Build crosshair paks, C theme catalog, legacy migration table, and F12 menu RMF."""

from __future__ import annotations

import re
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ASSET_DIR = ROOT / "rsrc" / "assets" / "crosshairs"
OUT_RMF = ROOT / "src" / "features" / "internal_menus" / "menu_library" / "sof_buddy" / "more_crosshair_list.rmf"

CH_PATH = re.compile(rb"pics/menus/status/ch(\d)(?:\.\w+)?")
STOCK_THEME = "stock"
MENU_RELOAD = (
    'key mouse1 "sofbuddy_menu sof_buddy/misc" '
    'key mouse2 "sofbuddy_menu sof_buddy/misc" '
)


def patch_m32_vpath(data: bytearray, vpath: str) -> None:
    if len(data) < 60:
        return
    if struct.unpack_from("<I", data, 0)[0] != 4:
        return
    gl_name = vpath[:-4] if vpath.endswith(".m32") else vpath
    name = gl_name.encode("ascii", errors="replace")[:56]
    data[4:60] = name.ljust(56, b"\0")[:56]


def m32_bytes_for_vpath(data: bytes, vpath: str) -> bytes:
    """SoF GL_FindImage matches the requested path to the name embedded in the .m32."""
    out = bytearray(data)
    patch_m32_vpath(out, vpath)
    return bytes(out)


def read_pak_entry(pak: Path, wanted: str) -> bytes | None:
    raw = pak.read_bytes()
    if len(raw) < 12 or raw[:4] != b"PACK":
        return None
    toc_off, dir_len = struct.unpack_from("<II", raw, 4)
    if dir_len == 0 or dir_len % 64 != 0:
        return None
    wanted_norm = wanted.lower().replace("\\", "/")
    toc_positions = [toc_off]
    alt = len(raw) - dir_len
    if 0 < alt != toc_off:
        toc_positions.append(alt)
    for toc_pos in toc_positions:
        for pos in range(0, dir_len, 64):
            name = raw[toc_pos + pos : toc_pos + pos + 56].split(b"\0")[0].decode(errors="replace")
            name = name.lower().replace("\\", "/")
            if name != wanted_norm:
                continue
            off, size = struct.unpack_from("<II", raw, toc_pos + pos + 56)
            if size == 0 or size > len(raw):
                continue
            for file_off in (off, off + 12) if off < 12 else (off,):
                chunk = raw[file_off : file_off + size]
                if len(chunk) >= 4 and struct.unpack_from("<I", chunk, 0)[0] == 4:
                    return bytes(chunk)
    return None


def write_pak_bytes(path: Path, entries: list[tuple[str, bytes]]) -> None:
    blob = bytearray()
    toc: list[tuple[str, int, int]] = []
    for vpath, raw in entries:
        data = m32_bytes_for_vpath(raw, vpath)
        toc.append((vpath, len(blob), len(data)))
        blob.extend(data)
    header = 12
    out = bytearray()
    out.extend(b"PACK")
    toc_off = header + len(blob)
    out.extend(struct.pack("<II", toc_off, len(toc) * 64))
    out.extend(blob)
    for vpath, off, size in toc:
        name = vpath.encode("ascii", errors="replace")[:56]
        out.extend(name.ljust(56, b"\x00"))
        out.extend(struct.pack("<II", header + off, size))
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(out)


def build_none_crosshair_m32() -> bytes:
    """Gray tile with an X — shown when a theme has no art for crosshair 0..4."""
    sauron = ASSET_DIR / "sauron_crosshairs.pak"
    template = read_pak_entry(sauron, "pics/menus/status/ch0.m32") if sauron.is_file() else None
    if not template:
        return b""
    data = bytearray(template)
    base, pitch, dim = 0x210, 0x10, 16
    for y in range(dim):
        for x in range(dim):
            off = base + y * pitch + x
            if off >= len(data):
                continue
            on = x == y or x == dim - 1 - y
            data[off] = 0xF0 if on else 0x18
    patch_m32_vpath(data, "pics/menus/status/none")
    return bytes(data)


def pak_slots(pak: Path) -> set[int]:
    raw = pak.read_bytes()
    return {int(m.group(1)) for m in CH_PATH.finditer(raw)}


def strings_slots(pak: Path) -> set[int]:
    try:
        out = subprocess.check_output(["strings", str(pak)], stderr=subprocess.DEVNULL)
    except (subprocess.CalledProcessError, FileNotFoundError):
        return pak_slots(pak)
    slots = set()
    for line in out.splitlines():
        m = re.search(rb"pics/menus/status/ch(\d)", line)
        if m:
            slots.add(int(m.group(1)))
    return slots


def engine_slot_map(pak_slots_set: set[int]) -> list[int]:
    """Map engine crosshair cvar 0..4 -> pak ch index, or -1."""
    out: list[int] = []
    for es in range(5):
        if es in pak_slots_set:
            out.append(es)
        elif (es + 1) in pak_slots_set:
            out.append(es + 1)
        else:
            out.append(-1)
    return out


def ensure_built_paks() -> None:
    """Repack mhp/sauron paks with correct TOC offsets and embedded GL names."""
    mhp_pak = ASSET_DIR / "mhp.pak"
    ch2 = read_pak_entry(mhp_pak, "pics/menus/status/ch2.m32") if mhp_pak.is_file() else None
    if ch2:
        write_pak_bytes(mhp_pak, [("pics/menus/status/ch2.m32", ch2)])
    sauron_pak = ASSET_DIR / "sauron_crosshairs.pak"
    if sauron_pak.is_file():
        sauron_entries: list[tuple[str, bytes]] = []
        for i in range(4):
            vpath = f"pics/menus/status/ch{i}.m32"
            blob = read_pak_entry(sauron_pak, vpath)
            if blob:
                sauron_entries.append((vpath, blob))
        if sauron_entries:
            write_pak_bytes(sauron_pak, sauron_entries)


def build_themes() -> list[dict]:
    ensure_built_paks()
    themes: list[dict] = [{"name": STOCK_THEME, "pak": "", "slot_map": [0, 1, 2, 3, 4]}]
    for pak in sorted(ASSET_DIR.glob("*.pak")):
        slots = sorted(strings_slots(pak))
        if not slots or not any(0 <= s <= 4 for s in slots):
            continue
        slot_set = set(slots)
        themes.append(
            {
                "name": pak.stem,
                "pak": f"crosshairs/{pak.name}",
                "slot_map": engine_slot_map(slot_set),
            }
        )
    return themes


def theme_index(themes: list[dict], pak_relpath: str) -> int:
    pak_relpath = pak_relpath.replace("\\", "/")
    for i, t in enumerate(themes):
        if t["pak"] == pak_relpath:
            return i
    return 0


def build_menu_catalog(themes: list[dict]) -> list[dict]:
    """One menu row per selectable crosshair (synced to pack slot_map)."""
    items: list[dict] = []
    for ti, t in enumerate(themes):
        sm = t["slot_map"]
        if ti == 0:
            for es in range(5):
                items.append({"theme": ti, "slot": es, "label": f"Stock {es}"})
            continue
        seen_pak_slot: set[int] = set()
        for es in range(5):
            ps = sm[es]
            if ps < 0 or ps in seen_pak_slot:
                continue
            seen_pak_slot.add(ps)
            items.append(
                {"theme": ti, "slot": es, "label": f'{t["name"]} (slot {es})'}
            )
    return items


THEME_ABBR = {
    "fl44": "f44",
    "jay_crosshairs_v4": "jay",
    "keifer_2009": "k09",
    "keifer_2010": "k10",
    "mhp": "mhp",
    "reps_crosshairs_v1.2": "rep",
    "sauron_crosshairs": "sau",
    "yump_skulls": "yum",
}


def short_menu_label(entry: dict, themes: list[dict]) -> str:
    if entry["theme"] == 0:
        return f"S{entry['slot']}"
    name = themes[entry["theme"]]["name"]
    abbr = THEME_ABBR.get(name, name[:4])
    return f"{abbr}{entry['slot']}"


def build_legacy_flat_catalog() -> list[dict]:
    """Old picker: one row per stock slot + per (pack, ch0..ch4) — for config migration."""
    ensure_built_paks()
    catalog: list[dict] = []
    for slot in range(5):
        catalog.append({"pak": "", "slot": slot})
    seen: set[tuple[str, int]] = set()
    for pak in sorted(ASSET_DIR.glob("*.pak")):
        slots = sorted(strings_slots(pak))
        if not slots:
            continue
        rel = f"crosshairs/{pak.name}"
        for slot in slots:
            if slot < 0 or slot > 4:
                continue
            key = (rel, slot)
            if key in seen:
                continue
            seen.add(key)
            catalog.append({"pak": rel, "slot": slot})
    return catalog


def write_placeholder_inc(out_dir: Path) -> None:
    blob = build_none_crosshair_m32()
    out = out_dir / "crosshair_placeholder.inc"
    if not blob:
        out.write_text(
            "/* No sauron_crosshairs.pak ch0 — placeholder disabled */\n"
            "#pragma once\n"
            "static const unsigned char kCrosshairNoneM32[] = {0};\n"
            "static const unsigned int kCrosshairNoneM32Size = 0u;\n",
            encoding="utf-8",
        )
        return
    lines = [
        "/* Auto-generated by tools/generate_crosshair_catalog.py */",
        "#pragma once",
        "",
        f"static const unsigned int kCrosshairNoneM32Size = {len(blob)}u;",
        "static const unsigned char kCrosshairNoneM32[] = {",
    ]
    for i in range(0, len(blob), 16):
        chunk = blob[i : i + 16]
        lines.append("    " + ", ".join(f"0x{b:02x}" for b in chunk) + ",")
    lines.append("};")
    lines.append("")
    out.write_text("\n".join(lines) + "\n", encoding="utf-8")


def write_outputs(
    themes: list[dict], legacy: list[dict], menu: list[dict], out_dir: Path
) -> None:
    write_placeholder_inc(out_dir)
    out_inc = out_dir / "crosshair_catalog.inc"
    out_inc.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        "/* Auto-generated by tools/generate_crosshair_catalog.py */",
        "#pragma once",
        "",
        "#include <cstdint>",
        "",
        "struct CrosshairThemeEntry {",
        "    const char* name; /* pak stem, or \"stock\" */",
        "    const char* pak_relpath; /* empty = vanilla FS for ch0..ch4 */",
        "};",
        "",
        "struct CrosshairMenuPick {",
        "    int theme_index;",
        "    int slot; /* engine crosshair cvar 0..4 */",
        "};",
        "",
        "struct CrosshairLegacyPick {",
        "    int theme_index;",
        "    int slot; /* legacy migration: pak ch index or stock 0..4 */",
        "};",
        "",
        f"static constexpr int kCrosshairThemeCount = {len(themes)};",
        "static const CrosshairThemeEntry kCrosshairThemes[] = {",
    ]
    for t in themes:
        pak = t["pak"].replace("\\", "/")
        lines.append(f'    {{"{t["name"]}", "{pak}"}},')
    lines.append("};")
    lines.append("")
    lines.append("// Engine crosshair 0..4 -> pak ch index (-1 = missing for theme).")
    lines.append("static const int8_t kCrosshairThemePakSlot[kCrosshairThemeCount][5] = {")
    for t in themes:
        sm = t["slot_map"]
        lines.append("    {" + ", ".join(str(x) for x in sm) + "},")
    lines.append("};")
    lines.append("")
    lines.append(f"static constexpr int kCrosshairMenuCount = {len(menu)};")
    lines.append("static const CrosshairMenuPick kCrosshairMenu[] = {")
    for e in menu:
        lines.append(f"    {{{e['theme']}, {e['slot']}}},")
    lines.append("};")
    lines.append("")
    lines.append(f"static constexpr int kCrosshairLegacyCatalogCount = {len(legacy)};")
    lines.append("static const CrosshairLegacyPick kCrosshairLegacyCatalog[] = {")
    for e in legacy:
        lines.append(f"    {{{theme_index(themes, e['pak'])}, {e['slot']}}},")
    lines.append("};")
    lines.append("")
    short_labels = [short_menu_label(e, themes) for e in menu]
    label_blob = ",".join(short_labels)
    if len(label_blob) >= 240:
        raise SystemExit(f"crosshair list labels exceed 240 bytes ({len(label_blob)})")
    match_blob = ",".join(str(i) for i in range(len(menu)))
    if len(match_blob) >= 240:
        raise SystemExit(f"crosshair list match exceed 240 bytes ({len(match_blob)})")
    lines.append(f'static const char kCrosshairMenuListLabels[] = "{label_blob}";')
    lines.append(f'static const char kCrosshairMenuListMatch[] = "{match_blob}";')
    lines.append("")
    out_inc.write_text("\n".join(lines) + "\n", encoding="utf-8")

    OUT_RMF.write_text(
        "{crosshair_list}\n<includecvar _sb_internal_crosshair_preview_rmf>\n",
        encoding="utf-8",
    )


def main() -> int:
    if not ASSET_DIR.is_dir():
        print(f"missing {ASSET_DIR}", file=sys.stderr)
        return 1
    out_dir = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "build" / "release"
    themes = build_themes()
    legacy = build_legacy_flat_catalog()
    menu = build_menu_catalog(themes)
    write_outputs(themes, legacy, menu, out_dir)
    print(
        f"crosshair themes: {len(themes)}, menu picks: {len(menu)}, legacy: {len(legacy)} -> "
        f"{out_dir / 'crosshair_catalog.inc'}, {OUT_RMF}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
