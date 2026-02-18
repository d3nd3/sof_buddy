# http_maps

Client-side assist for missing maps. If a BSP is missing, it can fetch zip content over HTTP and resume normal precache.

## Runtime flow

1. `CL_ParseConfigString` pre reads the next configstring index from `net_message`.
2. `CL_ParseConfigString` post runs only for index `36`, reads map path, caches it, updates loading UI, and may start worker early.
3. `CL_Precache_f` resolves map name (memory first, then cached), then:
   - passes through immediately if disabled or unresolved,
   - attaches to an already-running worker, or
   - starts a new worker and defers engine continue.
4. Worker flow: CRC-list fetch -> local CRC compare -> optional full zip download -> extract to `user/` -> BSP CRC validate.
   - if CRC-list is missing remotely, BSP presence is checked via engine FS (`FS_LoadFile(..., NULL, false)`) to decide `MAP PRESENT` vs `UDP Downloading...`.
5. `Qcommon_Frame` pre/post pumps progress/completion and runs deferred continue.

## Notes

- Early map detection keeps menu/loading status responsive.
- Post hook is hard-gated to configstring index `36`.
- Old worker jobs cannot complete over newer jobs.
- State resets on `Reconnect_f`, init, and transition to `ca_disconnected`.
- Statuses: `CHECKING`, `MAP PRESENT`, `HTTP Downloading...`, `UDP Downloading...`.
- If HTTP path fails, engine fallback stays normal.

## CVars

- `_sofbuddy_http_maps`: `0` off, `1` primary, `2` random provider, `3` rotate provider.
- `_sofbuddy_http_maps_dl_1/2/3`: zip download base URLs.
- `_sofbuddy_http_maps_crc_1/2/3`: CRC-list lookup base URLs.
- `_sofbuddy_http_show_providers`: show/hide provider UI inputs.
