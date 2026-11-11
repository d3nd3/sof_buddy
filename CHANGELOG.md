# Changelog

User-facing highlights. Older detail is summarized at the bottom.

## Unreleased

### Scaled menu

- The 16:9 chrome now forces the `16_9_l` / `16_9_r` side pages to a clear backdrop on load, so a pak or SoFPlus version with different side art cannot leave stale pixels beside the centred menu.
- Integer-mode scale settings survive a restart again. At startup the archived scale was applied before the real video mode was known (`max scale` still read as 1x), so a saved 2x/4x was clamped to 1x and persisted. The stored value is now left alone until the resolution is known; the runtime scale callbacks still clamp the effective scale.

## v8.51

### Scaled menu

- Tiled menu backdrops (`<backdrop tile>`, including `backdrop/web_bg`) now scale their repeat pitch and tile art with `_sofbuddy_menu_scale`. At high menu scales the background no longer stays at a fixed 128 px tile and looks tiny; it keeps the authored proportion to the menu instead.
- Integer scaling mode's slider knobs no longer jump to the left edge of the screen. The integer maximum now uses the real screen height (SoFPlus can expose a virtual 16:9 menu height while pages parse, which pinned the max at 1x and made the slider range zero → NaN), and a 1x maximum is served as a 0–1 slider so the knob stays on the bar.
- Startup disclaimer text keeps correct word spacing at high resolutions (e.g. 4K). The glyph run is now scaled from its start like normal menu text; the old per-quad advance ignored space glyphs, collapsing spaces as the scale grew.
- The menu cursor no longer shrinks while an `<stm resize>` dialog is open (player/precache popups). Resize pages pin their canvas scale to 1.0, which previously scaled the cursor and hotspot down to stock pixels at higher resolutions; the cursor now keeps the normal menu-page size.
- F12 navigation tabs are now centred inside their bordered boxes at every menu scale and resolution. Labels use draw-time `talign center` measurement instead of leading spaces, so the old 3 px-per-space assumption can no longer push text into the right border at fractional scales such as 800×600 at 1.25×.
- `<vbar>` height now follows the parsed page content instead of the frame height. Internal F12 frames use height `0` (full-screen sentinel), which previously gave the vbar a zero-height track; a cleanup hook sizes it to the content (clamped to the screen) before the thumb is measured. The invisible left/right content-inset blanks no longer inflate the page height, so the thumb length reflects the real list length and the page no longer scrolls into empty space.
- The small black version stamp `ctext` in `version.rmf` now scales with the rest of the menu text instead of being forced to stock size.
- Load Game and Save Game pages scale again. The loadbox width is divided by the engine canvas scale before `loadbox_c::Parse` applies it, so the save list stays onscreen instead of rendering empty at higher menu scales. Save preview thumbnails and their text lines now scale and recenter with the menu scale too.
- Frame borders now use the vertical design ratio (`vid_h / 480`, 2.25 at 1080p) for both axes at every menu scale. At 1× the scaled-menu hook previously fell back to the stock horizontal canvas (`vid_w / 640`, 3.0 at 1080p), so the authored 40 px frame border rendered as 120 px instead of 90 px. The oversized border left the Join server list's `<serverbox>` too wide to place, so the list rendered empty; the frame border is now constant across menu scales and the normal tab grid fits. Rect/content borders still follow `menuScale`.
- SoFPlus `_ss` menus are deprecated: any `_ss` menu name is redirected to its `_ns` variant at load time (toke hooks plus an FS-path fallback), and the `_ss`-specific frame, border, topic, and join-menu corrections were removed. When SoFPlus selects the `_ss` variant (its `_spf_cl_video_shortscreen` wide-screen test), the `_ns` replacement gets the 16:9 chrome injected: a full-height `16_9_center`, 80-wide side frames, and a 480-wide `ss_content` frame (at 1920×1080) carrying the `_ns` page with `cut ss_content`. The `_ns` page's `bghoul` `xoff`/`yoff`/`scale`, `ss_content` frame widths, and bolted ticker box are adjusted by the vertical-vs-canvas ratio (0.75 at 16:9); frame borders use the same scale. This also fixes the full-screen main-menu ticker, which has no `ss_content` wrapper. Bolted tickers draw with a full-screen scissor, and their vanilla 480p speed scales with resolution, independent of `_ns`/`_ss`. Frame height correction now applies to every frame except the internal `sof_buddy/*` pages. `<vbar>` widths, cap pictures, and scrollbar thumb width now scale with the menu scale instead of staying at the authored 16 px. The `_ns` page's `<tab>` column grid is scaled by the same vertical-vs-canvas ratio, so the server box keeps its columns onscreen inside the `ss_content` centre. Changing the menu scale while a page is open no longer jumps the page back to the top: the reload preserves each frame's scroll position as a fraction of the content height, and a reload is skipped when the rounded menu scale did not actually change.

