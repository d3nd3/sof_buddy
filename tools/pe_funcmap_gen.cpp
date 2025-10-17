#include <stdio.h>
#include <string.h>
#include <vector>
#include <string>
#include <algorithm>

#include "../src/pe_utils.h"

static bool buildMapForImage(const std::string &path, ImageFunctionMap &out) {
	std::vector<uint8_t> buf;
	if (!readFile(path, buf)) { fprintf(stderr, "Failed to read %s\n", path.c_str()); return false; }
	NtHeaders32 nt{}; const SectionHeader *firstSec = nullptr;
	if (!parseHeaders(buf, nt, firstSec)) { fprintf(stderr, "Invalid PE: %s\n", path.c_str()); return false; }
	const SectionHeader *textSec = nullptr; TextSectionInfo text{};
	if (!findTextSection(buf, nt, firstSec, text, textSec)) { fprintf(stderr, ".text not found: %s\n", path.c_str()); return false; }

    std::vector<FunctionStart> funcs;
    (void)enumerateExports(buf, nt, firstSec, funcs);
    std::vector<uint32_t> callTargets;
    scanCallRel32Targets(buf, nt, firstSec, textSec, callTargets);
    for (auto rva : callTargets) funcs.push_back(FunctionStart{ rva, std::string() });

    // Heuristic: add likely prologue starts
    std::vector<uint32_t> prologues;
    scanPrologueStarts(buf, nt, textSec, prologues);
    for (auto rva : prologues) funcs.push_back(FunctionStart{ rva, std::string() });

	// Add entrypoint for EXE
    if (path.size() >= 4 && (path.substr(path.size()-4) == ".exe" || path.substr(path.size()-4) == ".EXE")) {
        funcs.push_back(FunctionStart{ nt.optionalHeader.AddressOfEntryPoint, std::string("EntryPoint") });
	}

	// Dedup and sort
	std::sort(funcs.begin(), funcs.end(), [](const FunctionStart &a, const FunctionStart &b){ return a.rva < b.rva; });
	funcs.erase(std::unique(funcs.begin(), funcs.end(), [](const FunctionStart &a, const FunctionStart &b){ return a.rva == b.rva; }), funcs.end());

	out.imageName = path;
    out.imageSize = nt.optionalHeader.SizeOfImage;
	out.text = text;
	out.functions = std::move(funcs);
	return true;
}

int main(int argc, char **argv) {
	if (argc < 3) {
		fprintf(stderr, "Usage: %s <out_dir> <image1> [image2 ...]\n", argv[0]);
		return 1;
	}
	std::string outDir = argv[1];
	for (int i = 2; i < argc; ++i) {
		std::string img = argv[i];
		ImageFunctionMap map;
		if (!buildMapForImage(img, map)) continue;
		// Derive leaf name with .json
		const char *slash = strrchr(img.c_str(), '/');
		const char *leaf = slash ? slash + 1 : img.c_str();
		char outPath[1024];
		snprintf(outPath, sizeof(outPath), "%s/%s.json", outDir.c_str(), leaf);
		if (!writeFunctionMapJson(outPath, map)) {
			fprintf(stderr, "Failed to write %s\n", outPath);
		}
	}
	return 0;
}


