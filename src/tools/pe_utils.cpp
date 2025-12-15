#include "pe_utils.h"

#include <stdio.h>
#include <string.h>
#include <algorithm>

bool readFile(const std::string &path, std::vector<uint8_t> &out) {
	FILE *f = fopen(path.c_str(), "rb");
	if (!f) return false;
	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (size <= 0) { fclose(f); return false; }
	out.resize(static_cast<size_t>(size));
	if (fread(out.data(), 1, out.size(), f) != out.size()) { fclose(f); return false; }
	fclose(f);
	return true;
}

static const SectionHeader* nthSection(const SectionHeader* first, int idx) {
	return reinterpret_cast<const SectionHeader*>(reinterpret_cast<const uint8_t*>(first) + sizeof(SectionHeader) * idx);
}

bool parseHeaders(const std::vector<uint8_t> &buf, NtHeaders32 &nt, const SectionHeader* &firstSection) {
	if (buf.size() < sizeof(DosHeader)) return false;
	const DosHeader *dos = reinterpret_cast<const DosHeader*>(buf.data());
	if (dos->e_magic != 0x5A4D) return false; // 'MZ'
	if (buf.size() < dos->e_lfanew + sizeof(NtHeaders32)) return false;
	memcpy(&nt, buf.data() + dos->e_lfanew, sizeof(NtHeaders32));
	if (nt.Signature != 0x4550) return false; // 'PE\0\0'
    firstSection = reinterpret_cast<const SectionHeader*>(buf.data() + dos->e_lfanew + sizeof(uint32_t) + sizeof(FileHeader) + nt.fileHeader.SizeOfOptionalHeader);
	return true;
}

bool findTextSection(const std::vector<uint8_t> &buf, const NtHeaders32 &nt, const SectionHeader* first, TextSectionInfo &out, const SectionHeader* &textSectionOut) {
    for (int i = 0; i < nt.fileHeader.NumberOfSections; ++i) {
		const SectionHeader *sec = nthSection(first, i);
		if (strncmp(sec->Name, ".text", 5) == 0) {
			textSectionOut = sec;
			out.rva = sec->VirtualAddress;
			out.size = sec->Misc.VirtualSize ? sec->Misc.VirtualSize : sec->SizeOfRawData;
			return true;
		}
	}
	return false;
}

static bool rvaToFileOffset(const NtHeaders32 &nt, const SectionHeader* first, uint32_t rva, uint32_t &fileOff) {
    for (int i = 0; i < nt.fileHeader.NumberOfSections; ++i) {
		const SectionHeader *sec = nthSection(first, i);
		uint32_t start = sec->VirtualAddress;
		uint32_t size = sec->Misc.VirtualSize ? sec->Misc.VirtualSize : sec->SizeOfRawData;
		if (rva >= start && rva < start + size) {
			fileOff = sec->PointerToRawData + (rva - start);
			return true;
		}
	}
	return false;
}

bool enumerateExports(const std::vector<uint8_t> &buf, const NtHeaders32 &nt, const SectionHeader* first, std::vector<FunctionStart> &out) {
    const DataDirectory &expDir = nt.optionalHeader.dataDirectory[0]; // IMAGE_DIRECTORY_ENTRY_EXPORT
	if (expDir.VirtualAddress == 0 || expDir.Size == 0) return true; // No exports (OK)
	uint32_t off = 0; if (!rvaToFileOffset(nt, first, expDir.VirtualAddress, off)) return false;
	if (buf.size() < off + sizeof(ExportDirectory)) return false;
	const ExportDirectory *exp = reinterpret_cast<const ExportDirectory*>(buf.data() + off);

	// Arrays
	uint32_t aof_rva = exp->AddressOfFunctions;
	uint32_t aon_rva = exp->AddressOfNames;
	uint32_t aoo_rva = exp->AddressOfNameOrdinals;
	uint32_t aof_off = 0, aon_off = 0, aoo_off = 0;
	if (!rvaToFileOffset(nt, first, aof_rva, aof_off)) return false;
	if (!rvaToFileOffset(nt, first, aon_rva, aon_off)) return false;
	if (!rvaToFileOffset(nt, first, aoo_rva, aoo_off)) return false;

	const uint32_t *funcRVAs = reinterpret_cast<const uint32_t*>(buf.data() + aof_off);
	const uint32_t *nameRVAs = reinterpret_cast<const uint32_t*>(buf.data() + aon_off);
	const uint16_t *ordinals = reinterpret_cast<const uint16_t*>(buf.data() + aoo_off);

	// Names
	for (uint32_t i = 0; i < exp->NumberOfNames; ++i) {
		uint32_t nameRva = nameRVAs[i];
		uint32_t nameOff = 0; if (!rvaToFileOffset(nt, first, nameRva, nameOff)) continue;
		const char *nm = reinterpret_cast<const char*>(buf.data() + nameOff);
		uint16_t ord = ordinals[i];
		uint32_t fnRva = funcRVAs[ord];
		if (fnRva == 0) continue;
		out.push_back(FunctionStart{ fnRva, nm });
	}

	// Unnamed exports (by ordinal only)
	for (uint32_t i = 0; i < exp->NumberOfFunctions; ++i) {
		uint32_t fnRva = funcRVAs[i];
		if (fnRva == 0) continue;
		// Avoid duplicates with named ones
		bool exists = false;
		for (const auto &f : out) if (f.rva == fnRva) { exists = true; break; }
		if (!exists) out.push_back(FunctionStart{ fnRva, std::string() });
	}

	return true;
}

