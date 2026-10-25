#!/usr/bin/env python3
"""GOG violence-cluster + KB4571756 Raven probes (mirrors sof_install_detect.ps1)."""

import struct
import unittest
from pathlib import Path

IMAGE_BASE = 0x20000000
KB4571756_VA = 0x2015F1C0
GOG_STRINGS = {
    0x201373B8: b"GRANTED",
    0x201373C0: b"cl_violence",
    0x201373CC: b"sv_violence",
    0x201373D8: b"lock_pure",
    0x201373E4: b"lock_textures",
    0x20137428: b"lock_password",
    0x20137438: b"non_multilingual",
    0x2013744C: b"German",
}
GOG_PROBE_VAS = [
    0x201373BF, 0x201373CB, 0x201373D7, 0x201373E1, 0x201373E2, 0x201373E3,
    0x201373F1, 0x201373F2, 0x201373F3, 0x201373FE, 0x201373FF, 0x2013740E, 0x2013740F,
    0x2013741A, 0x2013741B, 0x20137427, 0x20137435, 0x20137436, 0x20137437,
    0x20137448, 0x20137449, 0x2013744A, 0x2013744B, 0x20137452, 0x20137453,
]


def pe_map(data: bytes, va: int, ibase: int = IMAGE_BASE) -> int:
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    fh = pe + 4
    sc = struct.unpack_from("<H", data, fh + 2)[0]
    opt_sz = struct.unpack_from("<H", data, fh + 16)[0]
    opt = fh + 20
    rva = va - ibase
    sect = opt + opt_sz
    for i in range(sc):
        s = sect + i * 40
        va_s = struct.unpack_from("<I", data, s + 12)[0]
        vsize = struct.unpack_from("<I", data, s + 8)[0]
        raw_off = struct.unpack_from("<I", data, s + 20)[0]
        if vsize and va_s <= rva < va_s + vsize:
            return raw_off + (rva - va_s)
    return -1


def is_gog_violence_layout(data: bytes) -> bool:
    for va, expected in GOG_STRINGS.items():
        off = pe_map(data, va)
        if off < 0 or data[off : off + len(expected)] != expected:
            return False
    for va in GOG_PROBE_VAS:
        off = pe_map(data, va)
        if off < 0 or data[off] not in (0, 1):
            return False
    return True


def kb4571756_exe_fix(data: bytes) -> bool:
    off = pe_map(data, KB4571756_VA)
    if off < 0:
        return False
    raven = "Raven Software".encode("utf-16-le")
    return data[off : off + len(raven)] == b"\x00" * len(raven)


class SoFInstallDetectTest(unittest.TestCase):
    def test_real_gog_bins(self):
        paths = [
            Path("/run/media/dinda/My Passport Ntfs/sof_backups/gog_sof_bins/SoF.exe"),
            Path(
                "/home/dinda/.local/share/wineprefixes/sof-fresh/drive_c/users/dinda/"
                "Soldier of Fortune/SoF.exe"
            ),
        ]
        for path in paths:
            if not path.is_file():
                continue
            data = path.read_bytes()
            self.assertTrue(is_gog_violence_layout(data), str(path))
            if "sof-fresh" in str(path):
                self.assertTrue(kb4571756_exe_fix(data))
            if "gog_sof_bins" in str(path):
                self.assertFalse(kb4571756_exe_fix(data))


def classify_distribution(data: bytes) -> str:
    return "gog" if is_gog_violence_layout(data) else "cd"


if __name__ == "__main__":
    unittest.main()