### Updater / release channels

- Startup update notifications now default to **stable-only**; users can still opt into startup checks for the selected unstable channel.
- Replaced the `stable.json` pointer with a branch-based channel model: stable releases are the newest non-prerelease GitHub release from `master`, while unstable builds are prereleases tagged `vX.Y-buildN-unstable` from the `unstable` branch. Removed `releases/stable.json` and `tools/set_stable_release.py`.
- The F12 → Version page now shows **Stable tag** and **Unstable tag**, with **Install stable release** and **Install unstable build** actions; legacy `_sofbuddy_update_target_tag latest` values map to `unstable`.
- The release list now lives in its own framed, independently scrolling `<vbar>` pane and is capped at 12 recent rows, so version history no longer grows indefinitely.
- Unstable builds include the local build number in update comparisons, so users on an older unstable build are offered newer prereleases even when `VERSION` has not changed.
- CI now builds and publishes prereleases from the `unstable` branch; stable releases are still published from `master` when `VERSION` changes.

## v8.50

### Updater

- Fixed corrupt updater output (`'ho.' is not recognized`, repeated prompts) by shipping `.cmd` scripts with CRLF line endings, running the updater from a temporary copy, and installing updated scripts as `.new` sidecars that the DLL promotes on the next game start. The running script is no longer overwritten mid-extraction.
- Fixed Wine updates: host script extraction is invoked through `bash -c`, waits for the completion marker instead of trusting `start /unix`'s exit code, and correctly passes paths containing spaces.
- Feature-setting prompts now apply to **universal** builds only. Compile-time channels keep their existing `features.cfg` and are never asked about feature toggles.
- On universal builds, updates now **merge** newly recommended feature defaults into the saved toggles instead of replacing the feature file, preserving explicit overrides. The installer/updater backs up the previous `features.cfg` as `features.cfg.prev`; the DLL applies the merge at the next start and removes the backup. The GUI checkbox, headless prompt, and `_sofbuddy_update_replace_features` default to merging; **Reset feature toggles** still discards the package file, archive overrides, backup, and legacy entries so compiled defaults apply.
- Fixed literal `^` characters showing in `(y/N)` prompts.
- README, installer/updater docs, and the new `docs/FEATURES_FLOW.md` now describe feature-file ownership, startup order, reset, and merge behavior.

### F12 menu

- Menu `<pic>`/`<image>` widgets now scale with `_sofbuddy_menu_scale`, so weapon-info and tab images follow the menu scale. Bolted tab icons, images marked `noscale`, and resize pages whose canvas already carries the scale are left authored.
- Deleting `base/sofbuddy_features.cfg` now resets feature toggles to the installed defaults (or compiled defaults when `features.cfg` is missing/auto-generated). Runtime selections no longer overwrite `sof_buddy/features.cfg`.
- `<hbar>` progress bars now scale their cap, repeated-tile pitch, and tile art with `_sofbuddy_menu_scale`, keeping layout centring and the progress fraction correct.
- Menu frames now keep the stock canvas-scaled width, with only a height-only 4:3→widescreen correction for non-`_ss` frames. `_sofbuddy_menu_scale` no longer resizes frames, fixing shrunken/misaligned Join and other framed menus at 1×–2× scales (for example 800×600 at 1.19).
- Smooth scaling now stays at 1× when the vertical maximum is below 2× (for example 800×600), matching the nearest-integer sub-2 rule instead of allowing fractional 1.19× scales.
- Internal `loading/*` menus no longer bypass the frame height correction. They now receive the same non-4:3 vertical frame adjustment as stock loading pages, fixing the bottom-anchored Disconnect layout at high menu scales.
- Removed the join-wide 0.75 content-scale workaround. `join_ss` now gets a targeted load-time layout correction: the server frame Y offset is scaled from 120 to 90 using the fixed 16:9 virtual height (360/480).
- Images with explicit `width`/`height` (such as the GameSpy logo in `join_ss`) are no longer scaled twice; the rect parse handles their fixed dimensions and `pic_c::Setup` skips them.
- F12 tab labels now account for separate menu and text scales when centering, and the tab-row prefix is centered in authored units before scaling, so the sixth tab no longer wraps off-screen at 2×.
- Profile GUID help text now wraps onto separate lines, and slot-0 crosshair picks from some packs no longer crash from an incorrect `Draw_GetPicSize` lookup.

