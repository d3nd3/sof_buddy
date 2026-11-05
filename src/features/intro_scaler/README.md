# Intro Scaler

Startup **intro.ghb** cinematic and **violence disclaimer** (`m_disclaimer.rmf`) scaling.

- **640×480** virtual page width maps with **`vid_w / 640`** (same as stock SoF menus).
- **Disclaimer**: letterbox via `menu_c+0xA0` (`FreeY`); full-screen backdrop clip; red violence text at **`vid_w/640`** only (auto from resolution — not `menuScale` / scaled-menu cvars). `R_Strlen`/`R_Strheight` + glyph vertices use that scale; scaled-menu rect/border hooks skip the disclaimer screen.
- **Intro ghoul**: model scale + vertical shift on `ghoul_c::Draw`.

Enabled by default (`intro_scaler` in `FEATURES.txt`). Runtime: `RUNTIME_FEATURE_INTRO_SCALER` / `_sofbuddy_feature_intro_scaler`. No menu-scale cvars.

When **Scaled Menu** is also compiled, backdrop clip widening registers via `intro_scaler_set_clip_extend(scaled_menu_backdrop_clip_extend)`.

Requires `UI_MENU`.
