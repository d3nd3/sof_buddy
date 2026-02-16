---
name: rmf-menu-authoring
description: Reference for creating and validating Raven Menu Format (.rmf) menus. Use when authoring or editing .rmf files, parsing or transforming RMF, or when the user asks about RMF syntax, frames, areas, layout, br/hbr, or menu console commands.
---

# RMF Menu Authoring

## Source precedence (when sources disagree)

1. `rmf_docs/menu_docs.txt` (official RMF v1.00)
2. `.cursor/rules/rmf_*/RULE.md`
3. Real corpus `rmf_real_menus/*.rmf`
4. `rmf_docs/structure.md`, `rmf_docs/virtual_pixels.md`
5. `rmf_docs/scrappy_docs.txt`
6. `html_renderer/*` behavior (quirks, not engine-exact)

## File rules

- Extension `.rmf`; location `base/menus` or `user/menus`.
- **Entrypoint**: exactly one top-level `<stm>...</stm>`. Content before `<stm>` and after `</stm>` ignored.
- Fragment files (included via `include`, `cinclude`, etc.) may omit `<stm>` and contain raw body.
- Tags angle-bracketed; `[]` optional, `<>` required. Quoted strings keep spaces. Page refs omit `.rmf`.

## Coordinates (critical)

- **Frame** geometry is **virtual** → % of screen. **Area** `width`/`height` (e.g. `blank`, `selection`) are **absolute screen pixels**.
- Prefer `<frame>` for scalable layout; `<blank>` does not scale (only 640px at 640 wide; at 1920 width frame scales, blank stays 640px).
- Frame %: `frameWidthPct = frameWidth * 100 / 640`; `frameHeightPct = frameHeight * 100 / effectiveVirtualHeight` with `effectiveVirtualHeight = 640 * screenHeight / screenWidth` (e.g. 16:9 → 360).

## Frames vs layout

- **Frames exist outside the `<br>` system.** `<br>`/`<hbr>` and layout (`center`/`normal`/`left`/`right`) apply only **inside** a frame’s active page.
- Frames are placed in sequence; width/height `0` = fill remaining (auto). When a row is “filled,” next frame wraps below at the **left edge of the last auto-filled frame**. A fixed-width frame to the left of that auto frame acts as a **permanent vertical margin** (wrapped frames cannot use that space). To get a clean top/header band, define frames in completed horizontal layers.

## br vs hbr

- **`<hbr>`**: hard break; advances the “top” anchor. Everything above is cemented; layout changes cannot move back up.
- **`<br>`**: soft break; step down by one font line. **Layout mode change resets the `<br>` stack** back to the current “top” (last `<hbr>`). So `<br>` is local/temporary; `<hbr>` is committing.
- First line of a page is treated as an implicit `<hbr>` line. Layout change = clear the current “sand castle” of `<br>` steps but keep what was already drawn at the top of that cluster.

## Include conditions

| Tag | When |
|-----|------|
| `include` | always |
| `cinclude` | cvar ≠ 0 |
| `cninclude` | cvar == 0 |
| `exinclude` | `<cvar> <pageIfZero> <pageIfNonZero>` |
| `einclude` | include when cvar equals value |

## Menu console commands (main)

`menu <menu> [frame]` | `intermission <menu>` | `reloadall` | `refresh` | `popmenu` | `killmenu` | `return` | `menuoff` | `select <cvar>` | `checkpass` | `changepass` | `freshgame`. `return` = guarded teardown; `menuoff` = direct teardown.

## Authoring checklist (before emitting .rmf)

1. Single `<stm>...</stm>` root.
2. Included page names extensionless, resolvable from menus path.
3. Tints defined before use (or raw hex).
4. String packages loaded before `^TOKEN^`.
5. Layout mode changes intentional (persist until changed).
6. Area key/cvar bindings use valid commands and cvars.
7. `filebox`/`filereq`: conservative option order.
8. Undocumented tags (`hbar`, `bot`, `botlist`, `onexit`) only if runtime supports.
9. Include truth: 0 = false, non-zero = true.
10. Frame/cut names valid; with `defaults border` confirm you don’t over-box; switch layout back after `<left>`/`<normal>` sections; for scaling use frames not blank-heavy layout; use `<br>` vs `<hbr>` intentionally.

## Minimal valid RMF

```text
<stm>
  <frame default 640 480 page main>
</stm>
```

## Typical interactive skeleton

```text
<stm>
  <include tints>
  <strings menu_generic>
  <defaults noborder>
  <vbar tint vbargray>
  <font type title tint normaltext atint hilitetext>
  <center>
  <text "^MENU_GENERIC_TITLE^">
  <hr width 256 tint gray>
  <list "Off,On" cvar my_toggle atext "Toggle : " key mouse1 "set changed 1">
  <slider cvar my_value min 0 max 1 step 0.1>
  <text "^MENU_GENERIC_APPLY^" key mouse1 "exec apply.cfg" border 16 1 gray>
</stm>
```

## Full reference

For the complete keyword registry (control, layout, area elements), tint list, evaluation timing, quirks, and reverse-engineering notes, see [reference.md](reference.md).
