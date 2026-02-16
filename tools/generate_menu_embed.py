#!/usr/bin/env python3
"""Generate menu_data.cpp from src/features/internal_menus/menu_library/*/."""
import os
import sys
from pathlib import Path

def main():
    script_dir = Path(__file__).parent
    project_root = script_dir.parent
    menu_lib = project_root / "src" / "features" / "internal_menus" / "menu_library"
    out_cpp = project_root / "src" / "features" / "internal_menus" / "menu_data.cpp"

    if not menu_lib.exists():
        content = '''#include "feature_config.h"
#if FEATURE_INTERNAL_MENUS
#include "shared.h"
void internal_menus_load_library(void) { g_menu_internal_files.clear(); }
#endif
'''
        out_cpp.parent.mkdir(parents=True, exist_ok=True)
        old_content = out_cpp.read_text() if out_cpp.exists() else None
        if old_content != content:
            out_cpp.write_text(content)
        return 0

    lines = [
        '#include "feature_config.h"',
        '#if FEATURE_INTERNAL_MENUS',
        '#include "shared.h"',
        '',
        'void internal_menus_load_library(void) {',
        '    g_menu_internal_files.clear();',
    ]

    for menu_dir in sorted(menu_lib.iterdir()):
        if not menu_dir.is_dir():
            continue
        menu_name = menu_dir.name
        for rmf in sorted(menu_dir.glob("*.rmf")):
            with open(rmf, 'rb') as f:
                data = f.read()
            hex_bytes = ', '.join(f'0x{b:02x}' for b in data)
            lines.append(f'    {{')
            lines.append(f'        std::vector<uint8_t> v = {{{hex_bytes}}};')
            lines.append(f'        g_menu_internal_files["{menu_name}"]["{rmf.name}"] = std::move(v);')
            lines.append(f'    }}')

    lines.extend(['}', '', '#endif'])
    content = '\n'.join(lines) + '\n'

    out_cpp.parent.mkdir(parents=True, exist_ok=True)
    old_content = out_cpp.read_text() if out_cpp.exists() else None
    if old_content != content:
        out_cpp.write_text(content)
    return 0

if __name__ == '__main__':
    sys.exit(main())
