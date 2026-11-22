#!/usr/bin/env python3
"""Print PE fingerprint fields for sof2/sof2_buddy/patches.json."""
import hashlib
import struct
import sys
from pathlib import Path


def pe_image_size(data: bytes) -> int:
    e = struct.unpack_from("<I", data, 0x3C)[0]
    return struct.unpack_from("<I", data, e + 24 + 56)[0]


def import_dll_offset(data: bytes, dll: str) -> int | None:
    e = struct.unpack_from("<I", data, 0x3C)[0]
    coff = e + 4
    nsec = struct.unpack_from("<H", data, coff + 2)[0]
    opt_sz = struct.unpack_from("<H", data, coff + 16)[0]
    opt = coff + 20
    imp_rva = struct.unpack_from("<I", data, opt + 104)[0]
    sec = opt + opt_sz
    sections = []
    for i in range(nsec):
        s = sec + i * 40
        sections.append(
            (
                struct.unpack_from("<I", data, s + 12)[0],
                struct.unpack_from("<I", data, s + 8)[0],
                struct.unpack_from("<I", data, s + 16)[0],
                struct.unpack_from("<I", data, s + 20)[0],
            )
        )

    def rva_to_off(rva: int) -> int | None:
        for va, vs, rs, raw in sections:
            if va <= rva < va + max(vs, rs):
                return rva - va + raw
        return None

    off = rva_to_off(imp_rva)
    while off and struct.unpack_from("<I", data, off)[0]:
        name_rva = struct.unpack_from("<I", data, off + 12)[0]
        no = rva_to_off(name_rva)
        name = data[no : data.find(b"\x00", no)].decode()
        if name.lower() == dll.lower():
            return no
        off += 20
    return None


def main() -> None:
    path = Path(sys.argv[1] if len(sys.argv) > 1 else "SoF2MP.exe")
    data = path.read_bytes()
    print("file:", path)
    print("sha256:", hashlib.sha256(data).hexdigest())
    print("image_size:", pe_image_size(data))
    off = import_dll_offset(data, "WSOCK32.dll")
    print("wsock_import_name_offset:", f"0x{off:X}" if off is not None else None)


if __name__ == "__main__":
    main()
