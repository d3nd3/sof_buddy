# Model Anisotropic Filtering

Extends anisotropic filtering to model and ghoul-skinned geometry. SoF's world
rendering applies `GL_TEXTURE_MAX_ANISOTROPY_EXT`; the engine's own model renderer
did not, so model textures stayed at bilinear/trilinear and blurred at grazing
angles even with 16x anisotropic filtering enabled.

## Why

Two separate texture paths exist in SoF:

- **World / ref_gl textures** — `GL_SetFilter` (`ref_gl.dll 0x30006570`) sets
  `GL_TEXTURE_MIN_FILTER` / `MAG_FILTER` and, for the default image types
  (`it_skin`, `it_sprite`, `it_wall`, `it_detail`), also sets
  `GL_TEXTURE_MAX_ANISOTROPY_EXT` (`0x84FE`). The engine hard-codes `2.0`; SoFPlus
  (`spcl.dll`) patches that immediate with its 1–16x
  `_sp_cl_vid_gl_texture_max_anisotropy_ext` setting.
- **Model / ghoul textures** — SoF.exe renders these itself. The engine stores its
  GL dispatch table at `0x20403C04` (via the virtual method at `0x20070780`) and
  the model texture setup (`0x200BF0xx`, `0x200947xx`) calls the table's
  `glTexParameteri` entry (offset `0x80`) with `GL_LINEAR` /
  `GL_LINEAR_MIPMAP_NEAREST` filters. `0x84FE` never appears in SoF.exe, so this
  path never enables anisotropy.

## Fix

- New `model_anisotropic` feature (F12 → Features → Graphics, enabled by default).
- `ghoul_set_gl_table` (`SoF.exe 0x00070780`, the method that stores the engine GL
  table) is detoured. Immediately after the original stores the table, the entry
  for `glTexParameteri` is wrapped.
- The wrapper calls the original filter setup, and when a mipmapped minification
  filter is set (`GL_NEAREST_MIPMAP_NEAREST` … `GL_LINEAR_MIPMAP_LINEAR`) it also
  applies `GL_TEXTURE_MAX_ANISOTROPY_EXT` to the currently bound texture.
- The level follows the same settings as the world path:
  - `gl_ext_anisotropic 0` disables it;
  - SoFPlus `_sp_cl_vid_gl_texture_max_anisotropy_ext` (1–16x) when present;
  - otherwise the engine's `2.0`;
  - clamped to `GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT`.

No new cvar or menu option: the feature reuses the existing GL filtering settings.
The runtime feature toggle can restore stock behaviour.
