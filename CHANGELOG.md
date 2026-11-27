# Changelog

User-facing highlights. Older detail is summarized at the bottom.

## v8.61

### Loading screen

- **The Disconnect button is back at its intended height.** The v8.56 header-clipping fix raised both the loading content frame and the header frame from 180 to 240 authored units. The header needed the extra height, but the content frame determines where the included loading-files page starts, so the button moved down by 60 × canvas (180 px at 1080p). The content frame is back to 180 and the header stays at 240, so the Map/Download lines remain fully visible and the button is no longer pinned to the bottom edge.

### Menus

- **Frame normalization is now on by default for every menu.** The 4:3→wide height correction used to skip loose user menus on disk, so a menu the game or a mod spawned from a loose file could keep the stock 640×480 canvas and sit too tall or off-centre at 16:9. Every push now gets the correction, including loose user menus and automatically spawned pages. `menu_native <menu> [frame]` opts a single push out for RMF authors who authored a native canvas. `menu_wide` remains a legacy alias for `menu`, existing keybinds keep working, and `sof_buddy/*` pages still always keep their authored canvas.

## v8.60

### Menus

- **The main-menu ticker is clipped to its tape again.** The ticker scissor was being expanded to the full screen for every bolted ticker, so at 1080p/16:9 the orange tape text ran across the black side bar and appeared before it entered the band. The engine's own ticker-box clip is now left intact, so the text enters and leaves at the band edges and waits hidden.
- **The startup violence disclaimer covers the full screen again.** The backdrop-open test read `__builtin_return_address(0)` from inside a helper; in unoptimized builds that helper adds a stack frame, so the check never matched. The disclaimer then used the virtual-page clip, which stops at the 4:3 page bottom at 16:9, leaving the main-menu console/version stamp visible in the bottom strip. The caller return address is now passed directly. The same fix restores the wide-chrome global backdrop extension.

## v8.59

### Updater

- **Windows XP no longer hits an HTTPS redirect during update checks.** The live SofVault host serves `latest.json`, `latest`, and the `files/` tree over plain HTTP, but root `list.json` and `by_tag/` still return a 301 to HTTPS, which XP's WinHTTP reports as `win32 error 12175`. The updater now reads the HTTP-clean `files/list.json`, `files/by_tag/<tag>.json`, `files/stable.json`, and `files/unstable.json` first, falls back to the current `latest.json`/`latest` release when the full list is unavailable, and logs the exact failing URL instead of only an error code.
- **A normal Windows package running on XP now uses the mirror too.** The updater detects NT 5.x at runtime, migrates archived GitHub or `https://sofvault.org/...` feeds to the plain-HTTP mirror, and selects `release_windows_xp.zip`. Previously only the separate XP package got those defaults.
- **The unstable channel works over HTTP.** The mirror publishes `files/unstable.json` and `files/release_windows_xp_unstable.zip` next to the stable package, and the manifests carry release notes. XP startup checks, **Refresh Release List**, and selecting the current release no longer depend on the redirecting root paths.
- **Older tags no longer download the wrong package.** Older mirror tags are comparison-only; if a tag has no archived package, the updater reports that instead of downloading the current stable zip under the older tag.
- `rsrc/sofvault_mirror/sync_from_github.sh` now writes the compact `files/` manifests (latest, stable, unstable, list, per-tag) and keeps the root copies for older clients. Reinstall and run it once on the sofvault host to publish the new files; `check_http.sh` verifies every XP-critical endpoint.

## v8.58

### Crosshairs

- **Stock crosshair tints are now fully saturated.** The previous palette mixed in 20–55% of the other channels to keep the colors soft, which made red, green, and blue look pastel. Red is now `(1,0,0)`, green `(0,1,0)`, blue `(0,0,1)`, with pure yellow/cyan/magenta and saturated orange/purple.

## v8.57

### Scaled scope

- **Every `cScope::Draw` overlay element now maps into the centred 4:3 frame**, not just the tube and vignette. The right-side ammo/zoom gauge, arrows, crosshair bars, and magnification readout all keep their authored proportions, and the readout glyphs scale with the scope instead of staying at stock 8 px. `cScope::Draw` (`0x6EA0`) was missing from `funcmaps/SoF.exe.json`, so the callsite classifier could not identify its `Draw_StretchPic`/`R_DrawFont` calls; the scope hooks now also key off the active-frame flag, so an older funcmap cannot silently disable them.

