#include "feature_config.h"

#if FEATURE_SCALED_CON || FEATURE_SCALED_HUD || FEATURE_SCALED_MENU || FEATURE_SCALED_UI_BASE

#include <windows.h>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <vector>
#include "sof_compat.h"
#include "util.h"
#include "shared.h"

namespace {

using GLenum = unsigned int;
using GLuint = unsigned int;
using GLint = int;
using GLsizei = int;
using GLubyte = unsigned char;

constexpr GLenum kGLTexture2D = 0x0DE1;
constexpr GLenum kGLTextureBinding2D = 0x8069;
constexpr GLenum kGLTextureWidth = 0x1000;
constexpr GLenum kGLTextureHeight = 0x1001;
constexpr GLenum kGLTextureInternalFormat = 0x1003;
constexpr GLenum kGLTextureMinFilter = 0x2801;
constexpr GLenum kGLTextureMagFilter = 0x2800;
constexpr GLenum kGLTextureWrapS = 0x2802;
constexpr GLenum kGLTextureWrapT = 0x2803;
constexpr GLenum kGLMaxTextureSize = 0x0D33;
constexpr GLenum kGLRgba = 0x1908;
constexpr GLenum kGLUnsignedByte = 0x1401;
constexpr GLenum kGLLinear = 0x2601;
constexpr GLenum kGLClamp = 0x2900;

using tGlGenTextures = void(__stdcall*)(GLsizei, GLuint*);
using tGlDeleteTextures = void(__stdcall*)(GLsizei, const GLuint*);
using tGlBindTexture = void(__stdcall*)(GLenum, GLuint);
using tGlTexImage2D = void(__stdcall*)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
using tGlTexParameteri = void(__stdcall*)(GLenum, GLenum, GLint);
using tGlGetIntegerv = void(__stdcall*)(GLenum, GLint*);
using tGlGetTexLevelParameteriv = void(__stdcall*)(GLenum, GLint, GLenum, GLint*);
using tGlGetTexImage = void(__stdcall*)(GLenum, GLint, GLenum, GLenum, void*);
using tGlGetError = GLenum(__stdcall*)(void);
using tFindImage = void*(__cdecl*)(const char*, int, int, int);

tGlGenTextures pGlGenTextures = nullptr;
tGlDeleteTextures pGlDeleteTextures = nullptr;
tGlBindTexture pGlBindTexture = nullptr;
tGlTexImage2D pGlTexImage2D = nullptr;
tGlTexParameteri pGlTexParameteri = nullptr;
tGlGetIntegerv pGlGetIntegerv = nullptr;
tGlGetTexLevelParameteriv pGlGetTexLevelParameteriv = nullptr;
tGlGetTexImage pGlGetTexImage = nullptr;
tGlGetError pGlGetError = nullptr;
tFindImage pFindImage = nullptr;

std::unordered_map<uint64_t, GLuint> g_cache;
GLint g_maxTextureSize = 0;

void ensure_gl() {
	if (pGlGenTextures) return;
	HMODULE gl = GetModuleHandleA("opengl32.dll");
	if (!gl) gl = LoadLibraryA("opengl32.dll");
	if (!gl) return;
	pGlGenTextures = reinterpret_cast<tGlGenTextures>(GetProcAddress(gl, "glGenTextures"));
	pGlDeleteTextures = reinterpret_cast<tGlDeleteTextures>(GetProcAddress(gl, "glDeleteTextures"));
	pGlBindTexture = reinterpret_cast<tGlBindTexture>(GetProcAddress(gl, "glBindTexture"));
	pGlTexImage2D = reinterpret_cast<tGlTexImage2D>(GetProcAddress(gl, "glTexImage2D"));
	pGlTexParameteri = reinterpret_cast<tGlTexParameteri>(GetProcAddress(gl, "glTexParameteri"));
	pGlGetIntegerv = reinterpret_cast<tGlGetIntegerv>(GetProcAddress(gl, "glGetIntegerv"));
	pGlGetTexLevelParameteriv = reinterpret_cast<tGlGetTexLevelParameteriv>(GetProcAddress(gl, "glGetTexLevelParameteriv"));
	pGlGetTexImage = reinterpret_cast<tGlGetTexImage>(GetProcAddress(gl, "glGetTexImage"));
	pGlGetError = reinterpret_cast<tGlGetError>(GetProcAddress(gl, "glGetError"));
	if (!pFindImage)
		pFindImage = reinterpret_cast<tFindImage>(rvaToAbsRef(reinterpret_cast<void*>(0x7380)));
	if (pGlGetIntegerv) {
		GLint value = 0;
		pGlGetIntegerv(kGLMaxTextureSize, &value);
		g_maxTextureSize = value;
	}
}

bool is_integer_scale(float scale) {
	const float nearest = std::floor(scale + 0.5f);
	return std::fabs(scale - nearest) < 0.001f;
}

} // namespace