## v8.49

### Installer / menus

- **Graphics → Video** now starts with a `gl_mode` resolution picker: current active mode plus the SoFPlus `_sp_cl_vid_mode_0..9` entries, applying with `vid_restart`.
- Fixed cvar lookup missing SoFPlus cvars registered after the menu system initialized; this also restores the resolution list when `_sp_cl_vid_mode_*` were created late.
- Legacy `cl_maxfps` quantization now rounds down to the largest listed legacy cap not above the requested value, so `150` settles to `143` (7ms) instead of `167` (6ms); the engine default `30` is also a listed cap, so it is no longer rewritten to `25`. With Exact FPS off this normalization runs on every cvar write (startup/config restore, console, and menu changes), not only when the CPU page opens. SoFPlus SPM persistence now flushes once the post-restore frame/screen hooks are live, so runtime changes are written to `User/sofplus/data/spm_settings.cfg` immediately.
- Fixed the media-timers custom main loop bypassing the `Qcommon_Frame` hook entry; Qcommon pre/post callbacks (HTTP maps, crosshair preview, master fallback, and config/SPM saves) now run again when the custom loop is active.
- Feature toggles now persist separately in `base/sofbuddy_features.cfg`, leaving normal SoF Buddy settings in `base/sofbuddy.cfg`. Setup resets the wizard to the **Recommended feature set** on upgrades; when Buddy is already installed it offers **Expecting a new feature? Reset feature toggles**, which clears only the feature file and legacy feature lines so new defaults apply.
- The graphical and command-line updaters gained a separate **Reset feature toggles** option (`SOFBUDDY_UPDATER_RESET_FEATURES=1` for scripts), independent of the existing fresh-settings reset. The GUI now waits for the user to click **Install**, so the reset checkboxes are applied before extraction.
- Changing an F12 setting whose backing feature is disabled now opens a **Feature Required** prompt and suppresses the dead cvar write; `Y` jumps to the Features tab. This also prevents the “cvar exists in config but does nothing” confusion on universal builds.
- **Check for updates** dialogs now scale and center correctly at high resolution; the up-to-date dialog uses the same global/nopush behavior as the other update prompts.
- The F12 → Version release picker now uses clickable Stable/Latest channel and release-tag rows plus a live **Selected install target** line instead of a one-line cycling list. Enable **Show non-stable tags** and press **Refresh release list** to load older tags.
- **Check for updates** is now limited to once every 10 seconds with a cooldown status; release-list refresh has the same 10-second cooldown, and update checks are guarded against re-entry to prevent spam from stalling or crashing SoF.
- Removed the `_sofbuddy_font_scale_auto_axis`, `_sofbuddy_hud_scale_auto_axis`, and `_sofbuddy_scale_auto_axis` cvars; font, HUD, and menu auto scaling are now always vertical (`vid_h / 480`). The UI Scale axis rows were removed.
- Removed `_sofbuddy_crossh_scale_auto_axis` as well; crosshair auto scaling is now always vertical (`vid_h / 480`).
- UI Scale now uses per-scale integer percentage sliders (`0%` = `1×`, `100%` = vertical max) with a **Reset to default (75%)** button, replacing the manual step list and Manual/Auto/Max/Min preset cvars. Percentages are archived and reapplied on startup and resolution/video changes.
- Added a **Scaling Mode** toggle. **Smooth** maps 0% = 1× linearly up to the vertical max at 100% (`1 + (max − 1) × pct/100`); sub-2× results round to the nearest integer (1× or 2×), while 2× and above stay fractional. It uses cached integer-prescaled textures for HUD/crosshair/scoreboard images before linear downscaling. **Integer** shows an integer scale slider and floors font, HUD, menu, and crosshair scales to whole numbers.
- Smooth mode now bypasses the 0.25 scale snapping for direct/direct-cvar scale values as well; switching modes reloads the conditional slider pages.
- Scale sliders now defer cvar updates until mouse release (120 ms settle), while the slider knob previews the pending value during the drag. Menu-scale sliders reparse the open UI Scale page after release, so the menu actually resizes instead of only updating the displayed value.
- Menu tooltip default appear delay is now 750 ms (was 250 ms); Restore Defaults writes the new value.
- Menu tooltip duration default is now 15000 ms (was 5500 ms), and the duration slider range extends to 30000 ms.
- F12 scale changes now persist immediately instead of relying on the shutdown save, and any pending deferred slider value is flushed before saving. Font/HUD/menu scale changes survive restarts.
- Load Game and Save Game pages now ignore the scaled-menu layout and stay in stock scale. This keeps the save/load box populated when the integer menu scale is changed at runtime (previously the save list could render empty at 2x).
- Fixed switching between Integer and Smooth scaling modes leaving the old mode's slider (and therefore the wrong max percentage) on the UI Scale page. The mode change now schedules a page reparse after the cvar has actually changed, instead of relying on the list's early reload. The engine's integer cvar flag is also cleared before applying smooth percentages, so values like `2.25` are no longer truncated back to `2.0` after visiting Integer mode.
- Tooltips now appear only after the configured continuous hover delay and disappear immediately when the cursor leaves the hotspot. The tip renderer tracks each `stm_c`'s current hovered rect instead of the shared `menu_c+0xBC` cache, so tooltips on every stock and F12 frame behave consistently.
- F12 tab labels now use scale-aware leading padding instead of fixed leading spaces, so labels stay centered inside their boxes at 1× and higher menu scales.
- F12 styling cleanup: **Scaling Mode** is now a normal setting row instead of inheriting the green section-title color; non-interactable info lines use the CPU tab info color (`Current`/`gl_mode`/current scale/profile values), including labels above sliders. UI Scale sections use short `==FONT==`/`==HUD==`/`==MENU==`/`==SPRITES==`/`==CONSOLE==` headings with descriptive tooltips, and the page has separators between each section.
- Changing the Graphics resolution now restarts video and reopens the Graphics page, so the menu is re-parsed at the new resolution instead of staying at the old layout.
- Navigation now orders **Graphics, Gameplay, Lighting** before **Network**.
- `game_ss` now gets a small `border 8` content inset plus its authored 92→96 topic-height correction. The border-scaling hook applies the vertical `vid_h/480` factor only for `_ss` frame borders, avoiding double-scaling; other `_ss` menus keep their authored `border 0 0`.