## v8.56

### Loading screen

- The loading header frame is taller, so the **Map** and **Download** lines are no longer clipped at 1080p with the default menu scale.
- Rejoining a map that already completed on an earlier connection refreshes the **Download** status from the engine filesystem, so it shows **NOT NEEDED** again instead of staying at **CHECKING**.

### Menus

- **Graphics resolution changes now use Select then Apply.** The F12 → Graphics resolution list cycles through a staged value and leaves `gl_mode` alone until **Apply Resolution** is pressed, which copies the choice, runs `vid_restart`, and reopens the page. Previously every click applied immediately and rebuilt the page, so the first click could jump to 640×480 and later clicks appeared stuck or restarted the list. Leaving the page now discards an unapplied selection, and simply browsing resolutions never changes the archived `gl_mode`. While fullscreen is on, the list is filtered to modes the monitor reports, so legacy entries such as 960×720 can no longer drop the game into a window; windowed mode still shows the full list. The invalid `gl_mode` placeholders 0–2 (320×240/400×300/512×384) are no longer offered, so the picker starts at 640×480. The Graphics page shows whether the list is filtered, and the active mode stays available as `(current)` if filtering would remove it.

### Scaled menu

- **Topic tickers stay inside their band again.** The full-screen scissor was meant for the full-width, non-global main-menu tape, but it also applied to global topic tickers such as the Start Game page. Those already clip to their own box, so the override let their text scroll across the menu and past its edges. The screen scissor now applies only to non-global bolted tickers; global tickers keep the engine's ticker-box clip.
- **Tooltips now fade in at vanilla speed.** The engine scales its fade ramp with `tip_duration / 16`, so Buddy's longer 15000 ms duration stretched the fade to almost a second even though the tip only starts at `tip_duration / 4`. The tip renderer now caps the box and text alpha ramp at stock SoF's `2500 / 16 = 156 ms` for both the fade-in and the fade-out; the `tip_duration` setting keeps controlling how long the tip stays on screen, and shorter custom durations still use their own faster ramp.

## v8.55

### UI scaling

