#include "game_module.h"
#include "profile_mp_103.h"
#include <windows.h>
#include <psapi.h>
#include <string.h>

uint8_t* sof2_game_base(void) {
	HMODULE exe = GetModuleHandleA("SoF2MP.exe");
	if (!exe) exe = GetModuleHandleA(nullptr);
	if (!exe) return nullptr;
	char name[MAX_PATH] = {};
	if (!GetModuleFileNameA(exe, name, MAX_PATH)) return nullptr;
	const char* leaf = strrchr(name, '\\');
	leaf = leaf ? leaf + 1 : name;
	if (lstrcmpiA(leaf, "SoF2MP.exe") != 0) return nullptr;
	return (uint8_t*)exe;
}

bool sof2_is_sof2mp_exe(void) {
	HMODULE exe = GetModuleHandleA("SoF2MP.exe");
	if (!exe) exe = GetModuleHandleA(nullptr);
	if (!exe) return false;
	char name[MAX_PATH] = {};
	if (!GetModuleFileNameA(exe, name, MAX_PATH)) return false;
	const char* leaf = strrchr(name, '\\');
	leaf = leaf ? leaf + 1 : name;
	return lstrcmpiA(leaf, "SoF2MP.exe") == 0;
}

bool sof2_mp103_matches(uint8_t* base) {
	if (!base) return false;
	MODULEINFO mi = {};
	if (!GetModuleInformation(GetCurrentProcess(), (HMODULE)base, &mi, sizeof(mi))) return false;
	if (mi.SizeOfImage != Sof2Mp103Profile::kImageSize) return false;
	const int n = *(const int*)(base + Sof2Mp103Profile::kVidModeCountRva);
	if (n < 8 || n > 96) return false;
	const char* lab = *(const char**)(base + Sof2Mp103Profile::kVidModeTableRva);
	return lab && strstr(lab, "320x240") != nullptr;
}