## v8.48

### F12 menu

- **Adaptive nav bar**: 6×2 tabs on 16:9/16:10/ultrawide, 3×4 on 4:3/5:4, with the tab labels centered in their boxes.
- **Fixed navigation**: the tab bar now has its own frame, so the content scrollbar no longer moves the nav; switching tabs never requires scrolling back to the top.
- **General is now the second tab**, followed by the rest of the sections; nav tinting follows the selected theme and the nav font stays readable at the Small font preset.
- **Readable widescreen content column**: text is constrained to a height-derived 4:3 width (`height × 4/3`), e.g. 1440 px with 240 px side spacers at 1920×1080. It adapts to every resolution and is full-width on 4:3.
- **F12 first-open fix**: opening F12 before the stock main menu no longer inherits the startup menu's page layout; the overlay builds against a clean menu context.
- **Menu font size style**: Small / Normal / Large / Extra Large now select real `<font type>` tiers instead of a scale multiplier; default is **Small**.
- **Tooltip controls**: separate tooltip font (Small / Medium / Title, default Small), appearance delay default **250 ms**, and tooltip duration default **5500 ms**; both timing sliders are integer-only.
- **Auto crosshair scale**: new axis selector (Horizontal/Vertical, default Vertical) and auto toggle; when On it tracks the selected resolution axis and hides the manual scale row. Default remains Off.
- **Theme updates**: added **SoF1**, a palette based on the stock SoF menu (olive/charcoal panels, parchment text, cyan/teal headings and orange selection); **Cob2** is now the default theme.
- **Page polish**: Features is centered and no longer shows literal `hbr`; Lighting section `hbr` text fixed; Network headings, Gamespy Master, Current map, and HTTP provider fields are centered; texture filtering is one compact section; CPU Profile controls moved above Effects/debris under **Improve Frame-rate here.**; profile state reads **Would-Set** unless the active profile is Custom/Current; notify-duration labels show seconds.
- Removed Cvars/Version bloat text; added a max-anisotropy tooltip; Cvars tab now documents the current menu, tooltip, font, sound, hotkey, font-pack, and crosshair cvars with current defaults.
- **Quiet Alt-Tab sound messages** (default **On**): filters `Creating DS buffers`, `...using secondary sound buffer`, and `Destroying DS buffers` from `Com_Printf` when DirectSound reactivates after Alt-Tab.
- **Join menu scaling:** `menu join` now uses `max(0.75 × menuScale, 1)` while auto menu scale is on or manual menu scale is above 1, scoped to that menu only.

### Scaled Menu / UI scaling

