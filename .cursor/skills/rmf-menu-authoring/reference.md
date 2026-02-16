# RMF BIBLE — Full Reference

Definitive reference for Raven Menu Format (`.rmf`): terminology, syntax, behavior, and observed extensions. For humans and agents that parse, validate, generate, or transform RMF.

## 1. Source Precedence

1. `rmf_docs/menu_docs.txt` (official RMF v1.00 semantics)
2. `.cursor/rules/rmf_*/RULE.md` (project-level normalized rules)
3. Real menu corpus `rmf_real_menus/*.rmf`
4. `rmf_docs/structure.md`, `rmf_docs/virtual_pixels.md`
5. `rmf_docs/scrappy_docs.txt`
6. `html_renderer/*` behavior (practical quirks; not always game-engine exact)

## 2. Object Model and Terminology

### Runtime hierarchy

1. **Menu System**: global controller; one instance while menus active.
2. **Menu**: one complete screen, attached to the menu system.
3. **Frame**: rectangular partition of a menu; named and referenceable.
4. **Page**: stackable content layer in a frame; top page visible.
5. **Area**: concrete render/interact unit (text, image, slider, etc.).

### Glossary

`action`, `align` (left|center|right), `bolt`, `colour` (32-bit, often ABGR hex), `command`, `cvar`, `font` (small|medium|title), `frame`, `keyname`, `list`, `menu`, `model`, `sp`, `text`, `value`.

### Authoring patterns

- Frame definitions usually only on top-level page; included child pages are area/layout content.
- Common prefixes: `m_`, `t_`, `l_`, `r_`, `a_`. Pages cached on first visit.

## 3. File Rules and Parsing

- Entrypoint: one top-level `<stm>...</stm>`. Fragment files may omit `<stm>`.
- Tags angle-bracketed; `[]` optional. Quoted strings preserve spaces. Page refs omit `.rmf`.
- **Evaluation**: parse-time immediate = `set`; deferred = `cbuf`; visit = `config`; exit = `exitcfg`, `onexit`; timed = `timeout`.

### Menu console commands

| Command | Effect |
|---------|--------|
| `menu <menu> [frame]` | Open/build menu page |
| `intermission <menu>` | Open non-exitable page |
| `reloadall` | Mark pages for reload on next visit |
| `refresh` | Refresh top page |
| `popmenu` / `killmenu` | Pop latest page |
| `return` / `menuoff` | Teardown (return = guarded, menuoff = direct) |
| `select <cvar>` | Selection-area helper |
| `checkpass` / `changepass` / `freshgame` | Password and game flow |

### Ordering

Frames define partition first; area/layout inside active frames/pages. Bare quoted text is flow; `<text ...>` is interactive. Frame composition at page root; child content via `page`/`cpage`/`include`.

## 4. Coordinates, Scaling, Units

- Virtual baseline: width 640, height 480 (aspect-dependent). Effective vertical: 640×480 (4:3), 640×400 (16:10), 640×360 (16:9).
- **Frame** `width`/`height`: virtual (resolution-scaled). **Area** `width`/`height`: **absolute screen pixels**. `defaults border` / `frame border`: often actual-pixel line thickness.
- **Frame %**: `frameWidthPct = frameWidth * 100 / 640`; `frameHeightPct = frameHeight * 100 / effectiveVirtualHeight` with `effectiveVirtualHeight = 640 * screenHeight / screenWidth` (e.g. 1920×1080 → 360). Use frames for scalable structure; avoid building layout with many `<blank>` at high res.
- `backdrop` = primary full-background; `frame backfill` = inner-frame/border fill, not a replacement for `backdrop`.

## 5. Color and Text

- `<tint name 0xAABBGGRR>` (ABGR). String packages: `<strings PACKAGE>` then `^TOKEN^`. Default tints in `rmf_real_menus/tints.rmf`: `vbargray`, `normaltext`, `hilitetext`, `backtint`, `tipback`, `tip`, `icontint`, `iconhilite`, `hgreen`, `hblue`, `orange`, `red`, `blue`, `green`, `purple`, `cyan`, `clear`, `black`, `transblack`, `semiblack`, `smokey`, `gray`, `white`, etc.

## 6. Control Elements

| Keyword | Purpose |
|---------|---------|
| `stm` | Root block; options: nopush, waitanim, fragile, global, command, resize |
| `frame` | Define frame: name, width, height, page/cpage/cut, border, backfill |
| `key` / `ikey` / `ckey` | Page-level key/action/conditional bindings |
| `comment` | Ignored |
| `includecvar` | Inject RMF from cvar |
| `include` / `cinclude` / `cninclude` / `exinclude` / `einclude` | Include pages (see truth table in SKILL.md) |
| `alias` | Alias command sequence |
| `set` / `fset` | Cvar set (immediate/typed) |
| `cbuf` / `config` / `exitcfg` / `timeout` | Deferred/visit/exit/timed |
| `onexit` (undocumented) | On exit: cvar + menu file |

### Frames vs br/hbr

Frames operate above layout. `br`/`hbr` only affect area placement **inside** a frame’s active page. Frame definitions processed in order; `0` width/height = fill remaining; wrap goes below at left edge of last auto-filled frame. Fixed-width frame to the left = permanent left margin for everything below. For clean top borders/header bands, define frames in completed horizontal layers.

