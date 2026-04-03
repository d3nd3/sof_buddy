# http_maps

Client-side assist for map availability during connect. It checks map zip metadata over HTTP, optionally downloads/extracts missing content, then lets the engine continue normal precache.

## Runtime flow

1. `CL_ParseConfigString` pre peeks the current configstring index from `net_message`.
2. `CL_ParseConfigString` post runs only for index `36` (map model path), normalizes/caches map BSP, and may start an early worker.
   - It does not restart work for the same cached map.
   - It ignores repeated map configstring waves once that map is already marked completed.
3. Early worker (`continue_callback == nullptr`) may finish before `CL_Precache_f`.
4. `CL_Precache_f` resolves map (memory first, then cached), then:
   - pass-through if feature disabled or unresolved,
   - if already waiting on same map: attach callback and pump,
   - otherwise start worker and defer engine continue.
5. `http_maps_pump()` drives progress/status and completion:
   - waits for `continue_callback` before final continue (prevents callback-null race),
   - if worker already finished but callback not set yet, status is still updated early via engine FS check,
   - once callback exists, completion continues into original `CL_Precache_f`.
6. Worker flow:
   - fetch zip CRC list -> compare local CRCs -> optional zip download -> extract to `user/` -> BSP CRC validate.
   - if remote CRC list is unavailable, treat HTTP path as "assume local/PAK" and let engine FS decide final status.
7. `Qcommon_Frame` pumps deferred work in post-frame and runs deferred continue when ready.

## UI and menu behavior

- `http_maps` shows loading UI through `loading_show_ui()` when starting early/late work.
- In unlock mode (`_sofbuddy_loading_lock_input 0`), internal menus use a safe loading shell (`loading/loading_safe`) to avoid interactive loading actions while console preview is enabled.
- `SCR_BeginLoadingPlaque` can fire multiple times during connect/load. Internal menu hook avoids extra push/killmenu in problematic repeated paths (including post-completion skip via http_maps state).

## Notes

- Configstring post is hard-gated to index `36`.
- Worker jobs are keyed; stale jobs cannot overwrite active job state.
- State resets on init, `Reconnect_f`, and stable disconnected transitions.
- Status labels: `CHECKING`, `NOT NEEDED`, `HTTP Downloading...`, `UDP Downloading...`.
- If HTTP path cannot provide map content, engine fallback remains the source of truth.

## CVars

Each block is: **name**, **default**, short description. Lines are kept short so the file reads without sideways scrolling.

**`_sofbuddy_http_maps`**  
Default: `1`  
`0` off · `1` primary provider · `2` random provider · `3` rotate provider.

**`_sofbuddy_http_maps_dl_1`**  
Default: `https://sofvault.org/sof1maps`  
Zip download base URL (provider slot 1).

**`_sofbuddy_http_maps_dl_2`**  
Default (one URL; wrapped for layout):  
`https://raw.githubusercontent.com/`  
`plowsof/sof1maps/main`  
Zip download base URL (provider slot 2).

**`_sofbuddy_http_maps_dl_3`**  
Default: *(empty)*  
Zip download base URL (provider slot 3).

**`_sofbuddy_http_maps_crc_1`**  
Default: `https://sofvault.org/sof1maps`  
CRC lookup base URL (index 1). HTTP Range fetch of the zip central directory.

**`_sofbuddy_http_maps_crc_2`**  
Default (one URL; wrapped for layout):  
`https://raw.githubusercontent.com/`  
`plowsof/sof1maps/main`  
CRC lookup base URL (index 2). Same Range/CRC-list behavior as slot 1.

**`_sofbuddy_http_maps_crc_3`**  
Default: *(empty)*  
CRC lookup base URL (index 3).

**`_sofbuddy_http_show_providers`**  
Default: `0`  
Toggles provider UI inputs.