- Reworked menu scaling around actual screen pixels for frames, rects, borders, text, tooltips, sliders, tickers, version stamps, and internal menu pages.
- **Sliders** now scale their bar, button, cap, and flow-layout dimensions with `menuScale`.
- **Manual menu scale** changes reparse the open page immediately, so each 0.25 step reflows correctly instead of shifting text.
- **Ticker speed** now uses the height-derived 4:3 width scale, keeping scroll speed a fixed percentage of that content width at any resolution; ticker delay remains authored.
- Split font/HUD/menu auto-scale axes and added `screen_x_scale`; scale-rounding and resize/intro/disclaimer paths now use the correct scale.
- Version-stamp rendering uses a full-screen scissor and bypasses menu glyph scaling, fixing the stamp when the menu scale is large or the menu is first opened.
- Cinematic/typewriter text now keeps character spacing and glyph size on the same HUD scale; menu scaling no longer stretches positions without scaling the glyph quads (large gaps between letters).
- Internal `sof_buddy/*` frames remain computed in the stock menu space while their content uses `menuScale`; `_ss` and loading-menu frame handling was corrected.

### Updater / other fixes

- Fixed updater menu reloads: `Cmd_ExecuteString` does not split on `;`, so reloads now issue `reloadall`, menu open, and `refresh` separately; non-startup update flows reload their menus at the right time.
- **List Match Fix** no longer hides off-list cvar values; numeric zero still uses the first label without changing the cvar.
- Installer feature grouping now places **Intro Scaler** and **Scaled Menu** under recommended **Menu Features**; installer version metadata updated.
- Crash backtraces now include module name + offset so reports remain usable when symbol lookup fails.
- Documentation updated for the new F12 layout, font/tooltip settings, theme, scaling, ticker, and Cvars reference.

## v8.47

### Updater (F12 → Version)

- **Refresh release list** reopens the current F12 page after fetch so the install-version picker updates (fixes stale `<includecvar>` list on global menu pages).
- Filtered list includes **Stable**, **Latest**, blessed stable tag, and **newest feed tag** when not showing all releases.
- **`_sofbuddy_update_startup_stable_only`** defaults to **Off** (startup check follows **Install version** unless you enable stable-only notify).

### Menus

- **Features** tab (universal): **Recommended / tested** and **Optional / unstable** sections match the Windows installer grouping (`tools/generate_features_menu.py`).
- **General** tab: centered section headers, bonus menu key row, and action buttons (removed left/tab indent that broke alignment).
- **Lighting** tab: **Lightblend** section (title, warning, Src/Dst lists, overbright note) stays centered with the rest of the page.

## v8.46

### Crosshairs (F12 → Gameplay)

- **Live preview** tracks the crosshair list selection (`status/chN` for stock **S0–S4**, `status/sbxch_{theme}_{slot}` for packs) via draw/`GL_FindImage` redirects without unsafe menu reloads or `SCR_UpdateScreen` overlays.
- **Stock preview fix:** recognize vanilla `status/ch0`–`ch4` paths correctly so **S0/S2/S3/S4** no longer all look like the **S1** dot; bundled themes unchanged.
- **Pick applies immediately:** list `SetValue` and pick cvar sync theme + engine **`crosshair`** slot; deferred pic refresh avoids crashes when switching pack themes (e.g. FL44).
- **Extra crosshair `.pak` discovery** in `base`/gamedir (non-`pak0`–`pak9`, skips duplicate buddy catalog names); short menu labels and regenerated catalog/RMF.
- **`_sofbuddy_crosshair_tint`** on stock **S0–S4** during `SCR_DrawCrosshair` (`glVertex2f` hook).
- **Stock art routing:** theme **0** loads `ch0`–`ch4` from retail `base`/gamedir only (not `sof_buddy/` mounts); pack slots use `sbxch_*` and `crosshair_pic_ptr` sync for slot **0** picks.

### Other

- **Crosshair scale** doc: F12 → **Gameplay** (not UI Scale).
- **Release tooling:** `tools/new_release.sh` supports SoF2 product tags; CI ignores `sof2/**` path-only pushes on the main workflow.

## v8.45

### Windows installer — optional SoF Plus client

- **Detect retail layout** from the GOG violence / parental-control cvar cluster (`0x201373BF`–`0x20137453`); **CD** when that cluster is absent (file size is not used).
- **Detect KB4571756-style exe fix** when UTF-16 `Raven Software` is cleared at `0x2015F1C0`; maps to `gog` / `gog-kb4571756` or `cd` / `cd-kb4571756` patch kits.
- Bundled **20140531 SoF Plus client patch kits**, `install_sofplus_client.ps1`, and installer task; skips Raven compat script when the exe already has the KB fix.
- Docs: `docs/WINDOWS_INSTALLER.md`, `tools/test_sof_install_detect.py`.

