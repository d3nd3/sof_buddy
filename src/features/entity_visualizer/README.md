# Entity visualizer

## Gate and commands

- **`_sofbuddy_entity_edit 1`** — required for map cache, `sofbuddy_entities_draw`, `ev_debugbox`, verbose logs, and attack-intersect. Map study can turn this on before `map`.
- **`sofbuddy_entities_draw`** — merged wireframes: BSP cache rows first, then live `g_edicts` only for keys not already present (dedupe: `classname` + quantized world AABB). Sorted map order → stable output.
- **`_sofbuddy_entities_draw_verbose 1`** — per-entity `PRINT_DEV` lines when drawing.
- **`ev_debugbox`** — sanity-check FX on the local player.

With **`_sofbuddy_entity_edit 0`**, the level cache is not filled from the BSP string; enable the cvar and load a map (or Map study load) to populate it.

---

## Level-entity cache (BSP string)

**Source:** The engine calls `ge->SpawnEntities(mapname, CM_EntityString(), spawnpoint)` — the same **BSP entity lump** (`LUMP_ENTITIES`) as in the `.bsp`. It lists every placed entity **before** gameplay frees or moves them (e.g. **`light*`** after light bake).

**Hook:** After **`Sys_GetGameApi`**, Buddy replaces **`game_export_t::SpawnEntities`** (fourth pointer, offset **`0x0C`**, see `SDK/sof-sdk/Source/Game/gamecpp/game.h`). No **`SpawnEntities` RVA** in `detours.yaml` — the export struct is the stable handle.

**Flow:**

1. **`OnSpawnEntitiesPrologue(entities)`** — if the cvar is on, parse **`entities`** like **`ED_ParseEdict`** (`COM_Parse` rules: `{` … `}` blocks; `_` keys skipped). Fill **`s_level_entity_cache`**.
2. Call the original **`SpawnEntities`** (`g_spawn.cpp`).
3. **`OnSpawnEntitiesEpilogue()`** — end **`ED_CallSpawn`** duplicate suppression for that pass.

During step 2, **`ED_CallSpawn`** does **not** append duplicate rows into the level cache (BSP parse already did). **`worldspawn`** does **not** clear that cache (old clear wiped BSP-filled rows before draw).

**Files:** `ev_draw.cpp` (parser + draw), `ev_spawn_entities.cpp` + `ev_gamedll_loaded.cpp` (`InstallSpawnEntitiesWrapper` on `GameDllLoaded`), `ev_internal.h`.

**Cache vs live:** Cache rows come from entity **classname** / **origin** / optional **mins**/**maxs** in the lump. Live merge adds movers and skips bad pickup origins; lights may be gone in-world but remain in the cache.

---

## Wireframes (FX line quads)

`sofbuddy_entities_draw` uses the client FX line path (`FX_AllocNewQuad`, `FX_SetQuadAsLine`, `FX_SetQuadLineGenericInfo`, `FX_ClearQuads`). Roughly **4** line quads per box; raise **`cl_max_quads`** if you hit the pool cap.

Implementation pitfalls (SoF.exe / ref_gl) are summarized as constants and comments in **`ev_internal.h`** (`kCQuad*`, palette): **`+0x34`** must be a **`GL_FindImage`**-loadable name (not a small integer), **`+0x6C`** expire time, **`+0x68`/`+0x8C`** vs **`fxCurTime`** (pointer vs float bits). Details live next to **`ReadFxCurTimeForExpire`** / box setup in **`ev_draw.cpp`**.

---

## Detours used by this feature

`detours.yaml`: **`ED_CallSpawn`**, **`G_FreeEdict`**, **`ClientThink`**, FX helpers, **`SCR_BeginLoadingPlaque`** (pre), plus **`Sys_GetGameApi`**-installed **`SpawnEntities`** replacement as above.