void scanCallRel32Targets(const std::vector<uint8_t> &buf, const NtHeaders32 &nt, const SectionHeader* first, const SectionHeader* textSection, std::vector<uint32_t> &out) {
	// Scan only the .text section range using correct RVA<->file offset mapping
	uint32_t textRva = textSection->VirtualAddress;
	uint32_t textSize = textSection->Misc.VirtualSize ? textSection->Misc.VirtualSize : textSection->SizeOfRawData;
	uint32_t textFile = textSection->PointerToRawData;
	uint32_t textFileEnd = textFile + textSize;
	if (textFileEnd > buf.size()) textFileEnd = (uint32_t)buf.size();
	for (uint32_t off = textFile; off + 5 <= textFileEnd; ++off) {
		if (buf[off] != 0xE8) continue;
		int32_t rel = *reinterpret_cast<const int32_t*>(&buf[off + 1]);
		uint32_t insnRva = textRva + (off - textFile);
		uint32_t callEndRva = insnRva + 5;
		uint32_t target = (uint32_t)(callEndRva + rel);
		if (target >= textRva && target < textRva + textSize) out.push_back(target);
	}
	std::sort(out.begin(), out.end());
	out.erase(std::unique(out.begin(), out.end()), out.end());
}

bool writeFunctionMapJson(const std::string &outPath, const ImageFunctionMap &map) {
	FILE *f = fopen(outPath.c_str(), "wb");
	if (!f) return false;
	fprintf(f, "{\n");
	fprintf(f, "  \"imageName\": \"%s\",\n", map.imageName.c_str());
	fprintf(f, "  \"imageSize\": %u,\n", map.imageSize);
	fprintf(f, "  \"textRva\": %u,\n", map.text.rva);
	fprintf(f, "  \"textSize\": %u,\n", map.text.size);
	fprintf(f, "  \"functions\": [\n");
	for (size_t i = 0; i < map.functions.size(); ++i) {
		const auto &fn = map.functions[i];
		if (!fn.name.empty())
			fprintf(f, "    { \"rva\": %u, \"name\": \"%s\" }%s\n", fn.rva, fn.name.c_str(), (i + 1 < map.functions.size()) ? "," : "");
		else
			fprintf(f, "    { \"rva\": %u }%s\n", fn.rva, (i + 1 < map.functions.size()) ? "," : "");
	}
	fprintf(f, "  ]\n");
	fprintf(f, "}\n");
	fclose(f);
	return true;
}

void scanPrologueStarts(const std::vector<uint8_t> &buf, const NtHeaders32 &nt, const SectionHeader* textSection, std::vector<uint32_t> &out) {
	uint32_t textRva = textSection->VirtualAddress;
	uint32_t textSize = textSection->Misc.VirtualSize ? textSection->Misc.VirtualSize : textSection->SizeOfRawData;
	uint32_t textFile = textSection->PointerToRawData;
	uint32_t textFileEnd = textFile + textSize;
	if (textFileEnd > buf.size()) textFileEnd = (uint32_t)buf.size();
	for (uint32_t off = textFile; off + 4 <= textFileEnd; ++off) {
		// Look for push ebp; mov ebp, esp => 55 8B EC
		if (buf[off] == 0x55 && off + 2 < textFileEnd && buf[off+1] == 0x8B && buf[off+2] == 0xEC) {
			out.push_back(textRva + (off - textFile));
			continue;
		}
		// Look for push ebp; sub esp, imm8 => 55 83 EC ??
		if (buf[off] == 0x55 && off + 3 < textFileEnd && buf[off+1] == 0x83 && buf[off+2] == 0xEC) {
			out.push_back(textRva + (off - textFile));
			continue;
		}
		// push ebp; mov ebp, esp; sub esp, imm => 55 8B EC 83 EC ??
		if (buf[off] == 0x55 && off + 4 < textFileEnd && buf[off+1] == 0x8B && buf[off+2] == 0xEC && buf[off+3] == 0x83 && buf[off+4] == 0xEC) {
			out.push_back(textRva + (off - textFile));
			continue;
		}
	}
	std::sort(out.begin(), out.end());
	out.erase(std::unique(out.begin(), out.end()), out.end());
}