### Updater / stable channel

- **Stable release pointer** (`releases/stable.json`, `raw.githubusercontent.com` feed) and in-game **Stable vs Latest** install target.
- **Release list** filtering, downgrade flow RMF, GUI updater improvements, and expanded `tools/update_test` (robustness + `verify_updater.py`).
- SofVault mirror script notes for stable JSON sync.

### Other

- F12 update prompt RMF refactor (`update_prompt_content.rmf`, downgrade menus).
- Minor crosshair `FS_LoadFile` / UI-scale menu tweaks.

## v8.44

### Crosshairs (F12 → Gameplay)

- **Unified crosshair list** generated from real pack contents (`_sofbuddy_crosshair_pick` sets theme + stock slot **0–4**); legacy `crosshair` values **5+** still migrate on first load.
- **Live menu preview** via `_sb_internal_crosshair_preview_rmf` (`status/chN` or `status/sbxch_{theme}_{slot}`); misc menu reloads when the pick changes so the image updates.
- **Preview rendering:** menu paths use ref_gl **PIC** imagetype (3), not sprite; fixes white squares for pack art while stock **S0–S4** still work.
- **`FS_LoadFile` routing:** serves buddy-pack `m32` for preview and in-game `chN` requests; normalizes `pics/menus/` prefixes; repacks **mhp** / **sauron** paks with correct PACK TOC offsets (fixes `Invalid version` spam).
- **`GL_FindImage` pre-hook** for crosshair menu image paths (sprite retry fallback retained).
- **Stability:** removed nested menu keys from the crosshair list (F12 scroll crash); short list labels stay under SoF’s 240-byte parser limit.

### Release artifacts

- GitHub release zips include **`sof_buddy/crosshairs/*.pak`**; catalog build no longer depends on loose `.m32` sources in the repo (pack-only assets).

### Updater / F12 menu

- Update prompt split into header / notes / footer RMF includes; **`reloadall`** after release-list refresh so open update menus show new notes.
- Main menu content tweaks for update flow.

## v8.43

### Singleplayer framerate (F12 → CPU)

- **Exact FPS cap** (`_sofbuddy_cl_maxfps_exact`, default `0`): with **Singleplayer Max FPS** on, set `1` so `cl_maxfps` is a **literal integer FPS** (10–360, e.g. `65` targets ~65 FPS). With exact off, the legacy **millisecond-budget** list still applies (e.g. list value `63` caps near **62.5** FPS).
- **Bresenham pacing** in the media-timers main loop and a matching **`CL_Frame` gate** (call patch into the DLL, integer-only, applied only while exact is on) so the engine does not early-out on the old float `1000/cl_maxfps` compare.
- **SoFPlus** `_sp_cl_frame_delay` is cleared in exact mode so SoFPlus does not stack a second `ceil(1000/fps)` limiter.
- **Stability:** removed unsafe mid-function detours; restores broken inline `CL_Frame` patches from older test builds; defers the SP listen-server `CL_Frame` NOP until `cl_maxfps` exists.
- **F12 → CPU:** separate RMF widgets for legacy framerate, exact integer cap, and Buddy presets.

### Crosshairs (F12 → Gameplay)

- **Theme picker** uses **`_sofbuddy_crosshair_theme`** plus stock `crosshair` **0–4**; packs load via **`FS_LoadFile`** routing (no `active_crosshair.pak` staging). Catalog/regenerated packs and menu list updated (`tools/generate_crosshair_catalog.py`).

### Other

- **Internal menus:** CPU tab sync for `_sb_internal_feature_cl_maxfps_singleplayer`; assorted tab/RGF layout fixes.
- **VSync display** and **scaled UI** hooks: minor fixes aligned with crosshair asset paths.
- **Docs/README:** exact vs legacy `cl_maxfps` semantics and new cvar table entry.

## v8.42

### Bug fixes

