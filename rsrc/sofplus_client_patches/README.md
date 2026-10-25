# SoF Plus client patch kits (20140531)

Bundled kits from the official SoF Plus client patch release. Each subdirectory is one
retail layout variant:

- `cd` / `gog` — original 2014 patchers
- `cd-kb4571756` / `gog-kb4571756` — Windows 10+ networking fix (May 2022)

The installer runs `sofplus-patch.exe` against the user's existing `SoF.exe` and `.pak`
files, then extracts the generated zip into the game directory. Do not redistribute
generated zips; they contain personalized game binaries.

To refresh from a local mirror:

```bash
tools/sync_sofplus_client_patches.sh /path/to/sofplus-clients
```
