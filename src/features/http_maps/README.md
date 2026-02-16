# http_maps

`http_maps` is a client-side map download assist feature inspired by:

- `d3nd3/sof-plus-plus-nix` `httpdl.cpp` (precache interception + HTTP map assist)

## What it does

1. Learns the map name as early as `CL_ParseConfigString`.
2. Intercepts `CL_Precache_f`.
3. If BSP exists locally, continues precache immediately.
4. If BSP is missing, starts HTTP assist worker and defers `CL_Precache_f` continue.
5. Fetches zip Central Directory via partial HTTP Range (CRC list only).
6. Compares each file CRC to disk; if any missing or CRC mismatch, downloads full zip.
7. Extracts into `user/` and validates extracted BSP CRC.
8. Continues precache when worker completes (success = HTTP files ready, failure = engine fallback path).
9. If map changes while waiting, a newer HTTP job preempts the older one.
10. Mirrors progress/status/files into the embedded loading menu cvars.

## Addresses (current build)

- `detours.yaml`:
  - `CL_Precache_f.identifier = 0x00112F0` (relative)
  - `CL_ParseConfigString.identifier = 0x000E690` (relative)
- `src/features/http_maps/http_maps.cpp`:
  - `kHttpMaps_MapNamePtr = 0x201E9DCC` (absolute address of map name string)

## CVars

- `_sofbuddy_http_maps` (default `1`)
  - Mode: 0=off, 1=use first URL index, 2=random index.
- `_sofbuddy_http_maps_dl_1` (default `https://raw.githubusercontent.com/plowsof/sof1maps/main`)
  - Zip download base URL (index 1).
- `_sofbuddy_http_maps_dl_2`, `_sofbuddy_http_maps_dl_3` (empty)
  - Extra download URLs.
- `_sofbuddy_http_maps_crc_1` (default same as dl_1)
  - CRC lookup base URL (partial range to zip).
- `_sofbuddy_http_maps_crc_2`, `_sofbuddy_http_maps_crc_3` (empty)
  - Extra CRC URLs.

## Notes

- Uses WinHTTP for partial Range requests (CRC-only) and full zip download.
- Local map checks: `maps/...`, `user/maps/...`.
- CRC comparison avoids full zip download when local files match.
- No `_sofbuddy_http_maps_timeout` CVar; engine fallback occurs on worker failure.