- **Gameplay tab crash/freeze:** the crosshair `<list>` label string exceeded SoF’s 240-byte menu parser buffer; labels are abbreviated in the picker (tooltip still documents packs).
- **Crosshair picker now drives the engine `crosshair` cvar directly:** the shadow `_sofbuddy_crosshair_pick` cvar, `sof_buddy/active_crosshair.pak` staging, and the startup pick/config reconciliation are gone. The list writes values 0–32 onto `crosshair` itself (archived, shared with community configs). SCR_DrawCrosshair's stock 4.0f clamp is bypassed (branch patch at SoF.exe RVA `0x15DE8`), and the new extended values are served by the FS_LoadFile route: an engine request for `pics/menus/status/chN` (5–32) returns that entry's `ch{src_slot}.m32` sprite from its pack under `sof_buddy/crosshairs/`, which also fixes the restart-reset reports; stock rows 0–4 still use vanilla art.
- **`cl_showfps` / `cl_maxfps` not persisting with SoFPlus:** F12/console changes save `spm_settings.cfg` on change and on exit (after SoFPlus’s startup restore), with chained callbacks so SoFPlus’s own handlers still run; programmatic master-server bootstrap no longer overwrites `spm_settings.cfg`.

### F12 menu

- Tab bar: `align center` stacked every tab onto its own centered line (regression of the v8.42 changelog bullet: the engine treats widget `align` as line alignment, not label alignment inside the box). Tabs now flow side by side in a flush 4x3 wrapped grid with uniform 144px boxes (fitting the measured widest label, Profile GUID 121px + 8px border pads), so no label overflows and rows wrap cleanly.

### Updater (developer)

- Local update-test kit: `make update-test` builds old/new universal DLLs, fixture JSON/zips, and `bin/sof_buddy-update-test-kit.zip`; `make update-test-serve` runs the fake feed on `127.0.0.1:8765`. See **DEBUGGING_UPDATER.md** and `tools/update_test/README.md`.
- Update-test DLLs block production GitHub/SofVault URLs unless `_sofbuddy_update_allow_production_urls` is set; supports generic `latest.json` release feeds.

## v8.41

First stable release in this line; **v8.39** and **v8.40** GitHub builds were withdrawn (broken).

### Bug fixes