void* scaled_ui_find_image(const char* name) {
	ensure_gl();
	if (!pFindImage || !name) return nullptr;
	return pFindImage(name, 3, 0, 1);
}

unsigned int scaled_ui_image_proxy_texnum(void* image, float scale) {
	if (!image) return 0;
	if (!_sofbuddy_scale_mode || _sofbuddy_scale_mode->value < 0.5f) return 0;
	if (scale <= 1.0f || is_integer_scale(scale)) return 0;
	ensure_gl();
	if (!pGlGenTextures || !pGlBindTexture || !pGlTexImage2D || !pGlTexParameteri ||
	    !pGlGetIntegerv || !pGlGetTexLevelParameteriv || !pGlGetTexImage)
		return 0;

	const GLint prescale = static_cast<GLint>(std::ceil(scale));
	if (prescale < 2 || prescale > 8) return 0;

	const int src_texnum = *reinterpret_cast<int*>(static_cast<char*>(image) + 80);
	if (src_texnum <= 0) return 0;
	const uint64_t key = (static_cast<uint64_t>(static_cast<unsigned int>(src_texnum)) << 8) |
	                     static_cast<unsigned int>(prescale);
	auto it = g_cache.find(key);
	if (it != g_cache.end()) return it->second;

	GLint prev = 0;
	pGlGetIntegerv(kGLTextureBinding2D, &prev);
	pGlBindTexture(kGLTexture2D, static_cast<GLuint>(src_texnum));
	GLint w = 0, h = 0;
	pGlGetTexLevelParameteriv(kGLTexture2D, 0, kGLTextureWidth, &w);
	pGlGetTexLevelParameteriv(kGLTexture2D, 0, kGLTextureHeight, &h);
	if (w <= 0 || h <= 0 || w > 512 || h > 512) {
		pGlBindTexture(kGLTexture2D, static_cast<GLuint>(prev));
		return 0;
	}
	GLint fmt = 0;
	pGlGetTexLevelParameteriv(kGLTexture2D, 0, kGLTextureInternalFormat, &fmt);
	if (fmt != 0x1908 && fmt != 0x1907 && fmt != 0x8058 && fmt != 0x8051) {
		pGlBindTexture(kGLTexture2D, static_cast<GLuint>(prev));
		return 0;
	}
	const GLint dst_w = w * prescale;
	const GLint dst_h = h * prescale;
	if (g_maxTextureSize > 0 && (dst_w > g_maxTextureSize || dst_h > g_maxTextureSize)) {
		pGlBindTexture(kGLTexture2D, static_cast<GLuint>(prev));
		return 0;
	}
	if (static_cast<int64_t>(dst_w) * dst_h > 4 * 1024 * 1024) {
		pGlBindTexture(kGLTexture2D, static_cast<GLuint>(prev));
		return 0;
	}

	std::vector<unsigned char> src(static_cast<size_t>(w) * h * 4);
	std::vector<unsigned char> dst(static_cast<size_t>(dst_w) * dst_h * 4);
	pGlGetTexImage(kGLTexture2D, 0, kGLRgba, kGLUnsignedByte, src.data());
	if (pGlGetError && pGlGetError() != 0) {
		pGlBindTexture(kGLTexture2D, static_cast<GLuint>(prev));
		return 0;
	}
	for (GLint y = 0; y < dst_h; ++y) {
		const unsigned char* s = src.data() + static_cast<size_t>(y / prescale) * w * 4;
		unsigned char* d = dst.data() + static_cast<size_t>(y) * dst_w * 4;
		for (GLint x = 0; x < dst_w; ++x) {
			std::memcpy(d + static_cast<size_t>(x) * 4, s + static_cast<size_t>(x / prescale) * 4, 4);
		}
	}

	GLuint tex = 0;
	pGlGenTextures(1, &tex);
	if (!tex) {
		pGlBindTexture(kGLTexture2D, static_cast<GLuint>(prev));
		return 0;
	}
	pGlBindTexture(kGLTexture2D, tex);
	pGlTexParameteri(kGLTexture2D, kGLTextureMinFilter, kGLLinear);
	pGlTexParameteri(kGLTexture2D, kGLTextureMagFilter, kGLLinear);
	pGlTexParameteri(kGLTexture2D, kGLTextureWrapS, kGLClamp);
	pGlTexParameteri(kGLTexture2D, kGLTextureWrapT, kGLClamp);
	pGlTexImage2D(kGLTexture2D, 0, kGLRgba, dst_w, dst_h, 0, kGLRgba, kGLUnsignedByte, dst.data());
	pGlBindTexture(kGLTexture2D, static_cast<GLuint>(prev));

	if (g_cache.size() >= 128) {
		if (pGlDeleteTextures)
			for (const auto& entry : g_cache) pGlDeleteTextures(1, &entry.second);
		g_cache.clear();
	}
	g_cache.emplace(key, tex);
	return tex;
}

void scaled_ui_image_cache_invalidate() {
	g_cache.clear();
}

#endif