- **Restore Scaling Defaults now resets every scale slider.** The button used to write only the computed absolute scale cvars, leaving the percentage sliders (and the other mode's stored value) untouched; switching Smooth → Integer → Smooth then brought the old scales back and each section had to be reset by hand. It now resets the font, HUD, and menu percentages and their computed scales together, so both modes return to the documented 75% default. The per-section **Reset to default** buttons also clear the stored percentage in Integer mode.

### Menus

- **New Mouse cursor option (Vanilla / SoFPlus).** SoFPlus ships a cursor override inside `sofplus.pak`. The new F12 → General setting defaults to **SoFPlus when the SoFPlus DLL is loaded**, otherwise **Vanilla**, works whether or not SoFPlus is installed, and can serve the SoFPlus cursor from a bundled copy when SoFPlus is absent. Configs written by the first cursor build (which defaulted to Vanilla) migrate to SoFPlus once when SoFPlus is loaded. Changing it restarts the renderer so the cached cursor image is reloaded.

### Updater

- **Windows XP `win32 error 12175` fixed at the client fallback:** the startup check now uses `latest.json` directly when its tag matches the selected channel, and `by_tag`-selected releases reuse `latest.json` for the current tag. The full `list.json`/`by_tag` feeds are only fetched when needed, so an HTTPS redirect on those paths no longer breaks the normal stable check.
- **XP TLS errors now name the real cause:** if WinHTTP returns `12175` after an HTTP→HTTPS redirect, the message says the mirror redirected to HTTPS and XP cannot negotiate modern TLS/SNI, instead of implying a local Windows setting problem. XP zip downloads are also normalized to the plain-HTTP SFVault mirror.
- The SofVault mirror now includes `check_http.sh`; it fails on any `latest.json`, `list.json`, `by_tag`, `latest`, or `files/` endpoint that redirects to HTTPS. The mirror README documents the required nginx exception.

### Lighting

- Lighting labels are clearer: **Shiny/Glossy surfaces** now uses **Disabled**, **Engine Decides**, **As Planar**, **As Sphere Map**, and **Planar And SphereMap - double**; the dynamic-light cutoff is labelled **D-Light Cut Off** and `gl_dlightintensity` is **D-Light Intensity**. The live panel is grouped into Renderer, Lighting Style, Dynamic Lighting, Ghoul Models, Engine, Lighting Status, and Maintenance sections, with extra spacing above slider groups.
- **Overbright Pre-Curve sliders now show 0-200%.** Because Overbright multiplies by 2, the editor labels each point with the final brightness after that product instead of the stored 0-100 value. The archived `_sofbuddy_lighting_overbright_curve_*` cvars and profiles are unchanged; displayed 100% = stored 50%, and the slider steps by 2 display units so every position maps to an exact stored value.
- **The Lighting nav tab now opens the live panel directly.** The old **Live Lighting (preview over game)** button is gone, and every lighting control lives in the 380x600 panel over the game: `gl_ext_multitexture`, Overbright, Map Lighting Normalization, the Style/Pre-Curve rows, Colour Mode, Shiny/Glossy surfaces, D-Light Cut Off, Dynamic Lights, `gl_dlightintensity`, all Ghoul rows, Smooth Lightstyles, Lightmap Multiplier, the Custom manual blend, Lighting status, Rebuild Lightmaps, and the reset/restart actions. **Back to Menu** returns to the full menu and **Back to Game** closes the panel. The two reset actions are now **Reset SoF Buddy Lighting** (curve/overbright/normalization/colour/cutoff/shiny/Ghoul minimum) and **Reset Ghoul & Dynamic Lights** (Ghoul model lighting, dynamic lights, `gl_dlightintensity`, `gl_modulate`) so their different scopes are obvious.
- The 11-point **Tone Curve** and **Overbright Pre-Curve** editors open as live-preview panels pinned to the right edge of the screen. The main menu closes first, so the game world stays visible while sliders move; each panel has the 11 sliders, a reset, **Back to Lighting**, and **Back to Game**. The editors are opened from the Lighting tab's **Tone Curve...** and **OB Pre-Curve...** buttons. Back/navigation buttons use an orange border so they stand out from normal controls, and the panels scale down to fit short screens.

### Scaled console

- The bottom-right console version stamp (`v1.07f`) no longer clips when the console font is scaled. `CON_DrawConsole` positioned it with a fixed 44 px offset, but `Draw_String` advances 8 px per character, so the six-character string already filled 48 px. The offset is now 56 px (48 px text + 8 px margin), and it scales with the virtual console width, so the stamp stays fully visible at every font scale.

### Scaled menu

- **Backdrop menu pages now set their scissor correctly.** Pages that declare a `<backdrop>` — including the SoFPlus `svrlist` serverbox page — delegate their clip to `menu_stm_set_clip` instead of `stm_c::Render`. The intro-scaler hook for that function is installed at early startup, before ref_gl loads, and it cached an empty `Draw_RenderRect` import slot, so it never called `Draw_RenderRect` and the scissor test stayed disabled. The serverbox's per-column `Draw_RenderRectRightClip(<tab>)` calls then changed the scissor rect without enabling the test, so a long hostname ran straight through the Ping/Type/Map columns. The hook now resolves the live import slot on first use, so the tab edge clips the row text again.
- **Loose user menus no longer get the 4:3→wide frame correction by default.** The stock `menu <name>` command now leaves menus whose `.rmf` exists on disk under `User/menus`, `base/menus`, `User`, `base`, or the game root at their authored frame geometry. Stock pak menus keep the correction. The new **`menu_wide <name> [frame]`** wrapper forces the correction for any menu, including loose user menus. `sof_buddy/*` pages still always skip it.
- The **Menu tooltip Appear (ms)** default is now **3750 ms**, the engine's exact spawn gate at the Buddy `tip_duration` of 15000 ms (`tip_duration / 4`). The old 750 ms and 625 ms defaults migrate once (guarded by `_sb_internal_tip_appear_vanilla_migrated` and `_sb_internal_tip_appear_spawn_migrated`); custom values are kept. The slider now goes up to 5000 ms.

## v8.54

### Sound

- New **Sound Alt-Tab Fix** feature (on by default): sounds started while the game window is in the background no longer stack up and play together when you return. Defsnd keeps queuing sound starts while its DirectSound buffers are released, then issues all of them on the first active frame. The fix wraps the engine's `S_Activate` pointer and calls `S_StopAllSounds` when focus returns; it is reinstalled after `snd_restart`. This is separate from **Quiet Alt-Tab sound messages**, which only filters `Com_Printf` output.

### Menus

- **F12 is now a toggle** and no longer stacks copies of the menu. Pressing F12 again on a SoF Buddy page closes the whole stack; a stock menu is still opened over normally. The new `sofbuddy_close_menu` command pops every open page, and the Live Lighting, Tone Curve, and Pre-Curve pages use it before navigating. **Back to Game** and **Close** now always return to the game instead of popping one layer and appearing to do nothing when the stack was deep.

### Lighting

- Live Lighting now defaults to **Preserve colour** and the **Popping mid+highlight** Overbright pre-curve; Map Lighting Normalization stays **Off**. Existing configs that still match the old defaults migrate once, while custom curves and colour modes are kept. **Restore Lighting Defaults** now also uses the new values.
- New `_sofbuddy_lightstyle_interpolation` option (default `0`/off) smoothly blends animated light styles between the engine's 10 Hz frames. Enable it in F12 → Lighting → **Smooth Lightstyles**. Stock frame-stepping is unchanged when off.
- `_sofbuddy_shiny_spherical` now has five modes: `0` off (skip shiny entirely), `1` default, `2` no spherical, `3` all spherical, `4` both (experimental planar + spherical). F12 → Lighting shows the mode as a five-entry list. If you previously used `0` to turn off only the spherical effect, use `2` now. Mode `4` intentionally double-draws shiny surfaces and may look darker/busier. Spherical responds to camera position **and view angle**; the planar/non-spherical mode responds only to camera position, so pure view rotation no longer changes it.

- **Overbright Pre-Curve symmetry:** the product is `2 × T × C(L)`, so a curve point preserves vanilla only when `C(L) = L/2`; a point at identity is doubled. The default **Popping mid+highlight** profile stays at half through 60% input and lifts 70–100% (70→80%, 80→110% clipped, 90→white). **Late highlights** is the gentler option: half through 70%, then only the top range lifts. **Popping highlights** is the stronger option: half through 60%, then 70→90% and 80→120% (clipped). A Custom curve that jumps from half to identity at 60% makes the whole upper range twice as bright instead. The pre-curve LUT uses nearest-even rounding so the preserved range does not drift one 8-bit level brighter. The profile tooltips and README state the crossover and preserved range.

- F12 → Lighting now uses a single **Lighting Style** preset instead of the old overbright toggle plus raw blend lists: **Vanilla**, **2x Linear**, **4x Linear**, **Soft Glow**, **High Contrast**, **Deep Shadows**, **Film Slope**, **Cliff Edge**, **Bright Punch**, **Midnight**, and **Custom**. Raven 2x is now the independent **Overbright Lighting** On/Off setting above the preset list.
- Every curve preset is an 11-point slider template: selecting one writes its values into the public curve cvars and closes the menu so the result is visible immediately (Custom keeps it open to edit). Editing any slider while a preset is active switches to Custom. This makes the presets a starting point rather than a separate hard-coded path.
- Curve presets apply a real lightmap tone curve before texture modulation, so they work with `gl_ext_multitexture 1` and keep texture detail while bright lights clip to white. **Overbright Lighting** now works with multitexture on **or** off: with multitexture on it applies the same `2 × L × T` product in texture unit 1 (`GL_COMBINE_ARB` + `GL_RGB_SCALE_ARB`), and with multitexture off it uses the original `GL_DST_COLOR, GL_SRC_COLOR` framebuffer blend. The toggle never changes the user's `gl_ext_multitexture` value.
- F12 → Lighting now shows the current map's **static light style 0** readout, e.g. `Static style 0 : 1.00 (m)`. It reads the renderer's live style-0 value, which comes from `CS_LIGHTS+0`. Quake maps letters with `(letter - 'a') / ('m' - 'a')`, so `m` is 1.00, `l` is 0.92, and `n`..`z` are above 1.00. The value is updated a few times per second and cleared while a new map loads.
- **Overbright Pre-Curve profiles** are now separate from the Lighting Style presets: **Popping mid+highlight** (`0,5,10,15,20,25,40,55,70,85,100`, default), **Late highlights** (`0,5,10,15,20,25,30,35,45,65,100`, gentler top-end lift), **Popping highlights** (`0,5,10,15,20,25,30,45,60,80,100`, stronger pop above 60%), **Preserve Darks only** (`0,5,10,15,23,33,45,59,75,93,100`), **2x everything** (`0,10,20,30,40,50,60,70,80,90,100`, the old linear identity and stock 2 × T × L look), and **Custom**. Selecting a profile writes its 11 values into the overbright curve cvars; editing any slider switches to Custom. The reset button returns to Popping mid+highlight. Existing identity overbright curves migrate to the new default; hand-tuned curves are kept as Custom. The existing `4` Custom value is unchanged; Late uses `5`.
- **Custom** exposes the 11-point curve (0,10,...,100% input → 0–100% output) for shadows, midtones, and highlights, plus the manual blend factors. Output 100 is white; map lower inputs to 100 for overbright. The curve resets to a linear identity with **Reset Curve (Linear)**. Curve changes rebuild the current map's lightmaps on the next frame; `vid_gamma` still composes normally. Selecting Custom from Overbright Lighting now clears the leftover `DST_C`/`SRC_C` blend pair so the curve starts without the legacy product blend stacked on top; a **Curve Only (Neutral Blend)** button restores that state.
- New **Colour Mode**: **Preserve brightness** is now the default and is selected automatically by built-in curve presets. Per-channel gives classic overbright with more saturation/neon; Preserve colour curves the brightest channel and scales RGB together so nothing clips, at the cost of less headroom for colours already near white.
- **Map Lighting Normalization** (`_sofbuddy_lighting_normalize`, default off) now has four F12 modes: **Off**, **Up**, **Down**, and **Both**. **Up** moves the map's brightest sample toward white; the live hub's Up amount sets how far (100% = 255). **Down** maps the map's absolute minimum to black while keeping the original maximum, so values above it (dynamic lights) retain their headroom and still stand out; because it uses the absolute minimum, it is a no-op when that minimum is already 0. **Both** applies Down then Up; the live hub exposes both amounts. All modes use one shared normalization LUT for world lightmaps, Preserve brightness/colour, and entity/particle/Ghoul `R_LightPoint` light. The map scan now reports the true absolute minimum (the previous minimum readout could remain 0 even on maps with a raised black point), so Down and Both use the real dark point. The F12 status line shows the mode and percentage targets; the live hub has the mode and amount sliders. Fixed: in **Preserve brightness** and **Preserve colour** the normalization was applied only by the `R_BuildLightMap` post-hook, while those modes leave the LUT at the gamma table. Toggling normalization therefore changed no LUT bytes and no rebuild was queued, so existing static lightmaps kept their old values and the setting appeared to do nothing. The normalization LUT now participates in change detection, so changing modes and loading a map with normalization already enabled both rebuild the static lightmaps.
- **Map Lighting Normalization amounts** are now percentages (`_sofbuddy_lighting_normalize_up`/`_down`, default 100). Up target = max + amount × (255-max); Down target = min - amount × min. The same setting adapts to each map. 0% is identity for that end, 100% is the strong endpoint; Both applies Down then Up. The fixed 4× Up cap was removed. The live hub shows only the sliders the active mode uses.
- Fixed stale bright patches on maps with animated or non-zero light styles. The in-place lightmap rebuild now includes those surfaces and refreshes `R_SetCacheState`, so changing the Overbright Pre-Curve or Map Lighting Normalization rebuilds them too instead of leaving the old lightmap behind. Animated styles still update normally when their style value changes. Reproduced on `tsr1` near `425 1053 -108` with every Overbright Pre-Curve slider at `0`.
- Ghoul minimum light now has a **Vanilla/Bypass** setting under F12 → Lighting → Ghoul. **Vanilla** is the default and keeps the engine's `0.1` floor for `RF_MINLIGHT` models. **Bypass** removes the floor so those models follow the actual light and can go fully dark with the curve. Entities explicitly flagged fullbright remain fullbright by design.
- Ghoul imported dynamic lights now follow the curve too. The copy of each dlight passed to `Ghoul::AddImportLight` is scaled by the effective curve, while the engine's original `r_dlights` list is left untouched for the world lightmap path. This removes the bright model light that previously survived when every Overbright Pre-Curve slider was set to `0`, without double-curving world dlights.
- **Map Lighting Normalization** lives in the **Live Lighting** panel next to Overbright Lighting, so its effect can be previewed over the game. The main Lighting tab keeps only the `Normalization :` status readout.
- **Ghoul models, entities, and particles now follow the normalized lightmap.** `R_LightPoint` samples raw lightmap bytes and bypassed the world LUT, so normalized world shadows could brighten while Ghoul models kept their old light and looked wrong (for example, bright in shadow). A post-hook now applies the same gamma, normalization reference, and per-channel/preserve curve to the returned entity light whenever the LUT differs from stock. Vanilla with no curve or normalization is untouched; a focused timedemo measured no meaningful FPS change (Vanilla 11.5 fps, Normalization + 2x Linear 11.4 fps). The **Ghoul Lighting Mode** tooltip now also notes that Balanced (1) follows lightmap shadows best, GPU modes are less shadow-following, and `ghl_light_multiply` scales model light on top.
- Old `_sofbuddy_lighting_preset 4` configs migrate to **Overbright Lighting** on with Vanilla selected underneath; the old `_sofbuddy_lighting_overbright 1` value is now the same independent toggle. Manually edited blend factors still migrate to Custom.
- Custom now remembers the previous `gl_ext_multitexture` value when a non-default manual blend forces it off. Switching back to a multitexture-safe lighting style restores it and requests one renderer restart; if multitexture was already off, it is left alone. Overbright Lighting no longer forces anything and works with either setting. The Custom page keeps the **Curve Only (Neutral Blend)** and **SRC² Blend (L²+L×T)** buttons; the old manual **Raven 2x Blend** shortcut was removed because the Overbright Lighting toggle now covers that product with the pre-curve.
- New commands: `sofbuddy_lighting_rebuild`, `sofbuddy_lighting_status`, `sofbuddy_lighting_curve_template`, `sofbuddy_lighting_curve_templates`, `sofbuddy_lighting_overbright_curve_reset`, and `sofbuddy_lighting_reset`.
- `gl_ext_multitexture` is now available at the top of F12 → Lighting as well as F12 → Graphics, since lighting modes depend on it. Both rows run `vid_restart` automatically after a change, so the renderer switches between the multitexture and single-texture paths immediately.
- F12 → Lighting no longer repeats long orange explainer paragraphs under every control. The redundant preset summary and blend notes were removed or moved into their tooltips; normalization keeps a single compact `max/255, gain` readout on the main tab.
- Overbright Lighting on multitexture hardware is now the same product as the legacy blend without switching renderer paths. Verified against the legacy Raven path on `nyc1`: static world surfaces match within rounding, while brush-model entities such as the lamp post stay slightly brighter under the legacy path. The hidden test switch was removed; shifting `gl_ext_multitexture` simply switches between the combiner and the legacy blend.
- **Overbright Lighting** is independent of Lighting Style and has a separate **Overbright Pre-Curve** with its own profile list, Colour Mode, and dedicated curve editor. The pre-curve is applied before the ×2 (`2 × T × C(L)`) and defaults to **Popping mid+highlight**; **2x everything** is the old linear identity. It works with `gl_ext_multitexture` on or off and leaves the stored Lighting Style curve untouched. In the live hub, turning Overbright on shows the Pre-Curve row and turning it off returns to the stored Lighting Style.
- The **Tone Curve** editor is available under built-in curve presets, not only under **Custom**. Selecting a preset seeds the editor with its 11 values; editing any slider keeps that value and switches Lighting Style to Custom, so a preset can be used directly as a starting point. The manual lightmap blend controls still appear only for Custom.
- The 11-point **Tone Curve** and **Overbright Pre-Curve** sliders now live in dedicated live-preview panels pinned to the right edge of the screen. The main menu closes first, so the game world stays visible while sliders move; each panel has the 11 sliders, a reset, a **Back to Live Lighting** button, and a **Back to Game** button. A new **Live Lighting (preview over game)** button opens a hub for Overbright Lighting, Map Lighting Normalization, Colour Mode, Specular Textures, Dynamic Light Threshold, Dynamic Lighting, `gl_dlightintensity`, and the Ghoul rows; it shows the Lighting Style row and **Tone Curve...** button only while Overbright is off, and the Overbright Pre-Curve profile and **OB Pre-Curve...** button only while it is on. The hub rebuilds itself when Overbright is toggled so the correct rows appear, and the Tone Curve and Pre-Curve editors are reachable from it. Back/navigation buttons use the `Back to ...` naming and an orange border so they stand out from normal controls. The panels scale down to fit short screens. `gl_ext_multitexture` and **Lightmap Multiplier** stay on the main tab because they need a renderer restart or confirmation.
- F12 → Lighting is now the non-preview page. Overbright Lighting, Map Lighting Normalization, the style and pre-curve rows, Colour Mode, Specular Textures, Dynamic Light Threshold, Dynamic Lighting, `gl_dlightintensity`, and all Ghoul lighting rows were removed from the tab and exist only in **Live Lighting**. The tab keeps `gl_ext_multitexture`, the Lighting status readouts, Lightmap Multiplier, the Custom manual blend, and the restore/rebuild actions. This removes the duplicated Dynamic Light Threshold and the other preview rows.
- Lighting performance is now labelled and the off-state paths are trimmed. **Overbright Lighting** notes its small per-surface cost while On and that Off does no combiner/blend work. Tone curve presets, the Overbright Pre-Curve, and Map Lighting Normalization are marked as having no per-frame cost (they rebuild lightmaps once when changed). Code-wise, the Overbright wrappers use a cached enabled bool, and the gamma-table/mirror checks run every 16 frames instead of every frame.
- F12 → Lighting's **Lightmap Multiplier** slider now asks before changing `gl_modulate` away from `1`: “Are you sure you want to do this? Its highly recommended to keep this at 1 to prevent flat lighting.” The slider uses an internal mirror, so the real cvar changes only after **Apply Anyway (Y)**; **Cancel (N)**, Escape, F12, or switching pages restores the old value. Returning the slider to `1` applies immediately without a popup, and slider drags are debounced briefly so the popup does not interrupt the drag.

### Scaled HUD

- The countdown timer text now follows `_sofbuddy_hud_scale` instead of staying at stock size during timed sections.

### Scaled scope

- New `scaled_scope` feature. The sniper scope overlay now keeps its authored 4:3 proportions on widescreen displays instead of stretching with the aspect ratio. The scope is centred with black side bars (top/bottom bars on displays narrower than 4:3), and the magnification readout scales with the scope so it stays legible at 1080p and above.

### Scaled menu

- `<stm resize>` pages (player/precache dialogs, SoFPlus SPM Settings) now scale their contents with the menu again. The resize canvas scales *frames* only — the engine multiplies frame `W/H` by the page canvas in `frame_c`'s constructor, but widget dimensions are never canvas-scaled. The previous whole-page skip left sliders, pictures, bars, and text at stock size inside a scaled frame; non-frame widgets now get the normal per-widget `_sofbuddy_menu_scale` factor, with click/drag hit-testing following the scaled art.
- Long F12 tooltips now wrap onto multiple lines instead of running off the left edge of the screen. Stock draws one line and only clamps its right edge, so a tip wider than the display gets a negative x. Wrapping breaks greedily at word boundaries (hard-splitting an over-long word), keeps a 12 px screen margin, moves the block up if it would cross the bottom, and keeps the bevel background hugging the wrapped text.
- The **Menu tooltip Appear (ms)** default is now **625 ms**, matching stock SoF (`tip_duration` 2500 / 4) instead of 750 ms. The old shipped 750 ms default is migrated once (guarded by `_sb_internal_tip_appear_vanilla_migrated`); custom values, including a later deliberate 750, are kept.

### New system detection

- New-hardware detection no longer leaves the mixer at 11 kHz. `drivers/highest.cfg` is now executed immediately (`EXEC_NOW`) when the stock cascade is overridden, instead of being appended to the command buffer: appended commands only ran after `CL_Init`, so `drivers/memory1.cfg`'s `s_khz 11` had already initialized `S_Init` at 11025 Hz. `highest.cfg` restores `s_khz 22` before the sound driver starts, and the performance overrides (`fx_maxdebrisonscreen`, `r_isf`, `r_iaf`) are applied after it.

### Installer

- The Windows installer no longer pre-selects **Install SoF Plus client**. Setup only ticks it when the chosen game folder already contains a SoF Plus client (`spcl.dll` or `SofPlus.dll`), so vanilla installs opt in manually while existing SoF Plus players keep the client step selected.

### Graphics

- Model and ghoul-skinned textures now receive the same anisotropic filtering as world geometry. SoF.exe's own model renderer sets `GL_TEXTURE_MIN_FILTER`/`MAG_FILTER` through its private GL dispatch table but never `GL_TEXTURE_MAX_ANISOTROPY_EXT`, so models stayed at bilinear/trilinear and blurred at grazing angles even with 16x enabled. The new `model_anisotropic` feature wraps that table's filter call and applies the same level used by the world path (`gl_ext_anisotropic`, SoFPlus's 1–16x setting, clamped to the GL maximum).

### Gameplay

- New **Force movespeed** option (F12 → Gameplay → Singleplayer, off by default). Singleplayer runs by default instead of walking — the same effective setting as SoF Plus's `cl_run 1` first-run default — while the engine's walk key still slows you down. It applies immediately, never changes your saved `cl_run`, and has no effect in multiplayer.
- Locally hosted multiplayer games no longer inherit a stale **Movescale: Half** (0.65) value from the host's config. The engine archives `g_movescale` (default Full = 1.0) and a listen server reused it as the server value, so the host moved at 65% speed — almost exactly the singleplayer 56.25% movescale. The new `local_mp_full_movescale` feature (F12 → Gameplay → Multiplayer, **Full** by default) forces full movescale for the local server session and restores the saved value on shutdown. Dedicated servers and remote clients are unaffected.

## v8.53

### Media timers

- Frame pacing now waits on a high-resolution waitable timer (`CreateWaitableTimerExW`, Windows 10 1803+, resolved dynamically so XP builds/wine still fall back), rounding up to 100 ns units. The frame loop uses short 100 µs waits instead of a coarse `Sleep(1)`, so `cl_maxfps` exact mode hits its 1 ms frame window more consistently. SoFPlus's own frame delay is held at 0 in the same frame-loop paths instead of being cleared from every `Sys_Mil` call.

## v8.52

### Scaled menu

- The 16:9 chrome now forces the `16_9_l` / `16_9_r` side pages to a clear backdrop on load, so a pak or SoFPlus version with different side art cannot leave stale pixels beside the centred menu. Global menu backgrounds (`<stm global>`, e.g. `backdrop/web_bg`) are now anchored at the menu origin, so they span the whole screen instead of starting at the centred page's left edge and leaving the left side strip unpainted.
- Integer-mode scale settings survive a restart again. The percentage callbacks fire while the scale cvars are registered at startup, before the video mode is known, and were recomputing the absolute scale as 1x; in Integer mode they are now ignored (and they never write while the maximum is unknown). Startup/vid_restart transients (640x480, 800x600, …) no longer clamp the stored value either: it is only clamped when the mode is known to be at least 960p, to that mode's integer maximum. Low modes still cap the *effective* scale without wiping the saved value. Slider teardown (`slider_c::Cleanup`) clamps are ignored for scale cvars, and a queued slider write is dropped before a reset/mode change.
- Tooltip backgrounds now hug their text at every menu scale and resolution. The bevel box was sized from the font line height, while scaled glyphs grow from the first glyph's top, so the bottom gap grew with `menuScale` and taller glyphs shifted the run. A `Draw_BeveledBox` IAT hook now sizes and positions the background from the measured screen-space glyph bounds.
- Load Game and Save Game now stay populated at 1× on 4K. The uncorrected loadbox width overflowed the 4:3 centre frame and the whole box was rejected as too wide; the box is now kept onscreen, so the save list renders at 1× as well as at higher menu scales.
- The outfit menu's weapon/item selection grids now scale with `_sofbuddy_menu_scale`, including icon pitch and click hit-testing, so the same number of slots stays onscreen. `noscale` is treated as the stock native-size drawing flag rather than a SoF Buddy opt-out, so the outfit menu's bolted knife/divider art scales too. Bolted menu art (such as SoFPlus Web Folders tab icons) uses the vertical-vs-canvas art ratio, so icons no longer overhang the tab art on wide screens.

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