- **Load/save slots empty** ([#24](https://github.com/d3nd3/sof_buddy/issues/24)): mount `sof_buddy/*.pak` during filesystem init **after** `basedir/base` and **before** the `user` directory, so the `user` search path stays on top and saves remain visible.
- **`sofbuddy.cfg`**: startup load matches save location (`basedir/base/sofbuddy.cfg`); MP profile **`sf_login.cfg`** read/write uses the engine **`user`** cvar path (not hardcoded `basedir/user`).
- **`FS_AddGameDirectory` hook:** automatic detour length so the engine prologue is not split (startup crash).

### Bundled assets

- Release zips include **`sof_buddy/sndrstfindimage.pak`** (placeholder textures so `GL_FindImage` stops spamming the console after **`snd_restart`**).
- **`sof_buddy/extended_skins.pak`** — extended multiplayer player models (skins by **Acadie**). Buddy mounts all `sof_buddy/*.pak` after `basedir/base` (below `user` / `-game` on the search path).

### F12 menu

- **Gameplay:** Audio section (`s_musicenabled`, `s_volume`, `s_khz`, `s_loadas8bit`, **Restart Audio** → `snd_restart`); kill-feed section (`con_notifytime`, `cl_obit_filter`).
- **Gameplay → Crosshair:** stock crosshairs plus community packs under **`sof_buddy/crosshairs/`** (catalog picker, **`active_crosshair.pak`** staging).
- **Lighting / Graphics:** clearer labels (lightmap multiplier, Ghoul lighting block, dynamic light threshold, specular textures); warp chunk size control moved to **Graphics**.
- Tab bar: centered tab labels; **MP Profile GUID** tab width adjusted.
- Loading tips default **`tip_duration`** to **10000** ms (migrates legacy 2500/5000).

### Updater

- Startup update prompt **What’s new** text fixed (chunked RMF cvars for long release notes).
- **`sofbuddy_update prompt`** — check for updates and open the update or up-to-date menu.
- Release notes shown in the prompt strip markdown/link noise for RMF text fields.
- Universal builds default **`_sofbuddy_update_gui_updater`** to on under Wine/Proton as well.
- **Wine `update_from_zip.cmd`:** “fresh settings” prompt and cleanup of `install_context.env` work under Wine `cmd` (no `set /p` colon parse bug).
- GitHub release bodies are generated from **CHANGELOG.md** (`tools/changelog_release_body.py`).

### Docs

- README features list extended skins (Acadie); cvar tables scroll horizontally on narrow viewports.

## v8.38

### Graphics and HUD

- Fix SoFPlus `cl_showfps` OSD scaling: digits no longer overlap when HUD scale is above 1.

### Updater

- **`_sofbuddy_update_use_gui`** on **F12 → Version** (and Cvars): choose graphical `sof_buddy_update.exe` vs script/console install on all builds. Defaults **On** on universal builds (all flavours); **Off** on non-universal channels.

## v8.37

### F12 menu

- Renamed tabs (Version, Cpu, Graphics, Gameplay, WebLinks, General); removed the Updates tab.
- Features tab section headers layout; Profile GUID tab label fits the tab bar.
- Web Links: **Server List megalag** and **Server List 333Net**; installer docs wording updates.

### Graphics and HUD

- SoFPlus top-right FPS counter follows **Auto HUD Scale** / `_sofbuddy_hud_scale` when SoFPlus is loaded.
- Fresh installs (or deleted `config.cfg`) default `gl_mode` to **0** instead of **3** to avoid the community-edition refresh glitch.

### Project

- GitHub issue forms require `_sp_cl_info_sofbuddy` and OS version; repo layout cleanup (`config/`, `scripts/`, `docs/`).

## v8.36

### Build and updater

- Fix release builds when the environment sets a native `CC` (always use MinGW for the DLL).
- Fix compile errors in startup userinfo (`proton` / `wine` channel) and in-game zip extract (directory creation).
- First GitHub release build after repository cleanup (no prior published tags).

## v8.35

- Bundles recent updater, Proton/Wine zip install, F12 flavour/build display, and VERSION-gated CI releases.

## v8.34

### Updater (Wine / Proton / universal)

- In-game zip install extracts in-process on Wine/Proton before quit, with script fallbacks when needed.
- Install context file for cross-platform update scripts; Windows installer can replace a locked DLL via its own helper script.
- F12 **Flavour** shows **Universal (Proton)** or **Universal (Wine)** when detected; Home shows **Build** number.

## v8.33

### Startup update prompt

- Scrollable prompt with installed version, available release, title, asset name, and release notes when an update is queued.

### Build

- CI builds universal DLL and Windows updater reliably; compiler cache key fixed for container workflows.

## v8.32

- F12 **Texture → Resolution**: `gl_mode` picker from SoFPlus video mode labels.
- Faster repeat CI builds (ccache).

## v8.31

- F12 **MP Profile Guid** tab (view, copy, persist GUID).
- Graphical Windows updater (`sof_buddy_update.exe`) on universal channel; console path unchanged elsewhere.
- Default GameSpy master `sof1master.sofvault.org`; installer handles locked DLL at install time.

## v8.30

- With SoFPlus: FPS overlay, framerate cap, and master server persist via SoFPlus settings save on exit.
- Vanilla: `cl_showfps` / `cl_maxfps` still use `config.cfg`.

## v8.29

- Installer verifies installed DLL hash and reports lock/read failures clearly.

## v8.28

- F12 **Network**: master server `<list>` on `_sp_cl_master_gamespy` (MegaLag, 333Networks, SofVault, Qtracker).

## v8.27

- `scripts/cptosof-universal` copies the universal build and bundled helper files to a game folder safely.

## v8.26 – v8.20

- Versioned Windows installer filenames; bonus menu key binding fixes; GameSpy master fallback and health checks.
- Font pack selector (Default / SoFPlus / Vanilla); CPU profile detection on F12; feature rows show **Unavailable** when not in the DLL.
- Translation font priority fix; centered DM scoreboard; installer feature grouping and optional task clarity.

## v8.19 – v8.11

- `list_match_fix` for off-list menu values; build number in userinfo; generated F12 Features tab from compile-time feature set.
- Universal vs standard release zips and updater channel matching; runtime feature toggles in F12 with `sofbuddy.cfg` persistence.

## v8.10 – v8.5

- F12 **CPU**: display refresh readout, refresh picker, VSync vs cap warning, improved `cl_maxfps` list (no crash on open).
- Team icon / player name HUD autoscale; texture filter reapply after `vid_restart`; read-only Cvars reference tab.
- `cl_maxfps` registered for media timer frame budget; Map Study cvars hidden when entity tools are not built in.
- Internal cvar prefix `_sb_internal_*` for bookkeeping (user cvars unchanged).
- Removed experimental focus workaround; safer high-resolution timers on Wine; releases only when `VERSION` bumps.

## v8.0 – v7.0

- Texture UI filter options clamped so menus cannot select invalid mip modes.
- Menu theme presets and Buddy tab; loading-screen options; deathmatch-only custom loading rules.
- Expanded F12 internal menus (tabs, persistence, layout fixes).

## v6.0 – v2.0

- Scaled UI, HUD, console, and menus; raw mouse; HTTP map download; in-game updater; Windows installer.
- HD textures, VSync, lighting blend, master server helpers, console overflow fixes, SoFPlus integration.
- Optional compile-time features (e.g. map entity tools) off by default.
