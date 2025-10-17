#pragma once

#include <stdint.h>
#include <vector>
#include <string>

// Minimal PE types (32-bit)
struct DosHeader { uint16_t e_magic; uint16_t e_cblp; uint16_t e_cp; uint16_t e_crlc; uint16_t e_cparhdr; uint16_t e_minalloc; uint16_t e_maxalloc; uint16_t e_ss; uint16_t e_sp; uint16_t e_csum; uint16_t e_ip; uint16_t e_cs; uint16_t e_lfarlc; uint16_t e_ovno; uint16_t e_res[4]; uint16_t e_oemid; uint16_t e_oeminfo; uint16_t e_res2[10]; uint32_t e_lfanew; };

struct FileHeader { uint16_t Machine; uint16_t NumberOfSections; uint32_t TimeDateStamp; uint32_t PointerToSymbolTable; uint32_t NumberOfSymbols; uint16_t SizeOfOptionalHeader; uint16_t Characteristics; };

struct DataDirectory { uint32_t VirtualAddress; uint32_t Size; };

struct OptionalHeader32 {
	uint16_t Magic; uint8_t MajorLinkerVersion; uint8_t MinorLinkerVersion; uint32_t SizeOfCode; uint32_t SizeOfInitializedData; uint32_t SizeOfUninitializedData; uint32_t AddressOfEntryPoint; uint32_t BaseOfCode; uint32_t BaseOfData; uint32_t ImageBase; uint32_t SectionAlignment; uint32_t FileAlignment; uint16_t MajorOperatingSystemVersion; uint16_t MinorOperatingSystemVersion; uint16_t MajorImageVersion; uint16_t MinorImageVersion; uint16_t MajorSubsystemVersion; uint16_t MinorSubsystemVersion; uint32_t Win32VersionValue; uint32_t SizeOfImage; uint32_t SizeOfHeaders; uint32_t CheckSum; uint16_t Subsystem; uint16_t DllCharacteristics; uint32_t SizeOfStackReserve; uint32_t SizeOfStackCommit; uint32_t SizeOfHeapReserve; uint32_t SizeOfHeapCommit; uint32_t LoaderFlags; uint32_t NumberOfRvaAndSizes;
	DataDirectory dataDirectory[16];
};

struct NtHeaders32 { uint32_t Signature; FileHeader fileHeader; OptionalHeader32 optionalHeader; };

struct SectionHeader { char Name[8]; union { uint32_t PhysicalAddress; uint32_t VirtualSize; } Misc; uint32_t VirtualAddress; uint32_t SizeOfRawData; uint32_t PointerToRawData; uint32_t PointerToRelocations; uint32_t PointerToLinenumbers; uint16_t NumberOfRelocations; uint16_t NumberOfLinenumbers; uint32_t Characteristics; };

struct ExportDirectory { uint32_t Characteristics; uint32_t TimeDateStamp; uint16_t MajorVersion; uint16_t MinorVersion; uint32_t Name; uint32_t Base; uint32_t NumberOfFunctions; uint32_t NumberOfNames; uint32_t AddressOfFunctions; uint32_t AddressOfNames; uint32_t AddressOfNameOrdinals; };

struct TextSectionInfo { uint32_t rva; uint32_t size; };

struct FunctionStart { uint32_t rva; std::string name; };

struct ImageFunctionMap {
	std::string imageName;
	uint32_t imageSize;
	TextSectionInfo text;
	std::vector<FunctionStart> functions;
};

// File helpers
bool readFile(const std::string &path, std::vector<uint8_t> &out);

// Parsing helpers
bool parseHeaders(const std::vector<uint8_t> &buf, NtHeaders32 &nt, const SectionHeader* &firstSection);
bool findTextSection(const std::vector<uint8_t> &buf, const NtHeaders32 &nt, const SectionHeader* first, TextSectionInfo &out, const SectionHeader* &textSectionOut);
bool enumerateExports(const std::vector<uint8_t> &buf, const NtHeaders32 &nt, const SectionHeader* first, std::vector<FunctionStart> &out);

// Scans .text for call rel32 (E8) targets inside .text using correct RVA/file offset mapping
void scanCallRel32Targets(const std::vector<uint8_t> &buf, const NtHeaders32 &nt, const SectionHeader* first, const SectionHeader* textSection, std::vector<uint32_t> &out);

// Heuristic: scan .text for common function prologues and treat them as starts
void scanPrologueStarts(const std::vector<uint8_t> &buf, const NtHeaders32 &nt, const SectionHeader* textSection, std::vector<uint32_t> &out);

// JSON emit (simple)
bool writeFunctionMapJson(const std::string &outPath, const ImageFunctionMap &map);


