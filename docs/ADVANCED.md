### Advanced: PE Function Map Generator

This project includes a small host-side utility to generate function start maps (exports + `E8 rel32` call targets) for SoF modules. The maps are written as JSON and can be loaded at runtime by a future classifier/loader.

### Build (host machine)

Use the optional make target; this is intentionally excluded from the default DLL build.

```bash
make funcmap-gen
```

This produces:

- `tools/pe_funcmap_gen`

### Usage

```bash
tools/pe_funcmap_gen <out_dir> <image1> [image2 ...]
```

Example (using Wine-installed game binaries):

```bash
mkdir -p sof_buddy/funcmaps
tools/pe_funcmap_gen sof_buddy/funcmaps \
  ~/.wine/drive_c/users/dinda/Soldier\ of\ Fortune/SoF.exe \
  ~/.wine/drive_c/users/dinda/Soldier\ of\ Fortune/ref_gl.dll \
  ~/.wine/drive_c/users/dinda/Soldier\ of\ Fortune/Base/player.dll \
  ~/.wine/drive_c/users/dinda/Soldier\ of\ Fortune/Base/gamex86.dll
```

This will emit one JSON per input image:

- `sof_buddy/funcmaps/SoF.exe.json`
- `sof_buddy/funcmaps/ref_gl.dll.json`
- `sof_buddy/funcmaps/player.dll.json`
- `sof_buddy/funcmaps/gamex86.dll.json`

### JSON schema (minimal)

```json
{
  "imageName": "<path>",
  "imageSize": <uint32>,
  "textRva": <uint32>,
  "textSize": <uint32>,
  "functions": [
    { "rva": <uint32>, "name": "<optional export>" }
  ]
}
```

### Notes & caveats

- The generator enumerates PE exports and heuristically scans `.text` for `E8 rel32` call targets landing inside `.text`.
- EXE images include an additional synthesized entry `{ name: "EntryPoint" }`.
- Intended for offline preparation; runtime code loads these maps from `sof_buddy/funcmaps` and falls back to `rsrc/funcmaps` if not present.

### How addresses are collected

- Exports:
  - Reads the PE export directory and collects RVAs from `AddressOfFunctions`.
  - Associates names via `AddressOfNames`/`AddressOfNameOrdinals` where available.
- `E8 rel32` scan:
  - Locates the `.text` section and scans bytes for opcode `0xE8` followed by a 32-bit signed displacement.
  - Computes the potential target RVA as `(call_end_rva + rel32)` and keeps targets that fall within `.text`.
- Entrypoint (EXE only):
  - Adds `OptionalHeader.AddressOfEntryPoint` as `"EntryPoint"`.
- Post-processing:
  - Deduplicates by RVA and sorts ascending.
- Address form:
  - All addresses in JSON are RVAs (relative virtual addresses), not absolute VAs.

Limitations of the first-pass scanner:
- The `E8` scanner is heuristic and may miss targets if `.text` file layout differs from virtual layout or if calls cross section boundaries.
- Indirect calls, imports, and tail-call patterns are not followed.
- No disassembly/CFG; only direct `E8` calls are considered.


