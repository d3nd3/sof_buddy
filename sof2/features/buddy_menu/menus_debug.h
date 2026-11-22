#pragma once

#include <windows.h>

void mnu_diag_file(const char* msg);
void sof2_menus_apply_mp_patches(HMODULE menus);
void sof2_menus_repin_native_calls(HMODULE menus);
void sof2_menus_install_debug_hooks(HMODULE menus);
void sof2_menus_debug_dump_imports(void);
void sof2_menus_trace_begin(void);
void sof2_menus_trace_end(void);
void sof2_menus_trace_import(int slot, const char* detail);
void sof2_menus_trace_step(const char* step);
void sof2_menus_trace_fmt(const char* fmt, ...);
void sof2_menus_reset_define_parse_state(void);
void sof2_menus_mark_past_defines_shaders(void);
int sof2_menus_log_lex_after_shaders(int c);
int sof2_menus_ba70_active(void);
void sof2_menus_lex_progress(int lex_n, int c);
void sof2_menus_shader_progress(int shader_n);