## 7. Layout Elements

| Keyword | Purpose |
|---------|---------|
| `tint` / `font` | Color and font (type, tint, atint) |
| `br` / `hbr` | Soft break (local) / hard break (commits top) |
| `center` / `normal` / `left` / `right` | Layout mode (persistent) |
| `cursor` / `defaults` | Cursor style; default border/tiptint/tipfont |
| `translate` / `emote` / `semote` / `demote` | Server/emote |
| `autoscroll` / `dmnames` / `tab` / `strings` | Scroll, DM names, tab stops, string package |
| `bghoul` / `backdrop` | Background model / fill |

### br / hbr behavior

- **`br`**: carriage return; move next position down by font height, reset X to left. On layout mode change, next position snaps to current `top` then moves down.
- **`hbr`**: advances `top` anchor; layout changes don’t jump back up.
- **Frames exist outside the `<br>` system.** `<br>`/`<hbr>` live **within** frames. Layout change on a `<br>` line = reset of local `<br>` (like clear / typewriter back to top); multiple “threads” in same `<hbr>` cluster. Everything before `<hbr>` is cemented. `<br>` = weak/temporary; `<hbr>` = hard/committing. First line = implicit `<hbr>` line.

Example (layout resets with borders and blanks):

```text
<stm>
<defaults border 1 1 white>
<blank 16 100>
<defaults border 1 1 cyan><br><normal>
<br><blank 16 16>
<defaults border 1 1 green><center>
<blank 2 2><blank 16 16>
<defaults border 1 1 blue><right>
<blank 16 45><blank 16 16>
<defaults border 1 1 orange><right>
<blank 16 80><blank 16 16>
</stm>
```

Top = last `<hbr>`. `<br>` = temporary stepping stones; layout change resets `<br>` stack. Areas grow right; first line = `<hbr>` line.

## 8. Common Area Modifiers

`key`, `ckey`, `ikey`, `tint`, `atint`, `btint`, `ctint`, `dtint`, `noshade`, `noscale`, `border`, `width`, `height`, `cvar`, `cvari`, `inc`, `mod`, `tip`, `xoff`, `yoff`, `tab`, `noborder`, `bolt`, `bbolt`, `align`. Reverse-engineered: `next`, `prev`, `iflt`, `ifgt`, etc.

## 9. Area Elements

`blank`, `hr`, `vbar`, `text`, `ctext`, `image`, `list`, `slider`, `ticker`, `input`, `setkey`, `popup`, `selection`, `ghoul`, `gpm`, `filebox`, `filereq`, `loadbox`, `serverbox`, `serverdetail`, `players`, `listfile`, `users`, `chat`, `rooms`. File sort: `name`/`rname`, `size`/`rsize`, `time`/`rtime`. Area width/height = absolute pixels. For exact tint on list/ctext use `noshade` and area-level tint.

## 10. Undocumented / Extended

- **hbar**: `<hbar "BACKGROUND" "BAR_ITEM" cvar PROGRESS_CVAR [invisible]>` (0.0–1.0).
- **botlist**: `<botlist cvar menu_newbot>`.
- **bot**: `<bot menu_newbot tab headings "Name,..." align center>`.
- **onexit**: `<onexit menu_settings_changed altersnd>`.

## 11. High-Value Quirks

- `cinclude` = cvar non-zero; `cninclude` = cvar zero. `exinclude` order: `<cvar> <pageIfZero> <pageIfNonZero>`.
- `ctext` often has `invisible`; `text` has `regular`. `serverbox` columns 0–5 (5 = pure). `filebox` uses `align` in practice.
- `chat` with `private`. Area parsers can be option-order sensitive. Most tags not explicitly closed.
- `return` = guarded teardown; `menuoff` = direct. `defaults noborder` + selective borders for cleaner layouts. Prefer frames over blank-heavy layout for scaling. Use `noshade` when list/ctext looks muted.

## 12. Agent Authoring Checklist

Single `<stm>`; extensionless includes; tints/strings before use; intentional layout transitions; valid key/cvar bindings; conservative filebox/filereq options; undocumented tags only if supported; frame/cut names valid; layout mode switched back after left/normal; br vs hbr intentional.

## 13. Minimal and Frame-Cut Templates

Minimal: `<stm><frame default 640 480 page main></stm>`.

Frame-cut: `<stm><frame topic 0 92 page t_header center><frame main 640 372 border 40 0 clear backfill clear><frame content 0 0 cut main cpage menu_mapname></stm>`.

## 14. Rect Type IDs (reverse notes)

0x00 setkey/serverbox, 0x01 slider, 0x02 ticker, 0x03 ghoul, 0x05 image, 0x06 text, 0x07 ctext, 0x08 list, 0x09 input, 0x0A filebox, 0x0B filereq, 0x0C loadbox, 0x0D popup, 0x0E blank, 0x0F rule/hr, 0x13 serverdetail, 0x14 players, 0x15 hbar, 0x16 vbar, 0x17 selection, 0x18 listfile, 0x19 botlist, 0x1A bot, 0x1B users, 0x1C chat, 0x1D rooms, 0x1E demote.
