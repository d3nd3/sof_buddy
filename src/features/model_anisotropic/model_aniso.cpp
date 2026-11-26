/*
	Model Anisotropic Filtering

	SoF's world textures go through ref_gl's GL_SetFilter, which applies
	GL_TEXTURE_MAX_ANISOTROPY_EXT (0x84FE). Models and ghoul skins are drawn by
	SoF.exe's own renderer, which calls GL through the dispatch table stored by
	the engine method at 0x20070780 (table pointer at 0x20403C04). That path
	sets GL_TEXTURE_MIN_FILTER/MAG_FILTER but never the anisotropy parameter, so
	model textures stay at bilinear/trilinear and blur at grazing angles.

	This feature wraps the table's glTexParameteri entry: whenever a mipmapped
	minification filter is set, it also applies the same anisotropy level used by
	ref_gl's world path (SoFPlus's 1..16x setting when present, otherwise the
	engine's 2x).
*/

#include "feature_config.h"

#if FEATURE_MODEL_ANISOTROPIC

#include <windows.h>

#include "util.h"
#include "generated_detours.h"
#include "runtime_features.h"
#include "shared.h"

namespace {

using glTexParameteri_t = void(__stdcall*)(unsigned int target, unsigned int pname, int param);
using glTexParameterf_t = void(__stdcall*)(unsigned int target, unsigned int pname, float param);
using glGetFloatv_t = void(__stdcall*)(unsigned int pname, float* params);

constexpr unsigned int kGLTexture2D = 0x0DE1;
constexpr unsigned int kGLTextureMinFilter = 0x2801;
constexpr unsigned int kGLTextureMaxAnisotropy = 0x84FE;
constexpr unsigned int kGLMaxTextureMaxAnisotropy = 0x84FF;
constexpr unsigned int kGLNearestMipmapNearest = 0x2700;
constexpr unsigned int kGLLinearMipmapLinear = 0x2703;

glTexParameteri_t o_glTexParameteri = nullptr;
glTexParameterf_t p_glTexParameterf = nullptr;
glGetFloatv_t p_glGetFloatv = nullptr;
bool s_gl_resolved = false;
float s_max_aniso = 0.0f;

void resolve_gl_entrypoints(void) {
	if (s_gl_resolved) return;
	s_gl_resolved = true;
	HMODULE gl = GetModuleHandleA("opengl32.dll");
	if (!gl) return;
	p_glTexParameterf = reinterpret_cast<glTexParameterf_t>(
		GetProcAddress(gl, "glTexParameterf"));
	p_glGetFloatv = reinterpret_cast<glGetFloatv_t>(
		GetProcAddress(gl, "glGetFloatv"));
}

float configured_aniso_level(void) {
	cvar_t* enabled = findCvar(const_cast<char*>("gl_ext_anisotropic"));
	if (enabled && enabled->value == 0.0f) return 1.0f;

	// ref_gl's world path uses 2.0; SoFPlus patches that immediate with the
	// user's 1..16x value, exposed through its own cvar.
	float level = 2.0f;
	cvar_t* sofplus = findCvar(
		const_cast<char*>("_sp_cl_vid_gl_texture_max_anisotropy_ext"));
	if (sofplus && sofplus->value >= 1.0f) level = sofplus->value;
	if (level > 16.0f) level = 16.0f;

	if (s_max_aniso <= 0.0f) {
		s_max_aniso = 1.0f;
		if (p_glGetFloatv)
			p_glGetFloatv(kGLMaxTextureMaxAnisotropy, &s_max_aniso);
		if (!(s_max_aniso > 1.0f)) s_max_aniso = 1.0f;
	}
	if (level > s_max_aniso) level = s_max_aniso;
	return level;
}

void __stdcall model_aniso_glTexParameteri(unsigned int target, unsigned int pname,
                                           int param) {
	if (o_glTexParameteri) o_glTexParameteri(target, pname, param);
	if (!p_glTexParameterf) return;
	if (pname != kGLTextureMinFilter) return;
	const unsigned int filter = static_cast<unsigned int>(param);
	if (filter < kGLNearestMipmapNearest || filter > kGLLinearMipmapLinear) return;
	const float level = configured_aniso_level();
	if (level > 1.0f)
		p_glTexParameterf(target, kGLTextureMaxAnisotropy, level);
}

} // namespace

void model_aniso_install(void* gl_table) {
	if (!gl_table) return;
	resolve_gl_entrypoints();
	void** entry = reinterpret_cast<void**>(
		static_cast<char*>(gl_table) + 0x80);  // glTexParameteri
	if (!*entry || *entry == reinterpret_cast<void*>(&model_aniso_glTexParameteri))
		return;
	o_glTexParameteri = reinterpret_cast<glTexParameteri_t>(*entry);
	writeUnsignedIntegerAt(entry,
	                       reinterpret_cast<unsigned int>(&model_aniso_glTexParameteri));
}

void hk_ghoul_set_gl_table(void* self, void* table,
                           detour_ghoul_set_gl_table::tghoul_set_gl_table original) {
	if (original) original(self, table);
	model_aniso_install(table);
}

#endif // FEATURE_MODEL_ANISOTROPIC
